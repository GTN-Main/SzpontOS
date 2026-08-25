/*
 * SzpontOS - USB HID (Human Interface Device) Keyboard Protocol & Parser
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_HID_H
#define SZPONTOS_DRIVERS_HID_H

#include <kernel/types.h>
#include <stdbool.h>

/* HID Modifier Bitmasks (Report Byte 0) */
#define HID_MOD_LCTRL (1 << 0)
#define HID_MOD_LSHIFT (1 << 1)
#define HID_MOD_LALT (1 << 2)
#define HID_MOD_LGUI (1 << 3)
#define HID_MOD_RCTRL (1 << 4)
#define HID_MOD_RSHIFT (1 << 5)
#define HID_MOD_RALT (1 << 6)
#define HID_MOD_RGUI (1 << 7)

/* Process an 8-byte HID Boot Keyboard Report */
void hid_process_keyboard_report(const uint8_t report[8]);

#endif /* SZPONTOS_DRIVERS_HID_H */
