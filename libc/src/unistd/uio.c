/*
 * SzpontOS - POSIX readv / writev implementation
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    if (iovcnt < 0 || iovcnt > 1024 || !iov) {
        errno = EINVAL;
        return -1;
    }

    ssize_t total_read = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (!iov[i].iov_base || iov[i].iov_len == 0)
            continue;

        ssize_t n = read(fd, iov[i].iov_base, iov[i].iov_len);
        if (n < 0) {
            if (total_read > 0)
                return total_read;
            return -1;
        }
        total_read += n;
        if ((size_t)n < iov[i].iov_len)
            break;
    }

    return total_read;
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    if (iovcnt < 0 || iovcnt > 1024 || !iov) {
        errno = EINVAL;
        return -1;
    }

    ssize_t total_written = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (!iov[i].iov_base || iov[i].iov_len == 0)
            continue;

        ssize_t n = write(fd, iov[i].iov_base, iov[i].iov_len);
        if (n < 0) {
            if (total_written > 0)
                return total_written;
            return -1;
        }
        total_written += n;
        if ((size_t)n < iov[i].iov_len)
            break;
    }

    return total_written;
}
