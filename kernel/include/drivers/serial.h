#ifndef SZPONTOS_DRIVERS_SERIAL_H
#define SZPONTOS_DRIVERS_SERIAL_H

#include <kernel/types.h>

#define COM1_PORT 0x3F8

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *str);
void serial_write(const char *buf, size_t len);
bool serial_received(void);
char serial_getc(void);

#endif /* SZPONTOS_DRIVERS_SERIAL_H */
