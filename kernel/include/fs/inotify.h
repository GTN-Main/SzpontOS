/*
 * SzpontOS - Linux-compatible inotify(7) Kernel Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_FS_INOTIFY_H
#define SZPONTOS_FS_INOTIFY_H

#include <kernel/types.h>
#include <kernel/spinlock.h>
#include <sched/waitqueue.h>
#include <fs/vfs.h>

#define IN_ACCESS        0x00000001
#define IN_MODIFY        0x00000002
#define IN_ATTRIB        0x00000004
#define IN_CLOSE_WRITE   0x00000008
#define IN_CLOSE_NOWRITE 0x00000010
#define IN_CLOSE         (IN_CLOSE_WRITE | IN_CLOSE_NOWRITE)
#define IN_OPEN          0x00000020
#define IN_MOVED_FROM    0x00000040
#define IN_MOVED_TO      0x00000080
#define IN_MOVE          (IN_MOVED_FROM | IN_MOVED_TO)
#define IN_CREATE        0x00000100
#define IN_DELETE        0x00000200
#define IN_DELETE_SELF   0x00000400
#define IN_MOVE_SELF     0x00000800

#define IN_UNMOUNT       0x00002000
#define IN_Q_OVERFLOW    0x00004000
#define IN_IGNORED       0x00008000

#define IN_ONLYDIR       0x01000000
#define IN_DONT_FOLLOW   0x02000000
#define IN_EXCL_UNLINK   0x04000000
#define IN_MASK_CREATE   0x10000000
#define IN_MASK_ADD      0x20000000
#define IN_ISDIR         0x40000000
#define IN_ONESHOT       0x80000000

#define IN_CLOEXEC       0x00080000
#define IN_NONBLOCK      0x00000800

struct inotify_event {
    int      wd;
    uint32_t mask;
    uint32_t cookie;
    uint32_t len;
    char     name[];
};

typedef struct inotify_watch {
    int wd;
    char path[256];
    uint32_t mask;
    vfs_node_t *node;
    struct inotify_watch *next;
} inotify_watch_t;

typedef struct inotify_event_node {
    int wd;
    uint32_t mask;
    uint32_t cookie;
    uint32_t len;
    char *name;
    struct inotify_event_node *next;
} inotify_event_node_t;

typedef struct inotify_instance {
    spinlock_t lock;
    inotify_watch_t *watches;
    int next_wd;
    inotify_event_node_t *events_head;
    inotify_event_node_t *events_tail;
    size_t event_count;
    wait_queue_t wq;
    uint32_t flags;
    struct inotify_instance *global_next;
} inotify_instance_t;

int64_t sys_inotify_init1(int flags);
int64_t sys_inotify_init(void);
int64_t sys_inotify_add_watch(int fd, const char *pathname, uint32_t mask);
int64_t sys_inotify_rm_watch(int fd, int wd);

void inotify_emit(const char *path, const char *name, uint32_t mask, uint32_t cookie);
bool inotify_has_pollin(vfs_node_t *node);

#endif /* SZPONTOS_FS_INOTIFY_H */
