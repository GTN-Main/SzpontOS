#include <arch/x86_64/pic.h>
#include <arch/x86_64/io.h>
#include <kernel/kprint.h>

#define ICW1_ICW4 0x01
#define ICW1_INIT 0x10
#define ICW4_8086 0x01

void pic_remap(uint8_t offset1, uint8_t offset2) {
    uint8_t a1, a2;

    /* Save masks */
    a1 = inb(PIC1_DATA);
    a2 = inb(PIC2_DATA);

    /* Start init sequence in cascade mode */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* Set vector offsets */
    outb(PIC1_DATA, offset1);
    io_wait();
    outb(PIC2_DATA, offset2);
    io_wait();

    /* Tell Master PIC that there is a slave PIC at IRQ2 (0000 0100) */
    outb(PIC1_DATA, 4);
    io_wait();
    /* Tell Slave PIC its cascade identity (0000 0010) */
    outb(PIC2_DATA, 2);
    io_wait();

    /* Set 8086 mode */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Restore masks */
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);

    klog_info("8259 PIC remapped to vectors 0x%02x - 0x%02x", offset1, offset2 + 7);
}

void pic_disable(void) {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    klog_info("8259 PIC disabled (all IRQs masked)");
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_set_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) | (1 << irq);
    outb(port, value);
}

void pic_clear_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

#include <mm/vmm.h>

static inline uint64_t pic_rdmsr(uint32_t msr) {
    uint32_t low, high;
#if defined(__x86_64__)
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
#else
    low = high = 0;
#endif
    return ((uint64_t)high << 32) | low;
}

void pic_enable_apic_extint(void) {
    /* If Local APIC is enabled, configure LINT0 as ExtINT so 8259 PIC interrupts are forwarded to CPU */
    uint64_t apic_base = pic_rdmsr(0x1B);
    if (apic_base & (1 << 11)) {
        uintptr_t lapic_phys = apic_base & 0xFFFFF000ULL;
        if (lapic_phys == 0)
            lapic_phys = 0xFEE00000;

        uintptr_t lapic_virt = (uintptr_t)PHYS_TO_VIRT(lapic_phys);
        vmm_map_page(&g_kernel_pagemap, lapic_virt, lapic_phys,
                     VMM_FLAG_WRITABLE | VMM_FLAG_PRESENT | VMM_FLAG_CACHE_DISABLE);

        volatile uint32_t *lapic = (volatile uint32_t *)lapic_virt;

        /* Enable APIC in Spurious Interrupt Vector Register (SVR 0xF0) */
        lapic[0xF0 / 4] |= 0x1FF;

        /* Task Priority Register (TPR 0x80) - accept all priority classes */
        lapic[0x80 / 4] = 0;

        /* LVT LINT0 (0x350) - configure Delivery Mode = 0b111 (ExtINT), Unmasked (bit 16 = 0) */
        lapic[0x350 / 4] = 0x00000700;

        /* LVT LINT1 (0x360) - configure Delivery Mode = 0b100 (NMI), Unmasked */
        lapic[0x360 / 4] = 0x00000400;

        klog_info("Local APIC ExtINT pass-through configured on LINT0 (phys: 0x%016lx)", (unsigned long)lapic_phys);
    }
}
