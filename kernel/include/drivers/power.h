/*
 * SzpontOS - ACPI Power Management & Hardware Reset (Shutdown / Reboot)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_POWER_H
#define SZPONTOS_DRIVERS_POWER_H

#include <kernel/types.h>

#define REBOOT_MAGIC1       0xfee1dead
#define REBOOT_MAGIC2       0x28121969

#define REBOOT_CMD_RESTART   0x01234567
#define REBOOT_CMD_HALT      0xcdef0123
#define REBOOT_CMD_POWER_OFF 0x4321fedc

void power_shutdown(void) __attribute__((noreturn));
void power_reboot(void) __attribute__((noreturn));
int  sys_reboot(int magic1, int magic2, int cmd, void *arg);

#endif /* SZPONTOS_DRIVERS_POWER_H */
