/*
 * SzpontOS - PC Speaker Audio Driver
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_SPEAKER_H
#define SZPONTOS_DRIVERS_SPEAKER_H

#include <kernel/types.h>

void speaker_tone(uint32_t freq_hz);
void speaker_off(void);
void speaker_beep(uint32_t freq_hz, uint32_t duration_ms);

#endif /* SZPONTOS_DRIVERS_SPEAKER_H */
