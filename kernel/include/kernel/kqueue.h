/*
 * SzpontOS - BSD kqueue/kevent Event Notification Subsystem
 * Inspired by FreeBSD sys/sys/event.h and sys/kern/kern_event.c
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_KERNEL_KQUEUE_H
#define SZPONTOS_KERNEL_KQUEUE_H

#include <kernel/types.h>
#include <kernel/spinlock.h>
#include <drivers/rtc.h>
#include <fs/vfs.h>

/* Filter types */
#define EVFILT_READ     (-1)
#define EVFILT_WRITE    (-2)
#define EVFILT_AIO      (-3)
#define EVFILT_VNODE    (-4)
#define EVFILT_PROC     (-5)
#define EVFILT_SIGNAL   (-6)
#define EVFILT_TIMER    (-7)
#define EVFILT_USER     (-11)

/* Actions / flags */
#define EV_ADD          0x0001
#define EV_DELETE       0x0002
#define EV_ENABLE       0x0004
#define EV_DISABLE      0x0008
#define EV_ONESHOT      0x0010
#define EV_CLEAR        0x0020
#define EV_RECEIPT      0x0040
#define EV_DISPATCH     0x0080

/* Return flags */
#define EV_ERROR        0x4000
#define EV_EOF          0x8000

/* Standard struct kevent */
struct kevent {
    uintptr_t ident;        /* Identifier (fd, signal, pid, timer id) */
    short     filter;       /* Filter type (EVFILT_*) */
    uint16_t  flags;        /* Action flags (EV_*) */
    uint32_t  fflags;       /* Filter-specific flags */
    int64_t   data;         /* Filter data (bytes available, timer ms, etc.) */
    void     *udata;        /* User opaque data pointer */
};

#define EV_SET(kevp, a, b, c, d, e, f) do { \
    (kevp)->ident = (uintptr_t)(a);         \
    (kevp)->filter = (short)(b);            \
    (kevp)->flags = (uint16_t)(c);          \
    (kevp)->fflags = (uint32_t)(d);         \
    (kevp)->data = (int64_t)(e);            \
    (kevp)->udata = (void *)(f);            \
} while (0)

#define MAX_KQ_EVENTS 64

typedef struct kqueue_entry {
    struct kevent ev;
    bool active;
    uint64_t target_tick; /* For EVFILT_TIMER */
} kqueue_entry_t;

typedef struct kqueue {
    kqueue_entry_t entries[MAX_KQ_EVENTS];
    size_t count;
    spinlock_t lock;
    vfs_node_t *vfs_node;
} kqueue_t;

int sys_kqueue(void);
int sys_kevent(int kq, const struct kevent *changelist, int nchanges,
               struct kevent *eventlist, int nevents, const struct timespec_kernel *timeout);

#endif /* SZPONTOS_KERNEL_KQUEUE_H */
