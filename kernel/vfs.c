#include "../include/types.h"
#include "fs.h"
#include "vfs.h"

/* Current Stage 4 RAM-disk filesystem backend. */
static const file_ops_t ramfs_ops = {
    .write   = fs_write,
    .read    = fs_read,
    .unlink  = fs_unlink,
    .size    = fs_size,
    .list    = fs_list,
    .mkdir   = fs_mkdir,
    .chdir   = fs_chdir,
    .getcwd  = fs_getcwd,
    .is_dir  = fs_is_dir
};

static vfs_mount_t root_mount;

void vfs_init(void) {
    root_mount.name = "ramfs";
    root_mount.ops  = &ramfs_ops;
}

const char *vfs_backend_name(void) {
    if (!root_mount.ops) {
        return "none";
    }

    return root_mount.name;
}

int vfs_write(const char *name, const char *data, uint32_t len) {
    if (!root_mount.ops || !root_mount.ops->write) return -1;
    return root_mount.ops->write(name, data, len);
}

int vfs_read(const char *name, char *buf, uint32_t maxlen) {
    if (!root_mount.ops || !root_mount.ops->read) return -1;
    return root_mount.ops->read(name, buf, maxlen);
}

int vfs_unlink(const char *name) {
    if (!root_mount.ops || !root_mount.ops->unlink) return -1;
    return root_mount.ops->unlink(name);
}

uint32_t vfs_size(const char *name) {
    if (!root_mount.ops || !root_mount.ops->size) {
        return (uint32_t)-1;
    }

    return root_mount.ops->size(name);
}

int vfs_list(int index, char *name_out, uint32_t *size_out) {
    if (!root_mount.ops || !root_mount.ops->list) return 0;
    return root_mount.ops->list(index, name_out, size_out);
}

int vfs_mkdir(const char *name) {
    if (!root_mount.ops || !root_mount.ops->mkdir) return -1;
    return root_mount.ops->mkdir(name);
}

int vfs_chdir(const char *name) {
    if (!root_mount.ops || !root_mount.ops->chdir) return -1;
    return root_mount.ops->chdir(name);
}

int vfs_getcwd(char *buf, uint32_t maxlen) {
    if (!root_mount.ops || !root_mount.ops->getcwd) return -1;
    return root_mount.ops->getcwd(buf, maxlen);
}

int vfs_is_dir(const char *name) {
    if (!root_mount.ops || !root_mount.ops->is_dir) return 0;
    return root_mount.ops->is_dir(name);
}
