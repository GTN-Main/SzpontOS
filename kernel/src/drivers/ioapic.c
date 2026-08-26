/*
 * SzpontOS - IO-APIC & Local APIC Interrupt Routing Engine
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/ioapic.h>
#include <drivers/acpi.h>
#include <arch/x86_64/pic.h>
#include <arch/x86_64/io.h>
#include <mm/vmm.h>
#include <kernel/kprint.h>
#include <kernel/string.h>

#define IOAPIC_REG_ID          0x00
#define IOAPIC_REG_VER         0x01
#define IOAPIC_REG_ARB         0x02
#define IOAPIC_REG_REDTBL_BASE 0x10

#define MADT_TYPE_LAPIC        0
#define MADT_TYPE_IOAPIC       1
#define MADT_TYPE_ISO          2
#define MADT_TYPE_NMI          4
#define MADT_TYPE_LAPIC_OVERRIDE 5

typedef struct {
    uint8_t ioapic_id;
    uintptr_t phys_addr;
    uintptr_t virt_addr;
    uint32_t gsi_base;
    uint32_t max_redirection_entries;
} ioapic_desc_t;

typedef struct {
    uint8_t bus;
    uint8_t source_irq;
    uint32_t gsi;
    uint16_t flags;
} iso_desc_t;

static ioapic_desc_t g_ioapics[4];
static size_t g_ioapic_count = 0;

static iso_desc_t g_isos[16];
static size_t g_iso_count = 0;

static uintptr_t g_lapic_virt = 0;
static bool g_ioapic_active = false;

static inline uint32_t ioapic_read(ioapic_desc_t *io, uint8_t reg) {
    volatile uint32_t *base = (volatile uint32_t *)io->virt_addr;
    base[0] = reg;
    return base[4]; /* 0x10 / 4 = index 4 */
}

static inline void ioapic_write(ioapic_desc_t *io, uint8_t reg, uint32_t val) {
    volatile uint32_t *base = (volatile uint32_t *)io->virt_addr;
    base[0] = reg;
    base[4] = val;
}

void lapic_eoi(void) {
    if (g_lapic_virt) {
        volatile uint32_t *lapic = (volatile uint32_t *)g_lapic_virt;
        lapic[0xB0 / 4] = 0;
    }
}

bool ioapic_is_active(void) {
    return g_ioapic_active;
}

void ioapic_map_irq(uint8_t irq, uint8_t vector, uint8_t dest_apic_id, bool level_trigger, bool active_low) {
    if (g_ioapic_count == 0)
        return;

    /* Check for Interrupt Source Override (ISO) */
    uint32_t gsi = irq;
    for (size_t i = 0; i < g_iso_count; i++) {
        if (g_isos[i].source_irq == irq) {
            gsi = g_isos[i].gsi;
            uint16_t flags = g_isos[i].flags;
            /* Polarity: bit 1 = Active Low */
            if ((flags & 0x03) == 0x03)
                active_low = true;
            else if ((flags & 0x03) == 0x01)
                active_low = false;
            /* Trigger: bit 3 = Level Trigger */
            if ((flags & 0x0C) == 0x0C)
                level_trigger = true;
            else if ((flags & 0x0C) == 0x04)
                level_trigger = false;
            break;
        }
    }

    /* Find the IO-APIC that handles this GSI */
    ioapic_desc_t *target_io = NULL;
    for (size_t i = 0; i < g_ioapic_count; i++) {
        if (gsi >= g_ioapics[i].gsi_base &&
            gsi < g_ioapics[i].gsi_base + g_ioapics[i].max_redirection_entries) {
            target_io = &g_ioapics[i];
            break;
        }
    }

    if (!target_io)
        return;

    /* Detect target APIC ID (BSP APIC ID if 0 requested) */
    uint8_t target_apic_id = dest_apic_id;
    if (g_lapic_virt) {
        volatile uint32_t *lapic = (volatile uint32_t *)g_lapic_virt;
        target_apic_id = (uint8_t)((lapic[0x20 / 4] >> 24) & 0xFF);
    }

    uint32_t pin = gsi - target_io->gsi_base;
    uint32_t reg_lo = IOAPIC_REG_REDTBL_BASE + pin * 2;
    uint32_t reg_hi = reg_lo + 1;

    uint32_t low_val = vector; /* Fixed delivery, physical dest, unmasked (bit 16 = 0) */
    if (active_low)
        low_val |= (1 << 13);
    if (level_trigger)
        low_val |= (1 << 15);

    uint32_t hi_val = ((uint32_t)target_apic_id) << 24;

    ioapic_write(target_io, reg_lo, low_val | (1 << 16)); /* Mask while configuring */
    ioapic_write(target_io, reg_hi, hi_val);
    ioapic_write(target_io, reg_lo, low_val); /* Unmask */
}

void ioapic_init(void) {
    acpi_sdt_header_t *madt_hdr = acpi_find_table("APIC");
    if (!madt_hdr) {
        klog_warn("IO-APIC: MADT ACPI table not found, keeping legacy 8259 PIC");
        return;
    }

    acpi_madt_t *madt = (acpi_madt_t *)madt_hdr;
    uintptr_t lapic_phys = madt->lapic_addr;

    /* Parse MADT entries */
    uint8_t *ptr = (uint8_t *)(madt + 1);
    uint8_t *end = (uint8_t *)madt + madt->header.length;

    while (ptr < end) {
        uint8_t type = ptr[0];
        uint8_t len = ptr[1];
        if (len == 0)
            break;

        if (type == MADT_TYPE_IOAPIC) {
            if (g_ioapic_count < 4) {
                ioapic_desc_t *io = &g_ioapics[g_ioapic_count++];
                io->ioapic_id = ptr[2];
                io->phys_addr = *(uint32_t *)(ptr + 4);
                io->gsi_base = *(uint32_t *)(ptr + 8);
            }
        } else if (type == MADT_TYPE_ISO) {
            if (g_iso_count < 16) {
                iso_desc_t *iso = &g_isos[g_iso_count++];
                iso->bus = ptr[2];
                iso->source_irq = ptr[3];
                iso->gsi = *(uint32_t *)(ptr + 4);
                iso->flags = *(uint16_t *)(ptr + 8);
            }
        } else if (type == MADT_TYPE_LAPIC_OVERRIDE) {
            lapic_phys = *(uint64_t *)(ptr + 4);
        }

        ptr += len;
    }

    if (g_ioapic_count == 0) {
        klog_warn("IO-APIC: No IO-APICs reported in MADT");
        return;
    }

    /* Map Local APIC MMIO */
    g_lapic_virt = (uintptr_t)PHYS_TO_VIRT(lapic_phys);
    vmm_map_page(&g_kernel_pagemap, g_lapic_virt, lapic_phys,
                 VMM_FLAG_WRITABLE | VMM_FLAG_PRESENT | VMM_FLAG_CACHE_DISABLE);

    /* Initialize Local APIC */
    volatile uint32_t *lapic = (volatile uint32_t *)g_lapic_virt;

    /* Enable Local APIC via MSR (bit 11) */
    uint64_t apic_base = rdmsr(0x1B);
    apic_base |= (1ULL << 11);
    wrmsr(0x1B, apic_base);

    /* SVR (0xF0) - Enable APIC + spurious vector 0xFF */
    lapic[0xF0 / 4] = 0x1FF;
    /* TPR (0x80) - Clear Task Priority Register to accept all interrupts */
    lapic[0x80 / 4] = 0;

    /* Map and initialize each IO-APIC */
    for (size_t i = 0; i < g_ioapic_count; i++) {
        ioapic_desc_t *io = &g_ioapics[i];
        io->virt_addr = (uintptr_t)PHYS_TO_VIRT(io->phys_addr);
        vmm_map_page(&g_kernel_pagemap, io->virt_addr, io->phys_addr,
                     VMM_FLAG_WRITABLE | VMM_FLAG_PRESENT | VMM_FLAG_CACHE_DISABLE);

        uint32_t ver = ioapic_read(io, IOAPIC_REG_VER);
        io->max_redirection_entries = ((ver >> 16) & 0xFF) + 1;

        /* Mask all redirection entries initially */
        for (uint32_t pin = 0; pin < io->max_redirection_entries; pin++) {
            ioapic_write(io, IOAPIC_REG_REDTBL_BASE + pin * 2, (1 << 16) | (0x20 + pin));
            ioapic_write(io, IOAPIC_REG_REDTBL_BASE + pin * 2 + 1, 0);
        }

        klog_info("IO-APIC #%u initialized (phys: 0x%08lx, GSI base: %u, Pins: %u)",
                  io->ioapic_id, io->phys_addr, io->gsi_base, io->max_redirection_entries);
    }

    /* Route essential ISA IRQs:
     * IRQ 0 (Timer) -> Vector 0x20 (GSI from ISO or 2/0)
     * IRQ 1 (Keyboard) -> Vector 0x21 (GSI 1)
     * IRQ 12 (PS/2 Mouse) -> Vector 0x2C (GSI 12)
     */
    ioapic_map_irq(0, 0x20, 0, false, false);
    ioapic_map_irq(1, 0x21, 0, false, false);
    ioapic_map_irq(12, 0x2C, 0, false, false);

    /* Mask LINT0 (disable ExtINT legacy PIC bypass) and set LINT1 as NMI */
    if (g_lapic_virt) {
        volatile uint32_t *lapic_regs = (volatile uint32_t *)g_lapic_virt;
        lapic_regs[0x350 / 4] = 0x10000; /* Mask LINT0 */
        lapic_regs[0x360 / 4] = 0x00000400; /* LINT1 = NMI */
    }

    /* Mask all 16 IRQs on legacy 8259 PIC to prevent interrupt storms */
    pic_disable();

    g_ioapic_active = true;
    klog_info("IO-APIC: Advanced Interrupt Routing active (IRQ 0->0x20, IRQ 1->0x21, IRQ 12->0x2C)");
}

