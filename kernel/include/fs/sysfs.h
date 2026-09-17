/*
 * SzpontOS - SysFS (System & Device Virtual Filesystem)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_FS_SYSFS_H
#define SZPONTOS_FS_SYSFS_H

#include <fs/vfs.h>
#include <kernel/types.h>

#define SYSFS_MAX_CHILDREN 64

typedef enum {
    SYSFS_NODE_DIR,
    SYSFS_NODE_STATIC,
    SYSFS_NODE_DYNAMIC,
    SYSFS_NODE_SYMLINK
} sysfs_type_t;

typedef size_t (*sysfs_read_func_t)(char *buf, size_t max_len, void *arg);

typedef struct sysfs_entry {
    char name[48];
    vfs_node_t *node;
} sysfs_entry_t;

typedef struct sysfs_data {
    sysfs_type_t type;
    char *static_content;
    size_t static_len;
    sysfs_read_func_t dynamic_fn;
    void *arg;
    char symlink_target[128];
    sysfs_entry_t children[SYSFS_MAX_CHILDREN];
    size_t child_count;
} sysfs_data_t;

vfs_node_t *sysfs_init(void);
vfs_node_t *sysfs_create_dir(vfs_node_t *parent, const char *name);
vfs_node_t *sysfs_create_file(vfs_node_t *parent, const char *name, const char *content);
vfs_node_t *sysfs_create_dynamic_file(vfs_node_t *parent, const char *name, sysfs_read_func_t fn, void *arg);
vfs_node_t *sysfs_create_symlink(vfs_node_t *parent, const char *name, const char *target);

#endif /* SZPONTOS_FS_SYSFS_H */
