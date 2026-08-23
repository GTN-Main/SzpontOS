/*
 * SzpontOS - In-Memory RAM Filesystem (TmpFS)
 * Inspired by FreeBSD sys/fs/tmpfs/
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_FS_TMPFS_H
#define SZPONTOS_FS_TMPFS_H

#include <fs/vfs.h>

vfs_node_t *tmpfs_create_fs(const char *name, mode_t mode, uid_t uid, gid_t gid);
void tmpfs_init(void);

#endif /* SZPONTOS_FS_TMPFS_H */
