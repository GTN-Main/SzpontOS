/*
 * SzpontOS - Realtek RTL8139 PCI Fast Ethernet Driver
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_RTL8139_H
#define SZPONTOS_DRIVERS_RTL8139_H

#include <kernel/types.h>
#include <stdbool.h>

void rtl8139_init(void);
void rtl8139_poll(void);

#endif /* SZPONTOS_DRIVERS_RTL8139_H */
