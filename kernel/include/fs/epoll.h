/*
 * SzpontOS - Linux-compatible epoll(7) Kernel Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_FS_EPOLL_H
#define SZPONTOS_FS_EPOLL_H

#include <kernel/types.h>
#include <kernel/spinlock.h>
#include <sched/waitqueue.h>
#include <fs/vfs.h>

#define EPOLL_CLOEXEC 0x00080000

#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

#define EPOLLIN        0x0001
#define EPOLLPRI       0x0002
#define EPOLLOUT       0x0004
#define EPOLLERR       0x0008
#define EPOLLHUP       0x0010
#define EPOLLRDNORM    0x0040
#define EPOLLRDBAND    0x0080
#define EPOLLWRNORM    0x0100
#define EPOLLWRBAND    0x0200
#define EPOLLMSG       0x0400
#define EPOLLRDHUP     0x2000
#define EPOLLEXCLUSIVE (1U << 28)
#define EPOLLWAKEUP    (1U << 29)
#define EPOLLONESHOT   (1U << 30)
#define EPOLLET        (1U << 31)

typedef union epoll_data {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;

struct epoll_event {
    uint32_t events;
    epoll_data_t data;
} __attribute__((packed));

typedef struct epoll_entry {
    int fd;
    uint32_t events;
    epoll_data_t data;
    uint32_t ready_mask;
    bool disabled;
    struct epoll_entry *next;
} epoll_entry_t;

typedef struct epoll_instance {
    spinlock_t lock;
    epoll_entry_t *entries;
    size_t num_entries;
    wait_queue_t wq;
} epoll_instance_t;

int64_t sys_epoll_create1(int flags);
int64_t sys_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int64_t sys_epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const void *sigmask, size_t sigsetsize);
int64_t sys_epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

void epoll_on_fd_close(void *process_ptr, int fd);
bool epoll_has_pollin(vfs_node_t *node);

#endif /* SZPONTOS_FS_EPOLL_H */
