#ifndef SZPONTOS_FS_DEVFS_H
#define SZPONTOS_FS_DEVFS_H

#include <fs/vfs.h>
#include <drivers/block.h>

void devfs_init(void);
int devfs_register_device(const char *name, vfs_node_t *node);
int devfs_unregister_device(const char *name);
int devfs_register_block_device(block_device_t *bdev);

#endif /* SZPONTOS_FS_DEVFS_H */
