/*
 * SzpontOS - Linux-compatible timerfd(2) Kernel Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_FS_TIMERFD_H
#define SZPONTOS_FS_TIMERFD_H

#include <kernel/types.h>
#include <kernel/spinlock.h>
#include <sched/waitqueue.h>
#include <fs/vfs.h>
#include <drivers/rtc.h>

#define TFD_TIMER_ABSTIME       (1 << 0)
#define TFD_TIMER_CANCEL_ON_SET (1 << 1)
#define TFD_CLOEXEC             0x80000
#define TFD_NONBLOCK            0x800

struct itimerspec_kernel {
    struct timespec_kernel it_interval;
    struct timespec_kernel it_value;
};

typedef struct timerfd_ctx {
    spinlock_t lock;
    int clockid;
    uint32_t flags;
    bool armed;
    uint64_t target_ns;   /* Target expiration timestamp in ns */
    uint64_t interval_ns; /* Periodic interval in ns */
    uint64_t expirations; /* Count of expirations since last read */
    wait_queue_t wq;
    struct timerfd_ctx *next;
} timerfd_ctx_t;

int64_t sys_timerfd_create(int clockid, int flags);
int64_t sys_timerfd_settime(int fd, int flags, const struct itimerspec_kernel *new_value, struct itimerspec_kernel *old_value);
int64_t sys_timerfd_gettime(int fd, struct itimerspec_kernel *curr_value);

void timerfd_tick(void);
bool timerfd_has_pollin(vfs_node_t *node);

#endif /* SZPONTOS_FS_TIMERFD_H */
