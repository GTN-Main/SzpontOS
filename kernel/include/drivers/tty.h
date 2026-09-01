/*
 * SzpontOS - UNIX TTY & Line Discipline Driver
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_TTY_H
#define SZPONTOS_DRIVERS_TTY_H

#include <kernel/types.h>
#include <stdbool.h>

/* Termios Flags (POSIX / Libc Compliant) */
#define TTY_IFLAG_IGNBRK 0000001
#define TTY_IFLAG_BRKINT 0000002
#define TTY_IFLAG_IGNPAR 0000004
#define TTY_IFLAG_PARMRK 0000010
#define TTY_IFLAG_INPCK 0000020
#define TTY_IFLAG_ISTRIP 0000040
#define TTY_IFLAG_INLCR 0000100
#define TTY_IFLAG_IGNCR 0000200
#define TTY_IFLAG_ICRNL 0000400
#define TTY_IFLAG_IXON 0002000
#define TTY_IFLAG_IXOFF 0010000

#define TTY_OFLAG_OPOST 0000001
#define TTY_OFLAG_ONLCR 0000004
#define TTY_OFLAG_OCRNL 0000010
#define TTY_OFLAG_ONOCR 0000020
#define TTY_OFLAG_ONLRET 0000040

#define TTY_LFLAG_ISIG 0000001
#define TTY_LFLAG_ICANON 0000002
#define TTY_LFLAG_ECHO 0000010
#define TTY_LFLAG_ECHOE 0000020
#define TTY_LFLAG_ECHOK 0000040
#define TTY_LFLAG_ECHONL 0000100
#define TTY_LFLAG_NOFLSH 0000200
#define TTY_LFLAG_TOSTOP 0000400
#define TTY_LFLAG_IEXTEN 0100000

#define TTY_NCCS 32
#define TTY_VINTR 0
#define TTY_VQUIT 1
#define TTY_VERASE 2
#define TTY_VKILL 3
#define TTY_VEOF 4
#define TTY_VTIME 5
#define TTY_VMIN 6
#define TTY_VSUSP 10

typedef struct termios {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag;
    uint8_t c_line;
    uint8_t c_cc[TTY_NCCS];
    uint32_t c_ispeed;
    uint32_t c_ospeed;
} termios_t;

#ifndef _STRUCT_WINSIZE_DEFINED
#define _STRUCT_WINSIZE_DEFINED
typedef struct winsize {
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
} winsize_t;
#endif

void tty_init(void);
ssize_t tty_read(void *buffer, size_t count);
ssize_t tty_write(const void *buffer, size_t count);
int tty_ioctl(uint64_t request, void *arg);
bool tty_has_input(void);

#endif /* SZPONTOS_DRIVERS_TTY_H */
