/*
 * SzpontOS - Linux-compatible eventfd(2) Kernel Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <fs/eventfd.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <mm/usercopy.h>
#include <sched/process.h>
#include <sched/sched.h>
#include <kernel/string.h>
#include <kernel/kprint.h>

static vfs_ops_t g_eventfd_ops;

static ssize_t eventfd_vfs_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    (void)offset;
    if (!node || !node->device_data || !buffer)
        return -14; /* -EFAULT */
    if (size < sizeof(uint64_t))
        return -22; /* -EINVAL */

    eventfd_ctx_t *ctx = (eventfd_ctx_t *)node->device_data;
    uint64_t return_val = 0;

    while (1) {
        spinlock_acquire(&ctx->lock);
        if (ctx->val > 0) {
            if (ctx->flags & EFD_SEMAPHORE) {
                return_val = 1;
                ctx->val -= 1;
            } else {
                return_val = ctx->val;
                ctx->val = 0;
            }
            spinlock_release(&ctx->lock);
            wait_queue_wake_all(&ctx->wq_write);
            break;
        }

        /* Value is 0 */
        if (ctx->flags & EFD_NONBLOCK) {
            spinlock_release(&ctx->lock);
            return -11; /* -EAGAIN */
        }

        /* Check thread/process nonblock */
        process_t *proc = sched_get_current_process();
        if (proc) {
            /* Check if any open fd for this node has O_NONBLOCK */
            for (int i = 0; i < MAX_FD; i++) {
                if (proc->fds[i] && proc->fds[i]->node == node) {
                    if (proc->fds[i]->flags & 0x0800 /* O_NONBLOCK */) {
                        spinlock_release(&ctx->lock);
                        return -11; /* -EAGAIN */
                    }
                    break;
                }
            }
        }

        spinlock_release(&ctx->lock);
        wait_queue_wait(&ctx->wq_read);
    }

    if (!copy_to_user((uintptr_t)buffer, &return_val, sizeof(uint64_t))) {
        /* If kernel buffer */
        if ((uintptr_t)buffer > USER_ADDR_MAX) {
            memcpy(buffer, &return_val, sizeof(uint64_t));
        } else {
            return -14; /* -EFAULT */
        }
    }

    return sizeof(uint64_t);
}

static ssize_t eventfd_vfs_write(vfs_node_t *node, off_t offset, size_t size, const void *buffer) {
    (void)offset;
    if (!node || !node->device_data || !buffer)
        return -14; /* -EFAULT */
    if (size < sizeof(uint64_t))
        return -22; /* -EINVAL */

    uint64_t add_val = 0;
    if (!copy_from_user(&add_val, (uintptr_t)buffer, sizeof(uint64_t))) {
        if ((uintptr_t)buffer > USER_ADDR_MAX) {
            memcpy(&add_val, buffer, sizeof(uint64_t));
        } else {
            return -14; /* -EFAULT */
        }
    }

    if (add_val == 0xFFFFFFFFFFFFFFFFULL)
        return -22; /* -EINVAL: 0xffffffffffffffff is reserved */

    eventfd_ctx_t *ctx = (eventfd_ctx_t *)node->device_data;

    while (1) {
        spinlock_acquire(&ctx->lock);
        if (EVENTFD_VAL_MAX - ctx->val >= add_val) {
            ctx->val += add_val;
            spinlock_release(&ctx->lock);
            wait_queue_wake_all(&ctx->wq_read);
            break;
        }

        /* Overflow */
        if (ctx->flags & EFD_NONBLOCK) {
            spinlock_release(&ctx->lock);
            return -11; /* -EAGAIN */
        }

        process_t *proc = sched_get_current_process();
        if (proc) {
            for (int i = 0; i < MAX_FD; i++) {
                if (proc->fds[i] && proc->fds[i]->node == node) {
                    if (proc->fds[i]->flags & 0x0800 /* O_NONBLOCK */) {
                        spinlock_release(&ctx->lock);
                        return -11; /* -EAGAIN */
                    }
                    break;
                }
            }
        }

        spinlock_release(&ctx->lock);
        wait_queue_wait(&ctx->wq_write);
    }

    return sizeof(uint64_t);
}

static int eventfd_vfs_close(vfs_node_t *node) {
    if (!node)
        return 0;
    eventfd_ctx_t *ctx = (eventfd_ctx_t *)node->device_data;
    if (ctx) {
        wait_queue_wake_all(&ctx->wq_read);
        wait_queue_wake_all(&ctx->wq_write);
        kfree(ctx);
        node->device_data = NULL;
    }
    kfree(node);
    return 0;
}

static void eventfd_ops_init(void) {
    static bool init = false;
    if (!init) {
        memset(&g_eventfd_ops, 0, sizeof(vfs_ops_t));
        g_eventfd_ops.read = eventfd_vfs_read;
        g_eventfd_ops.write = eventfd_vfs_write;
        g_eventfd_ops.close = eventfd_vfs_close;
        init = true;
    }
}

bool eventfd_has_pollin(vfs_node_t *node) {
    if (!node || !node->device_data)
        return false;
    eventfd_ctx_t *ctx = (eventfd_ctx_t *)node->device_data;
    spinlock_acquire(&ctx->lock);
    bool ready = (ctx->val > 0);
    spinlock_release(&ctx->lock);
    return ready;
}

bool eventfd_has_pollout(vfs_node_t *node) {
    if (!node || !node->device_data)
        return false;
    eventfd_ctx_t *ctx = (eventfd_ctx_t *)node->device_data;
    spinlock_acquire(&ctx->lock);
    bool ready = (EVENTFD_VAL_MAX - ctx->val > 0);
    spinlock_release(&ctx->lock);
    return ready;
}

int64_t sys_eventfd2(unsigned int initval, int flags) {
    /* Validate flags: only EFD_SEMAPHORE, EFD_CLOEXEC, EFD_NONBLOCK allowed */
    if (flags & ~(EFD_SEMAPHORE | EFD_CLOEXEC | EFD_NONBLOCK))
        return -22; /* -EINVAL */

    process_t *proc = sched_get_current_process();
    if (!proc)
        return -1;

    eventfd_ops_init();

    int fd = -1;
    for (int i = 3; i < MAX_FD; i++) {
        if (!proc->fds[i]) {
            fd = i;
            break;
        }
    }
    if (fd == -1)
        return -24; /* -EMFILE */

    eventfd_ctx_t *ctx = (eventfd_ctx_t *)kzalloc(sizeof(eventfd_ctx_t));
    if (!ctx)
        return -12; /* -ENOMEM */

    ctx->lock = SPINLOCK_INIT;
    ctx->val = (uint64_t)initval;
    ctx->flags = (uint32_t)flags;
    wait_queue_init(&ctx->wq_read);
    wait_queue_init(&ctx->wq_write);

    vfs_node_t *node = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    if (!node) {
        kfree(ctx);
        return -12; /* -ENOMEM */
    }

    ksnprintf(node->name, sizeof(node->name), "eventfd:[%d]", fd);
    node->flags = VFS_TYPE_CHARDEVICE;
    node->ops = &g_eventfd_ops;
    node->device_data = ctx;

    file_descriptor_t *fdesc = (file_descriptor_t *)kzalloc(sizeof(file_descriptor_t));
    if (!fdesc) {
        kfree(node);
        kfree(ctx);
        return -12; /* -ENOMEM */
    }

    fdesc->node = node;
    fdesc->flags = O_RDWR;
    if (flags & EFD_NONBLOCK)
        fdesc->flags |= 0x0800; /* O_NONBLOCK */
    fdesc->refcount = 1;

    proc->fds[fd] = fdesc;
    if (flags & EFD_CLOEXEC)
        proc->fd_cloexec[fd] = 1;

    return fd;
}
