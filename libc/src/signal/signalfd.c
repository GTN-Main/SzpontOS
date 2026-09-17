/*
 * SzpontOS Libc - signalfd(2) Implementation
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <sys/signalfd.h>
#include <sys/syscall.h>
#include <errno.h>

static inline int64_t __check_syscall(int64_t ret) {
    if (ret < 0) {
        errno = (int)-ret;
        return -1;
    }
    return ret;
}

int signalfd(int fd, const sigset_t *mask, int flags) {
    return signalfd4(fd, mask, sizeof(sigset_t), flags);
}

int signalfd4(int fd, const sigset_t *mask, size_t sizemask, int flags) {
    int64_t ret = __syscall4(SYS_signalfd4, (int64_t)fd, (int64_t)mask, (int64_t)sizemask, (int64_t)flags);
    return (int)__check_syscall(ret);
}
