/*
 * SzpontOS - Evdev & Mouse/Keyboard Input Event Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_EVDEV_H
#define SZPONTOS_DRIVERS_EVDEV_H

#include <kernel/types.h>

struct timeval {
    int64_t tv_sec;
    int64_t tv_usec;
};

struct input_event {
    struct timeval time;
    uint16_t type;
    uint16_t code;
    int32_t value;
};

struct input_id {
    uint16_t bustype;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;
};

#define EV_SYN       0x00
#define EV_KEY       0x01
#define EV_REL       0x02
#define EV_ABS       0x03
#define EV_MSC       0x04
#define EV_SW        0x05
#define EV_LED       0x11
#define EV_SND       0x12
#define EV_REP       0x14
#define EV_FF        0x15
#define EV_PWR       0x16
#define EV_FF_STATUS 0x17
#define EV_MAX       0x1f

#define SYN_REPORT      0
#define SYN_CONFIG      1
#define SYN_MT_REPORT   2
#define SYN_DROPPED     3

#define REL_X           0x00
#define REL_Y           0x01
#define REL_Z           0x02
#define REL_RX          0x03
#define REL_RY          0x04
#define REL_RZ          0x05
#define REL_HWHEEL      0x06
#define REL_DIAL        0x07
#define REL_WHEEL       0x08
#define REL_MISC        0x09
#define REL_MAX         0x0f

#define BTN_MISC        0x100
#define BTN_0           0x100
#define BTN_1           0x101
#define BTN_2           0x102
#define BTN_MOUSE       0x110
#define BTN_LEFT        0x110
#define BTN_RIGHT       0x111
#define BTN_MIDDLE      0x112
#define BTN_SIDE        0x113
#define BTN_EXTRA       0x114

#define EVIOCGVERSION   0x80044501
#define EVIOCGID        0x80084502

void evdev_init(void);
void evdev_push_mouse_packet(int dx, int dy, int dz, uint8_t buttons);
void evdev_push_key(uint16_t keycode, bool pressed);

ssize_t evdev_mice_read(void *buf, size_t count);
ssize_t evdev_mouse_event_read(void *buf, size_t count);
ssize_t evdev_kbd_event_read(void *buf, size_t count);
bool evdev_mouse_has_events(void);
bool evdev_kbd_has_events(void);
bool evdev_mice_has_data(void);

int evdev_mouse_ioctl(uint64_t request, uintptr_t arg);
int evdev_kbd_ioctl(uint64_t request, uintptr_t arg);

#endif /* SZPONTOS_DRIVERS_EVDEV_H */
