/*
 * SzpontOS Libc - timerfd(2) Implementation
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <sys/timerfd.h>
#include <sys/syscall.h>
#include <errno.h>

static inline int64_t __check_syscall(int64_t ret) {
    if (ret < 0) {
        errno = (int)-ret;
        return -1;
    }
    return ret;
}

int timerfd_create(int clockid, int flags) {
    int64_t ret = __syscall2(SYS_timerfd_create, (int64_t)clockid, (int64_t)flags);
    return (int)__check_syscall(ret);
}

int timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value) {
    int64_t ret = __syscall4(SYS_timerfd_settime, (int64_t)fd, (int64_t)flags, (int64_t)new_value, (int64_t)old_value);
    return (int)__check_syscall(ret);
}

int timerfd_gettime(int fd, struct itimerspec *curr_value) {
    int64_t ret = __syscall2(SYS_timerfd_gettime, (int64_t)fd, (int64_t)curr_value);
    return (int)__check_syscall(ret);
}
