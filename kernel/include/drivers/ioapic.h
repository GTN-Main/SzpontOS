#ifndef SZPONTOS_DRIVERS_IOAPIC_H
#define SZPONTOS_DRIVERS_IOAPIC_H

#include <kernel/types.h>

void ioapic_init(void);
void ioapic_map_irq(uint8_t irq, uint8_t vector, uint8_t dest_apic_id, bool level_trigger, bool active_low);
void lapic_eoi(void);
bool ioapic_is_active(void);
bool lapic_timer_init(uint32_t frequency_hz);

#endif /* SZPONTOS_DRIVERS_IOAPIC_H */
