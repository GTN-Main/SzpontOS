/*
 * SzpontOS - PS/2 Test Driver (Raw Keypress Detection, NO IRQ)
 * Test-only driver. Reads raw scancodes from the i8042 output buffer
 * by polling. Keyboard IRQ is left completely disabled.
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_PS2_TEST_H
#define SZPONTOS_DRIVERS_PS2_TEST_H

#include <kernel/types.h>
#include <stdbool.h>

void ps2_test_init(void);
void ps2_test_poll(void);
bool ps2_test_active(void);
bool ps2_test_key_was_pressed(uint8_t *scancode);
bool ps2_test_has_key(void);

#endif /* SZPONTOS_DRIVERS_PS2_TEST_H */
