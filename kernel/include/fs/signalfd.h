/*
 * SzpontOS - Linux-compatible signalfd(2) Kernel Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_FS_SIGNALFD_H
#define SZPONTOS_FS_SIGNALFD_H

#include <kernel/types.h>
#include <kernel/spinlock.h>
#include <sched/waitqueue.h>
#include <fs/vfs.h>
#include <kernel/signal.h>
#include <uapi/linux/signalfd.h>

typedef struct signalfd_ctx {
    spinlock_t lock;
    sigset_t sigmask;
    uint32_t flags;
    wait_queue_t wq;
} signalfd_ctx_t;

int64_t sys_signalfd(int fd, const sigset_t *mask, size_t sizemask);
int64_t sys_signalfd4(int fd, const sigset_t *mask, size_t sizemask, int flags);

void signalfd_notify(void *proc_ptr, int sig);
bool signalfd_has_pollin(vfs_node_t *node);

#endif /* SZPONTOS_FS_SIGNALFD_H */
