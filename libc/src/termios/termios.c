/*
 * SzpontOS - POSIX termios and ioctl implementation
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <termios.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <stdarg.h>
#include <errno.h>

extern int64_t __syscall3(int64_t num, int64_t a1, int64_t a2, int64_t a3);

int ioctl(int fd, unsigned long request, ...) {
    va_list args;
    va_start(args, request);
    void *argp = va_arg(args, void *);
    va_end(args);

    int64_t ret = __syscall3(SYS_ioctl, fd, (int64_t)request, (int64_t)argp);
    return (int)ret;
}

int tcgetattr(int fd, struct termios *termios_p) {
    return ioctl(fd, TCGETS, termios_p);
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    unsigned long req = TCSETS;
    if (optional_actions == TCSADRAIN) req = TCSETSW;
    else if (optional_actions == TCSAFLUSH) req = TCSETSF;
    return ioctl(fd, req, (void *)termios_p);
}

int tcflush(int fd, int queue_selector) {
    (void)fd; (void)queue_selector;
    return 0;
}

int tcflow(int fd, int action) {
    (void)fd; (void)action;
    return 0;
}

speed_t cfgetispeed(const struct termios *termios_p) {
    return termios_p ? termios_p->c_ispeed : B115200;
}

speed_t cfgetospeed(const struct termios *termios_p) {
    return termios_p ? termios_p->c_ospeed : B115200;
}

int cfsetispeed(struct termios *termios_p, speed_t speed) {
    if (!termios_p) return -1;
    termios_p->c_ispeed = speed;
    return 0;
}

int cfsetospeed(struct termios *termios_p, speed_t speed) {
    if (!termios_p) return -1;
    termios_p->c_ospeed = speed;
    return 0;
}
