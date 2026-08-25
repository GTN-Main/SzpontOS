#ifndef SZPONTOS_ARCH_X86_64_PIC_H
#define SZPONTOS_ARCH_X86_64_PIC_H

#include <kernel/types.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI 0x20

void pic_remap(uint8_t offset1, uint8_t offset2);
void pic_disable(void);
void pic_send_eoi(uint8_t irq);
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);
void pic_enable_apic_extint(void);

#endif /* SZPONTOS_ARCH_X86_64_PIC_H */
