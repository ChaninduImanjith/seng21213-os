#include "../include/types.h"
#include "ramdisk.h"
#include "fs.h"

typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t total_inodes;
    uint32_t block_size;
} __attribute__((packed)) fs_superblock_t;

/* Directory entries all live in block 1.
 *
 * `parent` turns this single metadata table into a hierarchical namespace:
 *
 *     parent inode 0:  docs -> inode 1
 *     parent inode 1:  note -> inode 2
 *
 * Therefore different directories may contain the same file name.
 */
typedef struct {
    char    name[FS_MAX_FILENAME];
    int32_t inode;             /* -1 = unused slot */
    int32_t parent;            /* inode number of containing directory */
} __attribute__((packed)) fs_dirent_t;

/* Names still live in directory entries, not inodes.
 *
 * A directory inode does not require its own data block in this teaching
 * filesystem; its children are the dirents whose `parent` field points to it.
 */
typedef struct {
    uint32_t size;
    uint32_t blocks[FS_DIRECT_BLOCKS];

    /* L12 §2 -- single-indirect block. */
    uint32_t indirect_block;

    /* Number of DATA blocks, excluding the indirect table itself. */
    uint32_t block_count;

    /* L12 §3 -- hierarchical filesystem metadata. */
    uint32_t type;             /* FS_TYPE_FILE or FS_TYPE_DIR */
    int32_t  parent;           /* parent directory inode */
} __attribute__((packed)) fs_inode_t;

/* Catch accidental metadata-layout growth at compile time. */
typedef char fs_dir_table_must_fit[
    (sizeof(fs_dirent_t) * FS_MAX_DIRENTS <= RD_BLOCK_SIZE) ? 1 : -1
];

typedef char fs_inode_table_must_fit[
    (sizeof(fs_inode_t) * FS_MAX_INODES <= RD_BLOCK_SIZE) ? 1 : -1
];

static int current_dir = FS_ROOT_INODE;


/* --------------------------------------------------------------------------
 * Small string helpers -- no libc in a freestanding kernel.
 * -------------------------------------------------------------------------- */

static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static void str_copy_n(char *dst, const char *src, int n) {
    int i = 0;

    while (src[i] && i < n - 1) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}

/* Directory/file names are one path component in this small filesystem.
 * '/' is handled only by fs_chdir("/") for the root directory. */
static int valid_component(const char *name) {
    int i;

    if (!name || !name[0]) return 0;

    if (str_eq(name, ".") || str_eq(name, "..")) return 0;

    for (i = 0; name[i]; i++) {
        if (name[i] == '/') return 0;
        if (i >= FS_MAX_FILENAME - 1) return 0;
    }

    return 1;
}


/* --------------------------------------------------------------------------
 * Metadata-table access.
 * -------------------------------------------------------------------------- */

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


/* --------------------------------------------------------------------------
 * Directory lookup.
 * -------------------------------------------------------------------------- */

/* Search a particular directory. */
static int find_dirent_in(int parent, const char *name) {
    fs_dirent_t *dir = dir_table();
    int i;

    for (i = 0; i < FS_MAX_DIRENTS; i++) {
        if (dir[i].inode >= 0 &&
            dir[i].parent == parent &&
            str_eq(dir[i].name, name)) {
            return i;
        }
    }

    return -1;
}

/* Normal file APIs resolve names relative to the current working directory. */
static int find_dirent(const char *name) {
    return find_dirent_in(current_dir, name);
}

static int find_free_dirent(void) {
    fs_dirent_t *dir = dir_table();
    int i;

    for (i = 0; i < FS_MAX_DIRENTS; i++) {
        if (dir[i].inode < 0) return i;
    }

    return -1;
}

/* Every non-root inode has exactly one directory entry naming it. */
static const char *find_name_for_inode(int inum) {
    fs_dirent_t *dir = dir_table();
    int i;

    for (i = 0; i < FS_MAX_DIRENTS; i++) {
        if (dir[i].inode == inum) {
            return dir[i].name;
        }
    }

    return 0;
}


/* --------------------------------------------------------------------------
 * Inode / block allocation.
 * -------------------------------------------------------------------------- */

static int find_free_inode(void) {
    uint8_t *ibmap = inode_bitmap();
    int i;

    /* inode 0 is permanently reserved for /. */
    for (i = 1; i < FS_MAX_INODES; i++) {
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

static void zero_block(uint32_t block_num) {
    uint8_t *p = (uint8_t *)ramdisk_block_ptr(block_num);
    uint32_t i;

    for (i = 0; i < RD_BLOCK_SIZE; i++) {
        p[i] = 0;
    }
}

static void inode_init(fs_inode_t *inode, uint32_t type, int32_t parent) {
    uint32_t i;

    inode->size = 0;
    inode->indirect_block = 0;
    inode->block_count = 0;
    inode->type = type;
    inode->parent = parent;

    for (i = 0; i < FS_DIRECT_BLOCKS; i++) {
        inode->blocks[i] = 0;
    }
}


/* --------------------------------------------------------------------------
 * Direct + single-indirect block mapping.
 * -------------------------------------------------------------------------- */

static uint32_t inode_get_block(fs_inode_t *inode, uint32_t index) {
    if (index < FS_DIRECT_BLOCKS) {
        return inode->blocks[index];
    }

    index -= FS_DIRECT_BLOCKS;

    if (index >= FS_INDIRECT_PTRS || inode->indirect_block == 0) {
        return 0;
    }

    {
        uint32_t *table =
            (uint32_t *)ramdisk_block_ptr(inode->indirect_block);

        return table[index];
    }
}

static int inode_set_block(fs_inode_t *inode,
                           uint32_t index,
                           uint32_t block_num) {
    if (index < FS_DIRECT_BLOCKS) {
        inode->blocks[index] = block_num;
        return 0;
    }

    index -= FS_DIRECT_BLOCKS;

    if (index >= FS_INDIRECT_PTRS) {
        return -1;
    }

    if (inode->indirect_block == 0) {
        int table_block = find_free_block();

        if (table_block < 0) {
            return -1;
        }

        block_bitmap()[table_block] = 1;
        inode->indirect_block = (uint32_t)table_block;
        zero_block((uint32_t)table_block);
    }

    {
        uint32_t *table =
            (uint32_t *)ramdisk_block_ptr(inode->indirect_block);

        table[index] = block_num;
    }

    return 0;
}

static void inode_release_blocks(fs_inode_t *inode) {
    uint8_t *bbmap = block_bitmap();
    uint32_t i;

    for (i = 0; i < inode->block_count; i++) {
        uint32_t blk = inode_get_block(inode, i);

        if (blk >= FS_DATA_START_NUM && blk < RD_TOTAL_BLOCKS) {
            bbmap[blk] = 0;
        }
    }

    if (inode->indirect_block >= FS_DATA_START_NUM &&
        inode->indirect_block < RD_TOTAL_BLOCKS) {
        bbmap[inode->indirect_block] = 0;
    }

    for (i = 0; i < FS_DIRECT_BLOCKS; i++) {
        inode->blocks[i] = 0;
    }

    inode->indirect_block = 0;
    inode->block_count = 0;
    inode->size = 0;
}


/* --------------------------------------------------------------------------
 * Filesystem initialisation.
 * -------------------------------------------------------------------------- */

void fs_init(void) {
    int i;
    ramdisk_init();

    {
        fs_superblock_t *sb =
            (fs_superblock_t *)ramdisk_block_ptr(FS_SUPERBLOCK_NUM);

        sb->magic        = FS_MAGIC;
        sb->total_blocks = RD_TOTAL_BLOCKS;
        sb->total_inodes = FS_MAX_INODES;
        sb->block_size   = RD_BLOCK_SIZE;
    }

    {
        fs_dirent_t *dir = dir_table();

        for (i = 0; i < FS_MAX_DIRENTS; i++) {
            dir[i].name[0] = 0;
            dir[i].inode = -1;
            dir[i].parent = -1;
        }
    }

    {
        uint8_t *ibmap = inode_bitmap();

        for (i = 0; i < FS_MAX_INODES; i++) {
            ibmap[i] = 0;
        }
    }

    {
        fs_inode_t *inodes = inode_table();

        for (i = 0; i < FS_MAX_INODES; i++) {
            inode_init(&inodes[i], FS_TYPE_UNUSED, -1);
        }
    }

    /* Blocks 0..4 are filesystem metadata. */
    {
        uint8_t *bbmap = block_bitmap();

        for (i = 0; i < RD_TOTAL_BLOCKS; i++) {
            bbmap[i] = (i < FS_DATA_START_NUM) ? 1 : 0;
        }
    }

    /* inode 0 represents the virtual root directory /. */
    inode_bitmap()[FS_ROOT_INODE] = 1;
    inode_init(&inode_table()[FS_ROOT_INODE],
               FS_TYPE_DIR,
               FS_ROOT_INODE);

    current_dir = FS_ROOT_INODE;
}


/* --------------------------------------------------------------------------
 * Regular-file API.
 * -------------------------------------------------------------------------- */

int fs_write(const char *name, const char *data, uint32_t len) {
    int dslot;
    int inum;
    fs_dirent_t *dir;
    fs_inode_t *inode;
    uint8_t *bbmap;
    uint32_t written;

    if (!valid_component(name)) return -1;

    dslot = find_dirent(name);
    dir = dir_table();

    if (dslot < 0) {
        dslot = find_free_dirent();
        if (dslot < 0) return -1;

        inum = find_free_inode();
        if (inum < 0) return -1;

        inode_bitmap()[inum] = 1;

        str_copy_n(dir[dslot].name, name, FS_MAX_FILENAME);
        dir[dslot].inode = inum;
        dir[dslot].parent = current_dir;

        inode_init(&inode_table()[inum],
                   FS_TYPE_FILE,
                   current_dir);
    } else {
        inum = dir[dslot].inode;

        /* Never truncate a directory through the file API. */
        if (inode_table()[inum].type != FS_TYPE_FILE) {
            return -1;
        }
    }

    inode = &inode_table()[inum];
    bbmap = block_bitmap();

    /* Create-or-truncate semantics. */
    inode_release_blocks(inode);

    if (len > FS_MAX_FILE_SIZE) {
        len = FS_MAX_FILE_SIZE;
    }

    written = 0;

    while (written < len) {
        int blk = find_free_block();

        if (blk < 0) break;

        bbmap[blk] = 1;

        if (inode_set_block(inode,
                            inode->block_count,
                            (uint32_t)blk) < 0) {
            bbmap[blk] = 0;
            break;
        }

        {
            uint8_t *dst =
                (uint8_t *)ramdisk_block_ptr((uint32_t)blk);

            uint32_t chunk = len - written;
            uint32_t j;

            if (chunk > RD_BLOCK_SIZE) {
                chunk = RD_BLOCK_SIZE;
            }

            for (j = 0; j < chunk; j++) {
                dst[j] = (uint8_t)data[written + j];
            }

            inode->block_count++;
            written += chunk;
        }
    }

    inode->size = written;
    return (int)written;
}

int fs_read(const char *name, char *buf, uint32_t maxlen) {
    int dslot = find_dirent(name);
    fs_dirent_t *dir;
    fs_inode_t *inode;
    uint32_t total;
    uint32_t copied;
    uint32_t i;

    if (dslot < 0) return -1;

    dir = dir_table();
    inode = &inode_table()[dir[dslot].inode];

    if (inode->type != FS_TYPE_FILE) {
        return -1;
    }

    total = inode->size;
    if (total > maxlen) total = maxlen;

    copied = 0;

    for (i = 0; i < inode->block_count && copied < total; i++) {
        uint32_t blk = inode_get_block(inode, i);
        uint8_t *src;
        uint32_t chunk;
        uint32_t j;

        if (blk == 0) break;

        src = (uint8_t *)ramdisk_block_ptr(blk);

        chunk = total - copied;
        if (chunk > RD_BLOCK_SIZE) {
            chunk = RD_BLOCK_SIZE;
        }

        for (j = 0; j < chunk; j++) {
            buf[copied + j] = (char)src[j];
        }

        copied += chunk;
    }

    return (int)copied;
}

int fs_unlink(const char *name) {
    int dslot = find_dirent(name);
    fs_dirent_t *dir;
    int inum;
    fs_inode_t *inode;

    if (dslot < 0) return -1;

    dir = dir_table();
    inum = dir[dslot].inode;
    inode = &inode_table()[inum];

    /* rm is for regular files. Directories are deliberately protected. */
    if (inode->type != FS_TYPE_FILE) {
        return -1;
    }

    inode_release_blocks(inode);
    inode->type = FS_TYPE_UNUSED;
    inode->parent = -1;

    inode_bitmap()[inum] = 0;

    dir[dslot].name[0] = 0;
    dir[dslot].inode = -1;
    dir[dslot].parent = -1;

    return 0;
}

uint32_t fs_size(const char *name) {
    int dslot = find_dirent(name);

    if (dslot < 0) {
        return (uint32_t)-1;
    }

    return inode_table()[dir_table()[dslot].inode].size;
}

int fs_list(int index, char *name_out, uint32_t *size_out) {
    fs_dirent_t *dir = dir_table();
    int seen = 0;
    int i;

    for (i = 0; i < FS_MAX_DIRENTS; i++) {
        int inum;

        if (dir[i].inode < 0) continue;
        if (dir[i].parent != current_dir) continue;

        if (seen == index) {
            inum = dir[i].inode;

            str_copy_n(name_out,
                       dir[i].name,
                       FS_MAX_FILENAME);

            *size_out = inode_table()[inum].size;
            return 1;
        }

        seen++;
    }

    return 0;
}


/* --------------------------------------------------------------------------
 * L12 §3 -- hierarchical directory extension.
 * -------------------------------------------------------------------------- */

int fs_mkdir(const char *name) {
    int dslot;
    int inum;
    fs_dirent_t *dir;

    if (!valid_component(name)) {
        return -1;
    }

    /* File and directory names share the same namespace. */
    if (find_dirent(name) >= 0) {
        return -1;
    }

    dslot = find_free_dirent();
    if (dslot < 0) return -1;

    inum = find_free_inode();
    if (inum < 0) return -1;

    dir = dir_table();

    inode_bitmap()[inum] = 1;
    inode_init(&inode_table()[inum],
               FS_TYPE_DIR,
               current_dir);

    str_copy_n(dir[dslot].name,
               name,
               FS_MAX_FILENAME);

    dir[dslot].inode = inum;
    dir[dslot].parent = current_dir;

    return 0;
}

int fs_chdir(const char *name) {
    int dslot;
    int inum;

    if (!name || !name[0]) {
        return -1;
    }

    if (str_eq(name, "/")) {
        current_dir = FS_ROOT_INODE;
        return 0;
    }

    if (str_eq(name, ".")) {
        return 0;
    }

    if (str_eq(name, "..")) {
        if (current_dir != FS_ROOT_INODE) {
            current_dir = inode_table()[current_dir].parent;
        }

        return 0;
    }

    dslot = find_dirent(name);
    if (dslot < 0) return -1;

    inum = dir_table()[dslot].inode;

    if (inode_table()[inum].type != FS_TYPE_DIR) {
        return -1;
    }

    current_dir = inum;
    return 0;
}

int fs_is_dir(const char *name) {
    int dslot = find_dirent(name);

    if (dslot < 0) return 0;

    return inode_table()[dir_table()[dslot].inode].type == FS_TYPE_DIR;
}

int fs_getcwd(char *buf, uint32_t maxlen) {
    int chain[FS_MAX_INODES];
    int depth = 0;
    int cur = current_dir;
    uint32_t pos = 0;
    int i;

    if (!buf || maxlen < 2) {
        return -1;
    }

    /* Walk toward / and remember the inode chain. */
    while (cur != FS_ROOT_INODE) {
        int parent;

        if (depth >= FS_MAX_INODES) {
            return -1;
        }

        if (cur < 0 || cur >= FS_MAX_INODES) {
            return -1;
        }

        chain[depth++] = cur;

        parent = inode_table()[cur].parent;

        if (parent < 0 || parent >= FS_MAX_INODES) {
            return -1;
        }

        cur = parent;
    }

    buf[pos++] = '/';

    /* Reverse the chain to produce /parent/child/... */
    for (i = depth - 1; i >= 0; i--) {
        const char *name = find_name_for_inode(chain[i]);
        int j = 0;

        if (!name) return -1;

        if (pos > 1) {
            if (pos + 1 >= maxlen) return -1;
            buf[pos++] = '/';
        }

        while (name[j]) {
            if (pos + 1 >= maxlen) {
                return -1;
            }

            buf[pos++] = name[j++];
        }
    }

    buf[pos] = 0;
    return 0;
}
