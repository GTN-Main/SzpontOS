/*
 * SzpontOS - POSIX termios.h
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef _TERMIOS_H
#define _TERMIOS_H

#include <sys/types.h>
#include <sys/ioctl.h>

typedef unsigned int   tcflag_t;
typedef unsigned char  cc_t;
typedef unsigned int   speed_t;

#define NCCS 32

struct termios {
    tcflag_t c_iflag;      /* Input modes */
    tcflag_t c_oflag;      /* Output modes */
    tcflag_t c_cflag;      /* Control modes */
    tcflag_t c_lflag;      /* Local modes */
    cc_t     c_line;       /* Line discipline */
    cc_t     c_cc[NCCS];   /* Control characters */
    speed_t  c_ispeed;     /* Input baud rate */
    speed_t  c_ospeed;     /* Output baud rate */
};

/* c_cc indices */
#define VINTR    0
#define VQUIT    1
#define VERASE   2
#define VKILL    3
#define VEOF     4
#define VTIME    5
#define VMIN     6
#define VSTART   8
#define VSTOP    9
#define VSUSP    10
#define VEOL     11

#define _POSIX_VDISABLE 0

/* Input modes (c_iflag) */
#define IGNBRK  0000001
#define BRKINT  0000002
#define IGNPAR  0000004
#define PARMRK  0000010
#define INPCK   0000020
#define ISTRIP  0000040
#define INLCR   0000100
#define IGNCR   0000200
#define ICRNL   0000400
#define IUCLC   0001000
#define IXON    0002000
#define IXANY   0004000
#define IXOFF   0010000

/* Output modes (c_oflag) */
#define OPOST   0000001
#define OLCUC   0000002
#define ONLCR   0000004
#define OCRNL   0000010
#define ONOCR   0000020
#define ONLRET  0000040

/* Control modes (c_cflag) */
#define CSIZE   0000060
#define CS5     0000000
#define CS6     0000020
#define CS7     0000040
#define CS8     0000060
#define CSTOPB  0000100
#define CREAD   0000200
#define PARENB  0000400
#define PARODD  0001000
#define HUPCL   0002000
#define CLOCAL  0004000

/* Local modes (c_lflag) */
#define ISIG    0000001
#define ICANON  0000002
#define ECHO    0000010
#define ECHOE   0000020
#define ECHOK   0000040
#define ECHONL  0000100
#define NOFLSH  0000200
#define TOSTOP  0000400
#define IEXTEN  0100000

/* tcsetattr actions */
#define TCSANOW    0
#define TCSADRAIN  1
#define TCSAFLUSH  2

/* tcflush actions */
#define TCIFLUSH   0
#define TCOFLUSH   1
#define TCIOFLUSH  2

/* tcflow actions */
#define TCOOFF     0
#define TCOON      1
#define TCIOFF     2
#define TCION      3

/* Baud rates */
#define B0       0
#define B50      1
#define B75      2
#define B110     3
#define B134     4
#define B150     5
#define B200     6
#define B300     7
#define B600     8
#define B1200    9
#define B1800    10
#define B2400    11
#define B4800    12
#define B9600    13
#define B19200   14
#define B38400   15
#define B57600   16
#define B115200  17

int tcgetattr(int fd, struct termios *termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);
int tcflush(int fd, int queue_selector);
int tcflow(int fd, int action);
speed_t cfgetispeed(const struct termios *termios_p);
speed_t cfgetospeed(const struct termios *termios_p);
int cfsetispeed(struct termios *termios_p, speed_t speed);
int cfsetospeed(struct termios *termios_p, speed_t speed);

#endif /* _TERMIOS_H */
