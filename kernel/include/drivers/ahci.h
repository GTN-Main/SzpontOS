/*
 * SzpontOS - AHCI (Advanced Host Controller Interface) SATA Driver
 * Modern Bare Metal SATA Storage Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_AHCI_H
#define SZPONTOS_DRIVERS_AHCI_H

#include <kernel/types.h>
#include <stdbool.h>

void ahci_init(void);
bool ahci_is_active(void);

#endif /* SZPONTOS_DRIVERS_AHCI_H */
