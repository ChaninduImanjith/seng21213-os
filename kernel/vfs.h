#ifndef VFS_H
#define VFS_H

#include "../include/types.h"

/* L12 §3 — Virtual File System abstraction.
 * A filesystem backend exposes its operations through this vtable.
 */
typedef struct file_ops {
    int      (*write)(const char *name, const char *data, uint32_t len);
    int      (*read)(const char *name, char *buf, uint32_t maxlen);
    int      (*unlink)(const char *name);
    uint32_t (*size)(const char *name);

    int      (*list)(int index, char *name_out, uint32_t *size_out);

    int      (*mkdir)(const char *name);
    int      (*chdir)(const char *name);
    int      (*getcwd)(char *buf, uint32_t maxlen);
    int      (*is_dir)(const char *name);
} file_ops_t;

typedef struct vfs_mount {
    const char       *name;
    const file_ops_t *ops;
} vfs_mount_t;

void vfs_init(void);

const char *vfs_backend_name(void);

int      vfs_write(const char *name, const char *data, uint32_t len);
int      vfs_read(const char *name, char *buf, uint32_t maxlen);
int      vfs_unlink(const char *name);
uint32_t vfs_size(const char *name);

int vfs_list(int index, char *name_out, uint32_t *size_out);
int vfs_mkdir(const char *name);
int vfs_chdir(const char *name);
int vfs_getcwd(char *buf, uint32_t maxlen);
int vfs_is_dir(const char *name);

#endif
