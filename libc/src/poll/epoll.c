/*
 * SzpontOS Libc - epoll(7) Implementation
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <sys/epoll.h>
#include <sys/syscall.h>
#include <errno.h>

static inline int64_t __check_syscall(int64_t ret) {
    if (ret < 0) {
        errno = (int)-ret;
        return -1;
    }
    return ret;
}

int epoll_create(int size) {
    if (size <= 0) {
        errno = EINVAL;
        return -1;
    }
    return epoll_create1(0);
}

int epoll_create1(int flags) {
    int64_t ret = __syscall1(SYS_epoll_create1, (int64_t)flags);
    return (int)__check_syscall(ret);
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    int64_t ret = __syscall4(SYS_epoll_ctl, (int64_t)epfd, (int64_t)op, (int64_t)fd, (int64_t)event);
    return (int)__check_syscall(ret);
}

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    int64_t ret = __syscall4(SYS_epoll_wait, (int64_t)epfd, (int64_t)events, (int64_t)maxevents, (int64_t)timeout);
    return (int)__check_syscall(ret);
}

int epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask) {
    int64_t ret = __syscall6(SYS_epoll_pwait, (int64_t)epfd, (int64_t)events, (int64_t)maxevents, (int64_t)timeout, (int64_t)sigmask, sizeof(sigset_t));
    return (int)__check_syscall(ret);
}
