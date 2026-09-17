/*
 * SzpontOS - Linux-compatible epoll(7) Kernel Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <fs/epoll.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <mm/usercopy.h>
#include <sched/process.h>
#include <sched/sched.h>
#include <arch/x86_64/pit.h>
#include <drivers/serial.h>
#include <drivers/keyboard.h>
#include <drivers/xhci.h>
#include <drivers/ehci.h>
#include <net/net.h>
#include <kernel/string.h>
#include <kernel/kprint.h>

static vfs_ops_t g_epoll_ops;

static int epoll_vfs_close(vfs_node_t *node) {
    if (!node)
        return 0;
    epoll_instance_t *inst = (epoll_instance_t *)node->device_data;
    if (inst) {
        spinlock_acquire(&inst->lock);
        epoll_entry_t *entry = inst->entries;
        while (entry) {
            epoll_entry_t *next = entry->next;
            kfree(entry);
            entry = next;
        }
        inst->entries = NULL;
        inst->num_entries = 0;
        spinlock_release(&inst->lock);
        wait_queue_wake_all(&inst->wq);
        kfree(inst);
        node->device_data = NULL;
    }
    kfree(node);
    return 0;
}

static void epoll_ops_init(void) {
    static bool init = false;
    if (!init) {
        memset(&g_epoll_ops, 0, sizeof(vfs_ops_t));
        g_epoll_ops.close = epoll_vfs_close;
        init = true;
    }
}

int64_t sys_epoll_create1(int flags) {
    if (flags & ~EPOLL_CLOEXEC)
        return -22; /* -EINVAL */

    process_t *proc = sched_get_current_process();
    if (!proc)
        return -1;

    epoll_ops_init();

    int fd = -1;
    for (int i = 3; i < MAX_FD; i++) {
        if (!proc->fds[i]) {
            fd = i;
            break;
        }
    }
    if (fd == -1)
        return -24; /* -EMFILE */

    epoll_instance_t *inst = (epoll_instance_t *)kzalloc(sizeof(epoll_instance_t));
    if (!inst)
        return -12; /* -ENOMEM */

    inst->lock = SPINLOCK_INIT;
    inst->entries = NULL;
    inst->num_entries = 0;
    wait_queue_init(&inst->wq);

    vfs_node_t *node = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    if (!node) {
        kfree(inst);
        return -12; /* -ENOMEM */
    }

    ksnprintf(node->name, sizeof(node->name), "epoll:[%d]", fd);
    node->flags = VFS_TYPE_CHARDEVICE;
    node->ops = &g_epoll_ops;
    node->device_data = inst;

    file_descriptor_t *fdesc = (file_descriptor_t *)kzalloc(sizeof(file_descriptor_t));
    if (!fdesc) {
        kfree(node);
        kfree(inst);
        return -12; /* -ENOMEM */
    }

    fdesc->node = node;
    fdesc->flags = O_RDWR;
    fdesc->refcount = 1;

    proc->fds[fd] = fdesc;
    if (flags & EPOLL_CLOEXEC)
        proc->fd_cloexec[fd] = true;

    return fd;
}

int64_t sys_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    process_t *proc = sched_get_current_process();
    if (!proc || epfd < 0 || epfd >= MAX_FD || !proc->fds[epfd])
        return -9; /* -EBADF */
    if (fd < 0 || fd >= MAX_FD || !proc->fds[fd])
        return -9; /* -EBADF */
    if (epfd == fd)
        return -22; /* -EINVAL: Can't epoll self */

    file_descriptor_t *epoll_fdesc = proc->fds[epfd];
    if (!epoll_fdesc->node || epoll_fdesc->node->ops != &g_epoll_ops)
        return -22; /* -EINVAL: Not an epoll file descriptor */

    epoll_instance_t *inst = (epoll_instance_t *)epoll_fdesc->node->device_data;
    if (!inst)
        return -22;

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    if (op == EPOLL_CTL_ADD || op == EPOLL_CTL_MOD) {
        if (!event)
            return -14; /* -EFAULT */
        if (!copy_from_user(&ev, (uintptr_t)event, sizeof(struct epoll_event))) {
            if ((uintptr_t)event > USER_ADDR_MAX) {
                memcpy(&ev, event, sizeof(struct epoll_event));
            } else {
                return -14; /* -EFAULT */
            }
        }
    }

    spinlock_acquire(&inst->lock);

    if (op == EPOLL_CTL_ADD) {
        /* Check if fd already in set */
        epoll_entry_t *cur = inst->entries;
        while (cur) {
            if (cur->fd == fd) {
                spinlock_release(&inst->lock);
                return -17; /* -EEXIST */
            }
            cur = cur->next;
        }

        epoll_entry_t *entry = (epoll_entry_t *)kzalloc(sizeof(epoll_entry_t));
        if (!entry) {
            spinlock_release(&inst->lock);
            return -12; /* -ENOMEM */
        }
        entry->fd = fd;
        entry->events = ev.events;
        entry->data = ev.data;
        entry->ready_mask = 0;
        entry->disabled = false;
        entry->next = inst->entries;
        inst->entries = entry;
        inst->num_entries++;

        spinlock_release(&inst->lock);
        return 0;
    } else if (op == EPOLL_CTL_MOD) {
        epoll_entry_t *cur = inst->entries;
        while (cur) {
            if (cur->fd == fd) {
                cur->events = ev.events;
                cur->data = ev.data;
                cur->ready_mask = 0;
                cur->disabled = false;
                spinlock_release(&inst->lock);
                return 0;
            }
            cur = cur->next;
        }
        spinlock_release(&inst->lock);
        return -2; /* -ENOENT */
    } else if (op == EPOLL_CTL_DEL) {
        epoll_entry_t *prev = NULL;
        epoll_entry_t *cur = inst->entries;
        while (cur) {
            if (cur->fd == fd) {
                if (prev)
                    prev->next = cur->next;
                else
                    inst->entries = cur->next;
                kfree(cur);
                inst->num_entries--;
                spinlock_release(&inst->lock);
                return 0;
            }
            prev = cur;
            cur = cur->next;
        }
        spinlock_release(&inst->lock);
        return -2; /* -ENOENT */
    }

    spinlock_release(&inst->lock);
    return -22; /* -EINVAL: Unknown op */
}

void epoll_on_fd_close(void *process_ptr, int fd) {
    process_t *proc = (process_t *)process_ptr;
    if (!proc || fd < 0 || fd >= MAX_FD)
        return;

    for (int i = 0; i < MAX_FD; i++) {
        if (proc->fds[i] && proc->fds[i]->node && proc->fds[i]->node->ops == &g_epoll_ops) {
            epoll_instance_t *inst = (epoll_instance_t *)proc->fds[i]->node->device_data;
            if (inst) {
                spinlock_acquire(&inst->lock);
                epoll_entry_t *prev = NULL;
                epoll_entry_t *cur = inst->entries;
                while (cur) {
                    if (cur->fd == fd) {
                        if (prev)
                            prev->next = cur->next;
                        else
                            inst->entries = cur->next;
                        kfree(cur);
                        inst->num_entries--;
                        break;
                    }
                    prev = cur;
                    cur = cur->next;
                }
                spinlock_release(&inst->lock);
            }
        }
    }
}

int64_t sys_epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const void *sigmask, size_t sigsetsize) {
    if (maxevents <= 0 || maxevents > 4096)
        return -22; /* -EINVAL */
    if (!events)
        return -14; /* -EFAULT */

    process_t *proc = sched_get_current_process();
    if (!proc || epfd < 0 || epfd >= MAX_FD || !proc->fds[epfd])
        return -9; /* -EBADF */

    file_descriptor_t *epoll_fdesc = proc->fds[epfd];
    if (!epoll_fdesc->node || epoll_fdesc->node->ops != &g_epoll_ops)
        return -22; /* -EINVAL */

    epoll_instance_t *inst = (epoll_instance_t *)epoll_fdesc->node->device_data;
    if (!inst)
        return -22;

    (void)sigmask;
    (void)sigsetsize;

    uint64_t start_tick = pit_get_ticks();
    uint64_t timeout_ticks = (timeout > 0) ? ((uint64_t)timeout * pit_get_frequency() + 999) / 1000 : 0;

    struct epoll_event k_events[64];
    int max_batch = (maxevents < 64) ? maxevents : 64;

    netif_poll_all();

    while (1) {
        int ready_count = 0;

        spinlock_acquire(&inst->lock);
        epoll_entry_t *entry = inst->entries;
        while (entry && ready_count < max_batch) {
            int target_fd = entry->fd;
            if (target_fd >= 0 && target_fd < MAX_FD && proc->fds[target_fd]) {
                file_descriptor_t *target_desc = proc->fds[target_fd];
                short status = vfs_poll_node(target_desc, (short)entry->events);

                uint32_t current = 0;
                if ((entry->events & EPOLLIN) && (status & 0x0001 /* POLLIN */))
                    current |= EPOLLIN;
                if ((entry->events & EPOLLOUT) && (status & 0x0004 /* POLLOUT */))
                    current |= EPOLLOUT;
                if ((entry->events & EPOLLPRI) && (status & 0x0002 /* POLLPRI */))
                    current |= EPOLLPRI;
                if (status & 0x0008 /* POLLERR */)
                    current |= EPOLLERR;
                if (status & 0x0010 /* POLLHUP */)
                    current |= EPOLLHUP;
                if ((entry->events & EPOLLRDHUP) && (status & 0x2000 /* POLLRDHUP */))
                    current |= EPOLLRDHUP;

                uint32_t deliverable = current;
                if (entry->disabled) {
                    deliverable = 0;
                }

                if (deliverable != 0) {
                    if (entry->events & EPOLLONESHOT)
                        entry->disabled = true;

                    k_events[ready_count].events = deliverable;
                    k_events[ready_count].data = entry->data;
                    ready_count++;
                }
            }
            entry = entry->next;
        }
        spinlock_release(&inst->lock);

        if (ready_count > 0) {
            size_t copy_bytes = ready_count * sizeof(struct epoll_event);
            if (!copy_to_user((uintptr_t)events, k_events, copy_bytes)) {
                if ((uintptr_t)events > USER_ADDR_MAX) {
                    memcpy(events, k_events, copy_bytes);
                } else {
                    return -14; /* -EFAULT */
                }
            }
            return ready_count;
        }

        if (timeout == 0)
            return 0;

        if (timeout > 0 && (pit_get_ticks() - start_tick) >= timeout_ticks)
            return 0;

        netif_poll_all();
        xhci_poll();
        ehci_poll();
        keyboard_poll_hardware();
        thread_sleep(2);
    }
}

int64_t sys_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    return sys_epoll_pwait(epfd, events, maxevents, timeout, NULL, 0);
}

bool epoll_has_pollin(vfs_node_t *node) {
    if (!node || !node->device_data)
        return false;
    epoll_instance_t *inst = (epoll_instance_t *)node->device_data;
    process_t *proc = sched_get_current_process();
    if (!proc)
        return false;

    spinlock_acquire(&inst->lock);
    epoll_entry_t *entry = inst->entries;
    while (entry) {
        int target_fd = entry->fd;
        if (target_fd >= 0 && target_fd < MAX_FD && proc->fds[target_fd]) {
            short status = vfs_poll_node(proc->fds[target_fd], (short)entry->events);
            if (!entry->disabled && (status & (short)entry->events)) {
                spinlock_release(&inst->lock);
                return true;
            }
        }
        entry = entry->next;
    }
    spinlock_release(&inst->lock);
    return false;
}
