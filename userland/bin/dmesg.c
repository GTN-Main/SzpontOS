/*
 * dmesg - print or control the kernel ring buffer
 * Inspired by FreeBSD sbin/dmesg/
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    int fd = open("/proc/dmesg", O_RDONLY);
    if (fd < 0) {
        fd = open("/proc/kmsg", O_RDONLY);
    }

    if (fd >= 0) {
        char buf[2048];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            printf("%s", buf);
        }
        close(fd);
        return 0;
    }

    /* Fallback to syslog syscall */
    char buf[65536];
    long ret = (long)__syscall3(SYS_syslog, 2, (int64_t)buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = '\0';
        printf("%s", buf);
        return 0;
    }

    fprintf(stderr, "dmesg: cannot read kernel log buffer\n");
    return 1;
}
