#ifndef SZPONTOS_DRIVERS_KEYBOARD_H
#define SZPONTOS_DRIVERS_KEYBOARD_H

#include <kernel/types.h>

void keyboard_init(void);
bool keyboard_has_char(void);
char keyboard_getc(void);

#endif /* SZPONTOS_DRIVERS_KEYBOARD_H */
