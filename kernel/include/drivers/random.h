/*
 * SzpontOS - Hardware / Software Random Number Generator (CSPRNG)
 * Inspired by FreeBSD sys/dev/random/
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_RANDOM_H
#define SZPONTOS_DRIVERS_RANDOM_H

#include <kernel/types.h>

#define GRND_NONBLOCK 0x0001
#define GRND_RANDOM 0x0002

void random_init(void);
void random_add_entropy(uint64_t data);
size_t random_get_bytes(void *buf, size_t len);
uint32_t random_get_u32(void);
uint64_t random_get_u64(void);

#endif /* SZPONTOS_DRIVERS_RANDOM_H */
