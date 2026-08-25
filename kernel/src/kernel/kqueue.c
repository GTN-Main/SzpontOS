/*
 * SzpontOS - BSD kqueue/kevent Kernel Implementation
 * Inspired by FreeBSD sys/kern/kern_event.c
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <kernel/kqueue.h>
#include <sched/process.h>
#include <sched/sched.h>
#include <mm/heap.h>
#include <fs/vfs.h>
#include <net/socket.h>
#include <drivers/keyboard.h>
#include <drivers/serial.h>
#include <arch/x86_64/pit.h>
#include <kernel/string.h>
#include <kernel/kprint.h>

typedef struct pipe_chan_kq {
    char data[4096];
    size_t head;
    size_t tail;
    size_t count;
    int readers;
    int writers;
} pipe_chan_kq_t;

static vfs_ops_t g_kqueue_ops;

static int kqueue_vfs_close(vfs_node_t *node) {
    if (!node)
        return 0;
    kqueue_t *kq = (kqueue_t *)node->device_data;
    if (kq) {
        kfree(kq);
    }
    kfree(node);
    return 0;
}

static void kqueue_ops_init(void) {
    static bool init = false;
    if (!init) {
        memset(&g_kqueue_ops, 0, sizeof(vfs_ops_t));
        g_kqueue_ops.close = kqueue_vfs_close;
        init = true;
    }
}

int sys_kqueue(void) {
    process_t *proc = sched_get_current_process();
    if (!proc)
        return -1;

    kqueue_ops_init();

    /* Find free fd */
    int fd = -1;
    for (int i = 3; i < MAX_FD; i++) {
        if (!proc->fds[i]) {
            fd = i;
            break;
        }
    }
    if (fd == -1)
        return -1;

    kqueue_t *kq = (kqueue_t *)kzalloc(sizeof(kqueue_t));
    if (!kq)
        return -1;

    kq->lock = SPINLOCK_INIT;

    vfs_node_t *node = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    if (!node) {
        kfree(kq);
        return -1;
    }

    ksnprintf(node->name, sizeof(node->name), "kqueue:[%d]", fd);
    node->flags = VFS_TYPE_CHARDEVICE;
    node->ops = &g_kqueue_ops;
    node->device_data = kq;
    kq->vfs_node = node;

    file_descriptor_t *fdesc = (file_descriptor_t *)kzalloc(sizeof(file_descriptor_t));
    if (!fdesc) {
        kfree(node);
        kfree(kq);
        return -1;
    }

    fdesc->node = node;
    fdesc->flags = O_RDWR;
    fdesc->refcount = 1;

    proc->fds[fd] = fdesc;
    return fd;
}

static kqueue_t *get_kqueue_from_fd(int fd) {
    process_t *proc = sched_get_current_process();
    if (!proc || fd < 0 || fd >= MAX_FD || !proc->fds[fd])
        return NULL;
    file_descriptor_t *fdesc = proc->fds[fd];
    if (!fdesc->node || fdesc->node->ops != &g_kqueue_ops)
        return NULL;
    return (kqueue_t *)fdesc->node->device_data;
}

static bool check_event_ready(process_t *proc, kqueue_entry_t *entry) {
    if (!entry || !entry->active)
        return false;

    struct kevent *ev = &entry->ev;

    switch (ev->filter) {
    case EVFILT_READ: {
        int fd = (int)ev->ident;
        if (fd < 0 || fd >= MAX_FD || !proc->fds[fd])
            return false;
        file_descriptor_t *fdesc = proc->fds[fd];

        if (fd == 0) {
            if (keyboard_has_char() || serial_received()) {
                ev->data = 1;
                return true;
            }
            return false;
        }

        if (fdesc->node && fdesc->node->flags == VFS_TYPE_SOCKET) {
            socket_t *sock = (socket_t *)fdesc->node->device_data;
            if (sock && sock->rx_len > 0) {
                ev->data = (int64_t)sock->rx_len;
                return true;
            }
            return false;
        }

        if (fdesc->node && fdesc->node->flags == VFS_TYPE_PIPE) {
            pipe_chan_kq_t *p = (pipe_chan_kq_t *)fdesc->node->device_data;
            if (p && p->count > 0) {
                ev->data = (int64_t)p->count;
                return true;
            }
            return false;
        }

        /* Regular files or devices */
        ev->data = 1;
        return true;
    }

    case EVFILT_WRITE: {
        int fd = (int)ev->ident;
        if (fd < 0 || fd >= MAX_FD || !proc->fds[fd])
            return false;
        ev->data = 4096;
        return true;
    }

    case EVFILT_TIMER: {
        uint64_t now_ticks = (uint64_t)pit_get_ticks();
        if (now_ticks >= entry->target_tick) {
            uint64_t interval_ms = (uint64_t)ev->data;
            uint64_t interval_ticks = (interval_ms > 0) ? (interval_ms / 10) : 1;
            if (interval_ticks == 0)
                interval_ticks = 1;
            entry->target_tick = now_ticks + interval_ticks;
            ev->data = 1; /* Number of timer expirations */
            return true;
        }
        return false;
    }

    case EVFILT_SIGNAL: {
        int sig = (int)ev->ident;
        if (sig > 0 && sig < 32 && (proc->pending_signals & (1U << sig))) {
            ev->data = 1;
            return true;
        }
        return false;
    }

    case EVFILT_USER: {
        if (ev->fflags & 0x0001) { /* Triggered */
            return true;
        }
        return false;
    }

    default:
        return false;
    }
}

int sys_kevent(int kq_fd, const struct kevent *changelist, int nchanges, struct kevent *eventlist, int nevents,
               const struct timespec_kernel *timeout) {
    kqueue_t *kq = get_kqueue_from_fd(kq_fd);
    if (!kq)
        return -1;

    process_t *proc = sched_get_current_process();
    if (!proc)
        return -1;

    spinlock_acquire(&kq->lock);

    /* 1. Process changelist */
    if (changelist && nchanges > 0) {
        for (int i = 0; i < nchanges; i++) {
            const struct kevent *ch = &changelist[i];

            if (ch->flags & EV_DELETE) {
                for (size_t j = 0; j < kq->count; j++) {
                    if (kq->entries[j].active && kq->entries[j].ev.ident == ch->ident &&
                        kq->entries[j].ev.filter == ch->filter) {
                        kq->entries[j].active = false;
                        break;
                    }
                }
            } else if (ch->flags & EV_ADD) {
                kqueue_entry_t *slot = NULL;
                for (size_t j = 0; j < kq->count; j++) {
                    if (kq->entries[j].active && kq->entries[j].ev.ident == ch->ident &&
                        kq->entries[j].ev.filter == ch->filter) {
                        slot = &kq->entries[j];
                        break;
                    }
                }

                if (!slot) {
                    for (size_t j = 0; j < kq->count; j++) {
                        if (!kq->entries[j].active) {
                            slot = &kq->entries[j];
                            break;
                        }
                    }
                }

                if (!slot && kq->count < MAX_KQ_EVENTS) {
                    slot = &kq->entries[kq->count++];
                }

                if (slot) {
                    slot->ev = *ch;
                    slot->active = !(ch->flags & EV_DISABLE);
                    if (ch->filter == EVFILT_TIMER) {
                        uint64_t ms = (uint64_t)ch->data;
                        uint64_t ticks = (ms / 10 > 0) ? (ms / 10) : 1;
                        slot->target_tick = (uint64_t)pit_get_ticks() + ticks;
                    }
                }
            } else if (ch->flags & EV_ENABLE) {
                for (size_t j = 0; j < kq->count; j++) {
                    if (kq->entries[j].ev.ident == ch->ident && kq->entries[j].ev.filter == ch->filter) {
                        kq->entries[j].active = true;
                        break;
                    }
                }
            } else if (ch->flags & EV_DISABLE) {
                for (size_t j = 0; j < kq->count; j++) {
                    if (kq->entries[j].ev.ident == ch->ident && kq->entries[j].ev.filter == ch->filter) {
                        kq->entries[j].active = false;
                        break;
                    }
                }
            }
        }
    }

    /* 2. Poll for triggered events */
    int ready_events = 0;
    uint64_t timeout_ticks = 0;
    bool has_timeout = false;

    if (timeout) {
        has_timeout = true;
        uint64_t total_ms = (uint64_t)timeout->tv_sec * 1000 + (uint64_t)(timeout->tv_nsec / 1000000);
        timeout_ticks = (uint64_t)pit_get_ticks() + (total_ms / 10);
    }

    while (ready_events == 0 && eventlist && nevents > 0) {
        extern void e1000_poll(void);
        e1000_poll();

        for (size_t i = 0; i < kq->count; i++) {
            if (kq->entries[i].active && check_event_ready(proc, &kq->entries[i])) {
                eventlist[ready_events] = kq->entries[i].ev;
                ready_events++;

                if (kq->entries[i].ev.flags & EV_ONESHOT) {
                    kq->entries[i].active = false;
                }

                if (ready_events >= nevents)
                    break;
            }
        }

        if (ready_events > 0)
            break;
        if (has_timeout && (uint64_t)pit_get_ticks() >= timeout_ticks)
            break;

        spinlock_release(&kq->lock);
        thread_sleep(10);
        spinlock_acquire(&kq->lock);
    }

    spinlock_release(&kq->lock);
    return ready_events;
}
