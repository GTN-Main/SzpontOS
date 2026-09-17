/*
 * SzpontOS - Linux-compatible eventfd(2) Kernel Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_FS_EVENTFD_H
#define SZPONTOS_FS_EVENTFD_H

#include <kernel/types.h>
#include <kernel/spinlock.h>
#include <sched/waitqueue.h>
#include <fs/vfs.h>

#define EFD_SEMAPHORE 0x0001
#define EFD_CLOEXEC   0x00080000
#define EFD_NONBLOCK  0x00000800

#define EVENTFD_VAL_MAX 0xFFFFFFFFFFFFFFFEULL

typedef struct eventfd_ctx {
    spinlock_t lock;
    uint64_t val;
    uint32_t flags;
    wait_queue_t wq_read;
    wait_queue_t wq_write;
} eventfd_ctx_t;

int64_t sys_eventfd2(unsigned int initval, int flags);
bool eventfd_has_pollin(vfs_node_t *node);
bool eventfd_has_pollout(vfs_node_t *node);

#endif /* SZPONTOS_FS_EVENTFD_H */
