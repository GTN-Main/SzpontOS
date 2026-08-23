/*
 * SzpontOS Libc - POSIX poll() implementation
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <poll.h>
#include <sys/syscall.h>
#include <errno.h>

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    int64_t ret = __syscall3(SYS_poll, (int64_t)fds, (int64_t)nfds, (int64_t)timeout);
    if (ret < 0) {
        errno = (int)-ret;
        return -1;
    }
    return (int)ret;
}
