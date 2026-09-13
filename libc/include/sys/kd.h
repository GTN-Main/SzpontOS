#ifndef _SYS_KD_H
#define _SYS_KD_H

#include <sys/ioctl.h>

#define GIO_FONT       0x4B60
#define PIO_FONT       0x4B61
#define GIO_FONTX      0x4B6B
#define PIO_FONTX      0x4B6C
#define PIO_FONTRESET  0x4B6D

#define GIO_SCRNMAP    0x4B40
#define PIO_SCRNMAP    0x4B41
#define GIO_UNISCRNMAP 0x4B69
#define PIO_UNISCRNMAP 0x4B6A
#define GIO_UNIMAP     0x4B66
#define PIO_UNIMAP     0x4B67
#define PIO_UNIMAPCLR  0x4B68

#define KDADDIO        0x4B34
#define KDDELIO        0x4B35
#define KDENABIO       0x4B36
#define KDDISABIO      0x4B37

#define KDSETMODE      0x4B3A
#define KDGETMODE      0x4B3B
#define KD_TEXT        0x00
#define KD_GRAPHICS    0x01

#define KDGKBMODE      0x4B44
#define KDSKBMODE      0x4B45
#define K_RAW          0x00
#define K_XLATE        0x01
#define K_MEDIUMRAW    0x02
#define K_UNICODE      0x03
#define K_OFF          0x04

#define KDGKBMETA      0x4B62
#define KDSKBMETA      0x4B63
#define K_METABIT      0x03
#define K_ESCPREFIX    0x04

#define KDGKBLED       0x4B64
#define KDSKBLED       0x4B65
#define LED_SCR        0x01
#define LED_NUM        0x02
#define LED_CAP        0x04

#define KDGKBTYPE      0x4B33
#define KB_84          0x01
#define KB_101         0x02
#define KB_OTHER       0x03

#define KDGETLED       0x4B64
#define KDSETLED       0x4B65

#define KIOCSOUND      0x4B2F
#define KDMKTONE       0x4B30

#define KDGKBENT       0x4B46
#define KDSKBENT       0x4B47

struct kbentry {
    unsigned char kb_table;
    unsigned char kb_index;
    unsigned short kb_value;
};

#endif /* _SYS_KD_H */
