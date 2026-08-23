/*
 * SzpontOS - UNIX98 Pseudo-Terminal (PTY/PTS) Subsystem
 * Inspired by FreeBSD sys/kern/tty_pts.c
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_PTY_H
#define SZPONTOS_DRIVERS_PTY_H

#include <fs/vfs.h>
#include <kernel/types.h>

#define MAX_PTS 32

/* POSIX / Linux PTY ioctls */
#ifndef TIOCGPTN
#define TIOCGPTN    0x80045430
#endif
#ifndef TIOCSPTLCK
#define TIOCSPTLCK  0x40045431
#endif
#ifndef TIOCGWINSZ
#define TIOCGWINSZ  0x5413
#endif
#ifndef TIOCSWINSZ
#define TIOCSWINSZ  0x5414
#endif

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

void pty_init(void);

#endif /* SZPONTOS_DRIVERS_PTY_H */
