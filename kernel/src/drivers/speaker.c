/*
 * SzpontOS - PC Speaker Audio Driver
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/speaker.h>
#include <arch/x86_64/io.h>
#include <arch/x86_64/pit.h>
#include <sched/sched.h>
#include <kernel/kprint.h>

#define PIT_CHANNEL2_DATA 0x42
#define PIT_COMMAND_PORT 0x43
#define SYSTEM_CTRL_PORT 0x61

void speaker_tone(uint32_t freq_hz) {
    if (freq_hz == 0) {
        speaker_off();
        return;
    }

    uint32_t div = 1193180 / freq_hz;
    if (div > 65535)
        div = 65535;
    if (div == 0)
        div = 1;

    /* Set Channel 2 to Mode 3 (Square Wave Generator), Binary 16-bit */
    outb(PIT_COMMAND_PORT, 0xB6);
    outb(PIT_CHANNEL2_DATA, (uint8_t)(div & 0xFF));
    outb(PIT_CHANNEL2_DATA, (uint8_t)((div >> 8) & 0xFF));

    /* Enable PC Speaker (Bits 0 and 1 on Port 0x61) */
    uint8_t val = inb(SYSTEM_CTRL_PORT);
    if ((val & 3) != 3) {
        outb(SYSTEM_CTRL_PORT, val | 3);
    }
}

void speaker_off(void) {
    uint8_t val = inb(SYSTEM_CTRL_PORT) & 0xFC;
    outb(SYSTEM_CTRL_PORT, val);
}

void speaker_beep(uint32_t freq_hz, uint32_t duration_ms) {
    speaker_tone(freq_hz);
    pit_sleep(duration_ms > 0 ? duration_ms : 1);
    speaker_off();
}
