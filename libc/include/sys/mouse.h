/*
 * SzpontOS - POSIX sys/mouse.h
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef _SYS_MOUSE_H_
#define _SYS_MOUSE_H_

#include <sys/types.h>
#include <sys/ioccom.h>

typedef struct mousestatus {
    int flags;
    int button;
    int obutton;
    int dx;
    int dy;
    int dz;
} mousestatus_t;

typedef struct mousehw {
    int buttons;
    int iftype;
    int type;
    int model;
    int hwid;
} mousehw_t;

typedef struct mousemode {
    int protocol;
    int rate;
    int resolution;
    int accelfactor;
    int level;
    int packetsize;
    unsigned char syncmask[2];
} mousemode_t;

#define MOUSE_GETSTATUS  _IOR('M', 0, mousestatus_t)
#define MOUSE_GETHWINFO  _IOR('M', 1, mousehw_t)
#define MOUSE_GETMODE    _IOR('M', 2, mousemode_t)
#define MOUSE_SETMODE    _IOW('M', 3, mousemode_t)
#define MOUSE_GETLEVEL   _IOR('M', 4, int)
#define MOUSE_SETLEVEL   _IOW('M', 5, int)

#define MOUSE_BUTTON1DOWN 0x0001
#define MOUSE_BUTTON2DOWN 0x0002
#define MOUSE_BUTTON3DOWN 0x0004
#define MOUSE_BUTTON4DOWN 0x0008
#define MOUSE_BUTTON5DOWN 0x0010
#define MOUSE_BUTTON6DOWN 0x0020
#define MOUSE_BUTTON7DOWN 0x0040
#define MOUSE_BUTTON8DOWN 0x0080

#define MOUSE_STDBUTTONS 0x0007
#define MOUSE_EXTBUTTONS 0x7ffffff8
#define MOUSE_BUTTONS    (MOUSE_STDBUTTONS | MOUSE_EXTBUTTONS)

#define MOUSE_POSCHANGED 0x80000000

#endif /* _SYS_MOUSE_H_ */
