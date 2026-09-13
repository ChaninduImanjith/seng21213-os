#include "../include/types.h"
#include "ramdisk.h"
#include "fs.h"

typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t total_inodes;
    uint32_t block_size;
} __attribute__((packed)) fs_superblock_t;

typedef struct {
    char    name[FS_MAX_FILENAME];
    int32_t inode;             /* -1 = empty directory slot */
} __attribute__((packed)) fs_dirent_t;

/* No name here on purpose -- the NAME lives in the directory entry,
 * not the inode. That separation is what makes hard links possible
 * in a real filesystem, even though we don't use that here. */
typedef struct {
    uint32_t size;
    uint32_t blocks[FS_DIRECT_BLOCKS];
    uint32_t block_count;
} __attribute__((packed)) fs_inode_t;

static int str_eq(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

static void str_copy_n(char *dst, const char *src, int n) {
    int i = 0;
    while (src[i] && i < n - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static fs_dirent_t *dir_table(void) {
    return (fs_dirent_t *)ramdisk_block_ptr(FS_DIR_BLOCK_NUM);
}

static fs_inode_t *inode_table(void) {
    return (fs_inode_t *)ramdisk_block_ptr(FS_INODE_TABLE_NUM);
}

static uint8_t *block_bitmap(void) {
    return (uint8_t *)ramdisk_block_ptr(FS_BLOCK_BITMAP_NUM);
}

static uint8_t *inode_bitmap(void) {
    return (uint8_t *)ramdisk_block_ptr(FS_INODE_BITMAP_NUM);
}

static int find_dirent(const char *name) {
    fs_dirent_t *dir = dir_table();
    int i;
    for (i = 0; i < FS_MAX_DIRENTS; i++) {
        if (dir[i].inode >= 0 && str_eq(dir[i].name, name)) return i;
    }
    return -1;
}

static int find_free_dirent(void) {
    fs_dirent_t *dir = dir_table();
    int i;
    for (i = 0; i < FS_MAX_DIRENTS; i++) {
        if (dir[i].inode < 0) return i;
    }
    return -1;
}

static int find_free_inode(void) {
    uint8_t *ibmap = inode_bitmap();
    int i;
    for (i = 0; i < FS_MAX_INODES; i++) {
        if (!ibmap[i]) return i;
    }
    return -1;
}

static int find_free_block(void) {
    uint8_t *bbmap = block_bitmap();
    int i;
    for (i = FS_DATA_START_NUM; i < RD_TOTAL_BLOCKS; i++) {
        if (!bbmap[i]) return i;
    }
    return -1;
}

void fs_init(void) {
    int i;
    ramdisk_init();

    fs_superblock_t *sb = (fs_superblock_t *)ramdisk_block_ptr(FS_SUPERBLOCK_NUM);
    sb->magic        = FS_MAGIC;
    sb->total_blocks = RD_TOTAL_BLOCKS;
    sb->total_inodes = FS_MAX_INODES;
    sb->block_size   = RD_BLOCK_SIZE;

    fs_dirent_t *dir = dir_table();
    for (i = 0; i < FS_MAX_DIRENTS; i++) dir[i].inode = -1;

    uint8_t *ibmap = inode_bitmap();
    for (i = 0; i < FS_MAX_INODES; i++) ibmap[i] = 0;

    /* Blocks 0..FS_DATA_START_NUM-1 are metadata -- permanently "used"
     * so a file's data can never overwrite the superblock/directory/etc. */
    uint8_t *bbmap = block_bitmap();
    for (i = 0; i < RD_TOTAL_BLOCKS; i++) {
        bbmap[i] = (i < FS_DATA_START_NUM) ? 1 : 0;
    }
}

int fs_write(const char *name, const char *data, uint32_t len) {
    int dslot = find_dirent(name);
    int inum;
    fs_dirent_t *dir = dir_table();

    if (dslot < 0) {
        dslot = find_free_dirent();
        if (dslot < 0) return -1;              /* directory full */
        inum = find_free_inode();
        if (inum < 0) return -1;               /* out of inodes */

        inode_bitmap()[inum] = 1;
        str_copy_n(dir[dslot].name, name, FS_MAX_FILENAME);
        dir[dslot].inode = inum;

        fs_inode_t *inode = &inode_table()[inum];
        inode->size = 0;
        inode->block_count = 0;
    } else {
        inum = dir[dslot].inode;
    }

    fs_inode_t *inode = &inode_table()[inum];
    uint8_t *bbmap = block_bitmap();

    /* Truncate: free whatever data blocks this file already had. */
    uint32_t i;
    for (i = 0; i < inode->block_count; i++) bbmap[inode->blocks[i]] = 0;
    inode->block_count = 0;
    inode->size = 0;

    if (len > FS_MAX_FILE_SIZE) len = FS_MAX_FILE_SIZE;

    uint32_t written = 0;
    while (written < len) {
        int blk = find_free_block();
        if (blk < 0) break;                    /* disk full */
        bbmap[blk] = 1;
        inode->blocks[inode->block_count++] = (uint32_t)blk;

        uint8_t *dst = (uint8_t *)ramdisk_block_ptr((uint32_t)blk);
        uint32_t chunk = len - written;
        if (chunk > RD_BLOCK_SIZE) chunk = RD_BLOCK_SIZE;
        uint32_t j;
        for (j = 0; j < chunk; j++) dst[j] = (uint8_t)data[written + j];
        written += chunk;
    }

    inode->size = written;
    return (int)written;
}

int fs_read(const char *name, char *buf, uint32_t maxlen) {
    int dslot = find_dirent(name);
    if (dslot < 0) return -1;

    fs_dirent_t *dir = dir_table();
    fs_inode_t  *inode = &inode_table()[dir[dslot].inode];

    uint32_t total = inode->size;
    if (total > maxlen) total = maxlen;

    uint32_t copied = 0, i;
    for (i = 0; i < inode->block_count && copied < total; i++) {
        uint8_t *src = (uint8_t *)ramdisk_block_ptr(inode->blocks[i]);
        uint32_t chunk = total - copied;
        if (chunk > RD_BLOCK_SIZE) chunk = RD_BLOCK_SIZE;
        uint32_t j;
        for (j = 0; j < chunk; j++) buf[copied + j] = (char)src[j];
        copied += chunk;
    }
    return (int)copied;
}

int fs_unlink(const char *name) {
    int dslot = find_dirent(name);
    if (dslot < 0) return -1;

    fs_dirent_t *dir = dir_table();
    int inum = dir[dslot].inode;
    fs_inode_t *inode = &inode_table()[inum];
    uint8_t *bbmap = block_bitmap();

    uint32_t i;
    for (i = 0; i < inode->block_count; i++) bbmap[inode->blocks[i]] = 0;
    inode->block_count = 0;
    inode->size = 0;

    inode_bitmap()[inum] = 0;
    dir[dslot].inode = -1;
    return 0;
}

uint32_t fs_size(const char *name) {
    int dslot = find_dirent(name);
    if (dslot < 0) return (uint32_t)-1;
    fs_dirent_t *dir = dir_table();
    return inode_table()[dir[dslot].inode].size;
}

int fs_list(int index, char *name_out, uint32_t *size_out) {
    fs_dirent_t *dir = dir_table();
    int seen = 0, i;
    for (i = 0; i < FS_MAX_DIRENTS; i++) {
        if (dir[i].inode < 0) continue;
        if (seen == index) {
            str_copy_n(name_out, dir[i].name, FS_MAX_FILENAME);
            *size_out = inode_table()[dir[i].inode].size;
            return 1;
        }
        seen++;
    }
    return 0;
}
