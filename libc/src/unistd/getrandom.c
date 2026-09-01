/*
 * SzpontOS Libc - getrandom and getentropy implementation
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <sys/random.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

ssize_t getrandom(void *buf, size_t buflen, unsigned int flags) {
    if (!buf && buflen > 0) {
        errno = EFAULT;
        return -1;
    }
    int64_t ret = __syscall3(SYS_getrandom, (int64_t)buf, buflen, flags);
    if (ret < 0) {
        errno = (int)-ret;
        return -1;
    }
    return (ssize_t)ret;
}

int getentropy(void *buffer, size_t length) {
    if (length > 256) {
        errno = EIO;
        return -1;
    }
    if (!buffer && length > 0) {
        errno = EFAULT;
        return -1;
    }
    size_t total = 0;
    while (total < length) {
        ssize_t n = getrandom((char *)buffer + total, length - total, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            errno = EIO;
            return -1;
        }
        if (n == 0) {
            errno = EIO;
            return -1;
        }
        total += (size_t)n;
    }
    return 0;
}
