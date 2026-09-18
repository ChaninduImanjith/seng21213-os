#include "../include/types.h"
#include "ramdisk.h"
#include "fs.h"
#include "journal.h"

#define JOURNAL_MAGIC 0x4A4E4C31U   /* "JNL1" */

typedef struct {
    uint32_t magic;
    uint32_t state;
    uint32_t sequence;
    uint32_t count;

    uint32_t target[FS_JOURNAL_DATA_BLOCKS];
    uint32_t checksum[FS_JOURNAL_DATA_BLOCKS];
} __attribute__((packed)) journal_header_t;

/* Metadata blocks protected by the redo journal. */
static const uint32_t metadata_blocks[FS_JOURNAL_DATA_BLOCKS] = {
    FS_DIR_BLOCK_NUM,
    FS_BLOCK_BITMAP_NUM,
    FS_INODE_BITMAP_NUM,
    FS_INODE_TABLE_NUM
};

/* In-memory transaction shadow.
 * Filesystem metadata is modified here first, never directly in the
 * home blocks while a transaction is active.
 */
static uint8_t shadow[FS_JOURNAL_DATA_BLOCKS][RD_BLOCK_SIZE];

static int tx_active = 0;
static int defer_next_commit = 0;

static journal_header_t *journal_header(void) {
    return (journal_header_t *)
        ramdisk_block_ptr(FS_JOURNAL_HEADER_NUM);
}

static void copy_bytes(void *dst_ptr,
                       const void *src_ptr,
                       uint32_t count) {
    uint8_t *dst = (uint8_t *)dst_ptr;
    const uint8_t *src = (const uint8_t *)src_ptr;
    uint32_t i;

    for (i = 0; i < count; i++) {
        dst[i] = src[i];
    }
}

static uint32_t checksum_block(const void *ptr) {
    const uint8_t *p = (const uint8_t *)ptr;
    uint32_t hash = 2166136261U;
    uint32_t i;

    for (i = 0; i < RD_BLOCK_SIZE; i++) {
        hash ^= p[i];
        hash *= 16777619U;
    }

    return hash;
}

static void mark_clean(void) {
    journal_header_t *h = journal_header();

    h->magic = JOURNAL_MAGIC;
    h->state = JOURNAL_STATE_CLEAN;
    h->count = 0;
}

void journal_init(void) {
    journal_header_t *h = journal_header();

    tx_active = 0;
    defer_next_commit = 0;

    if (h->magic != JOURNAL_MAGIC) {
        h->magic = JOURNAL_MAGIC;
        h->state = JOURNAL_STATE_CLEAN;
        h->sequence = 0;
        h->count = 0;
        return;
    }

    /* If a committed transaction exists, replay it.
     * A merely prepared transaction was never committed and is discarded.
     */
    journal_recover();
}

void *journal_metadata_ptr(uint32_t block_num) {
    uint32_t i;

    if (tx_active) {
        for (i = 0; i < FS_JOURNAL_DATA_BLOCKS; i++) {
            if (metadata_blocks[i] == block_num) {
                return shadow[i];
            }
        }
    }

    return ramdisk_block_ptr(block_num);
}

int journal_recover(void) {
    journal_header_t *h = journal_header();
    uint32_t i;

    if (h->magic != JOURNAL_MAGIC) {
        return -1;
    }

    if (h->state == JOURNAL_STATE_CLEAN) {
        return 0;
    }

    /* No commit record => transaction is incomplete. */
    if (h->state == JOURNAL_STATE_PREPARED) {
        mark_clean();
        return 0;
    }

    if (h->state != JOURNAL_STATE_COMMITTED ||
        h->count != FS_JOURNAL_DATA_BLOCKS) {
        mark_clean();
        return -1;
    }

    /* Validate the entire committed redo log before touching home blocks. */
    for (i = 0; i < FS_JOURNAL_DATA_BLOCKS; i++) {
        void *logged =
            ramdisk_block_ptr(FS_JOURNAL_DATA_NUM + i);

        if (h->target[i] != metadata_blocks[i]) {
            mark_clean();
            return -1;
        }

        if (checksum_block(logged) != h->checksum[i]) {
            mark_clean();
            return -1;
        }
    }

    /* REDO: copy committed metadata images to their home locations. */
    for (i = 0; i < FS_JOURNAL_DATA_BLOCKS; i++) {
        void *logged =
            ramdisk_block_ptr(FS_JOURNAL_DATA_NUM + i);

        void *home =
            ramdisk_block_ptr(h->target[i]);

        copy_bytes(home, logged, RD_BLOCK_SIZE);
    }

    mark_clean();
    return 1;
}

int journal_begin(void) {
    journal_header_t *h = journal_header();
    uint32_t i;

    if (tx_active) {
        return -1;
    }

    /* Resolve a previous interrupted transaction first. */
    if (h->magic == JOURNAL_MAGIC &&
        h->state != JOURNAL_STATE_CLEAN) {
        if (journal_recover() < 0) {
            return -1;
        }
    }

    for (i = 0; i < FS_JOURNAL_DATA_BLOCKS; i++) {
        void *home =
            ramdisk_block_ptr(metadata_blocks[i]);

        copy_bytes(shadow[i], home, RD_BLOCK_SIZE);
    }

    tx_active = 1;
    return 0;
}

int journal_commit(void) {
    journal_header_t *h = journal_header();
    uint32_t i;

    if (!tx_active) {
        return -1;
    }

    /*
     * WRITE-AHEAD ORDER:
     *
     * 1. copy new metadata images into journal payload
     * 2. write PREPARED header
     * 3. write COMMITTED marker
     * 4. checkpoint metadata into home blocks
     * 5. clear journal
     */

    for (i = 0; i < FS_JOURNAL_DATA_BLOCKS; i++) {
        void *logged =
            ramdisk_block_ptr(FS_JOURNAL_DATA_NUM + i);

        copy_bytes(logged, shadow[i], RD_BLOCK_SIZE);

        h->target[i] = metadata_blocks[i];
        h->checksum[i] = checksum_block(logged);
    }

    h->magic = JOURNAL_MAGIC;
    h->count = FS_JOURNAL_DATA_BLOCKS;
    h->sequence++;

    h->state = JOURNAL_STATE_PREPARED;

    /* Commit record is written only after all redo payloads are complete. */
    h->state = JOURNAL_STATE_COMMITTED;

    tx_active = 0;

    if (defer_next_commit) {
        /* Simulate a crash after commit but before checkpoint. */
        defer_next_commit = 0;
        return 0;
    }

    return journal_recover() < 0 ? -1 : 0;
}

void journal_abort(void) {
    tx_active = 0;
    defer_next_commit = 0;
}

uint32_t journal_state(void) {
    journal_header_t *h = journal_header();

    if (h->magic != JOURNAL_MAGIC) {
        return JOURNAL_STATE_CLEAN;
    }

    return h->state;
}

uint32_t journal_sequence(void) {
    journal_header_t *h = journal_header();

    if (h->magic != JOURNAL_MAGIC) {
        return 0;
    }

    return h->sequence;
}

void journal_test_defer_next_commit(void) {
    defer_next_commit = 1;
}
