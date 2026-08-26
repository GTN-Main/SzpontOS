/*
 * SzpontOS - Universal Mouse Subsystem Architecture
 * Unified abstraction and event distribution for PS/2, USB HID, and VirtIO mice.
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_MOUSE_H
#define SZPONTOS_DRIVERS_MOUSE_H

#include <kernel/types.h>
#include <stdbool.h>

/* Standard Mouse Button Bitmasks */
#define MOUSE_BUTTON_LEFT   (1 << 0)
#define MOUSE_BUTTON_RIGHT  (1 << 1)
#define MOUSE_BUTTON_MIDDLE (1 << 2)

/* Mouse Event Descriptor */
typedef struct mouse_event {
    uint8_t buttons;
    int32_t dx;
    int32_t dy;
    int8_t dz;       /* Scroll wheel */
    int32_t abs_x;   /* Absolute coordinates (e.g. tablet) */
    int32_t abs_y;
    bool is_absolute;
} mouse_event_t;

/* Universal Mouse Subsystem API */
void mouse_init(void);
void mouse_push_event(const mouse_event_t *ev);
bool mouse_has_event(void);
bool mouse_get_event(mouse_event_t *ev);

/* DevFS Interface (/dev/mouse & /dev/input/mice) */
ssize_t mouse_devfs_read(void *buf, size_t count);

#endif /* SZPONTOS_DRIVERS_MOUSE_H */
