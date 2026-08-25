/*
 * SzpontOS - UNIX98 Pseudo-Terminal (PTY/PTS) Subsystem Implementation
 * Inspired by FreeBSD sys/kern/tty_pts.c
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/pty.h>
#include <fs/devfs.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <sched/sched.h>
#include <sched/process.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/kprint.h>

#define PTY_BUFFER_SIZE 4096

typedef struct pty_pair {
    int index;
    bool allocated;
    bool locked;
    struct winsize ws;

    /* Master -> Slave buffer */
    char m2s_buf[PTY_BUFFER_SIZE];
    size_t m2s_head;
    size_t m2s_tail;
    size_t m2s_count;

    /* Slave -> Master buffer */
    char s2m_buf[PTY_BUFFER_SIZE];
    size_t s2m_head;
    size_t s2m_tail;
    size_t s2m_count;

    spinlock_t lock;
    vfs_node_t *master_node;
    vfs_node_t *slave_node;
} pty_pair_t;

static pty_pair_t g_ptys[MAX_PTS];
static spinlock_t g_pty_global_lock = SPINLOCK_INIT;

static vfs_ops_t g_ptmx_ops;
static vfs_ops_t g_pty_master_ops;
static vfs_ops_t g_pty_slave_ops;

/* =========================================================================
 * Master Node Operations
 * ========================================================================= */

static ssize_t pty_master_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    (void)offset;
    pty_pair_t *pty = (pty_pair_t *)node->device_data;
    if (!pty || !buffer || size == 0)
        return 0;

    char *dst = (char *)buffer;
    size_t read_bytes = 0;

    spinlock_acquire(&pty->lock);
    while (read_bytes < size && pty->s2m_count > 0) {
        dst[read_bytes++] = pty->s2m_buf[pty->s2m_tail];
        pty->s2m_tail = (pty->s2m_tail + 1) % PTY_BUFFER_SIZE;
        pty->s2m_count--;
    }
    spinlock_release(&pty->lock);

    return (ssize_t)read_bytes;
}

static ssize_t pty_master_write(vfs_node_t *node, off_t offset, size_t size, const void *buffer) {
    (void)offset;
    pty_pair_t *pty = (pty_pair_t *)node->device_data;
    if (!pty || !buffer || size == 0)
        return 0;

    const char *src = (const char *)buffer;
    size_t written = 0;

    spinlock_acquire(&pty->lock);
    while (written < size && pty->m2s_count < PTY_BUFFER_SIZE) {
        pty->m2s_buf[pty->m2s_head] = src[written++];
        pty->m2s_head = (pty->m2s_head + 1) % PTY_BUFFER_SIZE;
        pty->m2s_count++;
    }
    spinlock_release(&pty->lock);

    return (ssize_t)written;
}

/* =========================================================================
 * Slave Node Operations
 * ========================================================================= */

static ssize_t pty_slave_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    (void)offset;
    pty_pair_t *pty = (pty_pair_t *)node->device_data;
    if (!pty || !buffer || size == 0)
        return 0;

    char *dst = (char *)buffer;
    size_t read_bytes = 0;

    spinlock_acquire(&pty->lock);
    while (read_bytes < size && pty->m2s_count > 0) {
        dst[read_bytes++] = pty->m2s_buf[pty->m2s_tail];
        pty->m2s_tail = (pty->m2s_tail + 1) % PTY_BUFFER_SIZE;
        pty->m2s_count--;
    }
    spinlock_release(&pty->lock);

    return (ssize_t)read_bytes;
}

static ssize_t pty_slave_write(vfs_node_t *node, off_t offset, size_t size, const void *buffer) {
    (void)offset;
    pty_pair_t *pty = (pty_pair_t *)node->device_data;
    if (!pty || !buffer || size == 0)
        return 0;

    const char *src = (const char *)buffer;
    size_t written = 0;

    spinlock_acquire(&pty->lock);
    while (written < size && pty->s2m_count < PTY_BUFFER_SIZE) {
        pty->s2m_buf[pty->s2m_head] = src[written++];
        pty->s2m_head = (pty->s2m_head + 1) % PTY_BUFFER_SIZE;
        pty->s2m_count++;
    }
    spinlock_release(&pty->lock);

    return (ssize_t)written;
}

/* =========================================================================
 * /dev/ptmx Multiplexer
 * ========================================================================= */

static int ptmx_open(vfs_node_t *node, uint32_t flags) {
    (void)flags;
    spinlock_acquire(&g_pty_global_lock);

    int idx = -1;
    for (int i = 0; i < MAX_PTS; i++) {
        if (!g_ptys[i].allocated) {
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        spinlock_release(&g_pty_global_lock);
        return -1; /* EMFILE */
    }

    pty_pair_t *pty = &g_ptys[idx];
    memset(pty, 0, sizeof(pty_pair_t));
    pty->index = idx;
    pty->allocated = true;
    pty->locked = true;
    pty->ws.ws_col = 80;
    pty->ws.ws_row = 24;
    pty->lock = SPINLOCK_INIT;

    /* Create dynamic master node for this open handle */
    vfs_node_t *master = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    ksnprintf(master->name, sizeof(master->name), "ptmx_%d", idx);
    master->flags = VFS_TYPE_CHARDEVICE;
    master->permissions = 0666;
    master->ops = &g_pty_master_ops;
    master->device_data = pty;
    pty->master_node = master;

    /* Create slave device in /dev/pts/N or /dev/ptsN */
    char slave_name[32];
    ksnprintf(slave_name, sizeof(slave_name), "pts%d", idx);
    vfs_node_t *slave = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    slave->flags = VFS_TYPE_CHARDEVICE;
    slave->permissions = 0666;
    slave->ops = &g_pty_slave_ops;
    slave->device_data = pty;
    devfs_register_device(slave_name, slave);
    pty->slave_node = slave;

    /* Re-bind node ptr so caller's file descriptor gets the dedicated master */
    node->device_data = pty;
    node->ops = &g_pty_master_ops;

    spinlock_release(&g_pty_global_lock);
    return 0;
}

static vfs_ops_t g_ptmx_ops = {.open = ptmx_open};

static vfs_ops_t g_pty_master_ops = {.read = pty_master_read, .write = pty_master_write};

static vfs_ops_t g_pty_slave_ops = {.read = pty_slave_read, .write = pty_slave_write};

void pty_init(void) {
    memset(g_ptys, 0, sizeof(g_ptys));
    vfs_node_t *ptmx = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    ptmx->flags = VFS_TYPE_CHARDEVICE;
    ptmx->permissions = 0666;
    ptmx->ops = &g_ptmx_ops;
    devfs_register_device("ptmx", ptmx);
    klog_info("PTY: UNIX98 pseudo-terminal multiplexer /dev/ptmx registered");
}
