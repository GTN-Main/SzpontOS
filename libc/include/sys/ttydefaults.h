/*
 * SzpontOS - POSIX sys/ttydefaults.h
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef _SYS_TTYDEFAULTS_H
#define _SYS_TTYDEFAULTS_H

#define TTYDEF_IFLAG  (BRKINT | ICRNL | IXON)
#define TTYDEF_OFLAG  (OPOST | ONLCR)
#define TTYDEF_LFLAG  (ECHO | ECHOE | ECHOK | ICANON | ISIG | IEXTEN)
#define TTYDEF_CFLAG  (CS8 | CREAD | HUPCL)
#define TTYDEF_SPEED  (B115200)

#define CEOF    0x04
#define CEOL    0x00
#define CERASE  0x7f
#define CINTR   0x03
#define CKILL   0x15
#define CQUIT   0x1c
#define CSUSP   0x1a
#define CSTART  0x11
#define CSTOP   0x13

#endif /* _SYS_TTYDEFAULTS_H */
