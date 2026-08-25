/*
 * SzpontOS - PS/2 Mouse Driver (i8042 AUX Port / IRQ 12)
 * Inspired by FreeBSD sys/dev/atkbdc/psm.c
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_PS2_MOUSE_H
#define SZPONTOS_DRIVERS_PS2_MOUSE_H

#include <kernel/types.h>
#include <stdbool.h>

/* Mouse button flags */
#define MOUSE_BTN_LEFT (1 << 0)
#define MOUSE_BTN_RIGHT (1 << 1)
#define MOUSE_BTN_MIDDLE (1 << 2)

typedef struct mouse_packet {
    uint8_t buttons;
    int32_t dx;
    int32_t dy;
    int8_t dz; /* Scroll wheel delta */
} mouse_packet_t;

void ps2_mouse_init(void);
void ps2_mouse_handle_byte(uint8_t byte);
bool ps2_mouse_is_enabled(void);
bool ps2_mouse_has_packet(void);
bool ps2_mouse_get_packet(mouse_packet_t *pkt);

/* DevFS read interface for /dev/psaux */
ssize_t ps2_mouse_devfs_read(void *buf, size_t count);

#endif /* SZPONTOS_DRIVERS_PS2_MOUSE_H */
