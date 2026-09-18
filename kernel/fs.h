#ifndef FS_H
#define FS_H

#include "../include/types.h"

#define FS_MAGIC          0x21213F5
#define FS_MAX_INODES     64
#define FS_MAX_DIRENTS    64
#define FS_DIRECT_BLOCKS   8
#define FS_INDIRECT_PTRS   1024
#define FS_MAX_FILE_BLOCKS (FS_DIRECT_BLOCKS + FS_INDIRECT_PTRS)
#define FS_MAX_FILENAME    28
#define FS_MAX_FILE_SIZE   (FS_MAX_FILE_BLOCKS * 4096U) /* ~4 MB addressing */

/* Fixed block numbers -- laid out exactly as the design diagram:
 * 0 = superblock, 1 = directory, 2 = block bitmap, 3 = inode bitmap,
 * 4 = inode table, 5.. = data blocks. */
#define FS_SUPERBLOCK_NUM    0
#define FS_DIR_BLOCK_NUM     1
#define FS_BLOCK_BITMAP_NUM  2
#define FS_INODE_BITMAP_NUM  3
#define FS_INODE_TABLE_NUM   4
#define FS_DATA_START_NUM    5

void fs_init(void);

int fs_write(const char *name, const char *data, uint32_t len); /* create-or-truncate; returns bytes written or -1 */
int fs_read(const char *name, char *buf, uint32_t maxlen);      /* returns bytes read or -1 if not found */
int fs_unlink(const char *name);                                 /* returns 0 or -1 if not found */
uint32_t fs_size(const char *name);                               /* returns file size, or (uint32_t)-1 if not found */

/* Iterate directory entries for `ls`: call with index 0,1,2,... until
 * it returns 0 (no more entries). Fills *name_out and *size_out. */
int fs_list(int index, char *name_out, uint32_t *size_out);

#endif
