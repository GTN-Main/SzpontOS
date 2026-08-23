#ifndef SZPONTOS_ARCH_X86_64_PIT_H
#define SZPONTOS_ARCH_X86_64_PIT_H

#include <kernel/types.h>

void pit_init(uint32_t frequency_hz);
uint64_t pit_get_ticks(void);
uint32_t pit_get_frequency(void);
void pit_sleep(uint32_t ms);

#endif /* SZPONTOS_ARCH_X86_64_PIT_H */
