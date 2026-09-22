/*
 * SzpontOS - Intel i915 PCI Hardware Detection & MMIO Mapping
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/i915/i915_drv.h>
#include <drivers/i915/i915_pciids.h>
#include <drivers/i915/i915_reg.h>
#include <mm/heap.h>
#include <mm/vmm.h>
#include <kernel/kprint.h>
#include <kernel/string.h>

#define PAGE_SIZE 4096UL

static i915_device_t g_i915_dev;

bool i915_is_active(void) {
    return g_i915_dev.active;
}

i915_device_t *i915_get_device(void) {
    if (!g_i915_dev.mmio_base)
        return NULL;
    return &g_i915_dev;
}

uint32_t i915_read32(uint32_t reg) {
    if (!g_i915_dev.mmio_base)
        return 0;
    return *(volatile uint32_t *)(g_i915_dev.mmio_base + reg);
}

void i915_write32(uint32_t reg, uint32_t val) {
    if (!g_i915_dev.mmio_base)
        return;
    *(volatile uint32_t *)(g_i915_dev.mmio_base + reg) = val;
}

void i915_posting_read(uint32_t reg) {
    (void)i915_read32(reg);
}

static void i915_forcewake_get(i915_device_t *dev) {
    if (dev->gen < INTEL_GEN6)
        return;

    if (dev->gen >= INTEL_GEN9) {
        /* Gen9+ (Skylake, Kaby Lake, Coffee Lake, etc.) */
        i915_write32(0xa278, 0x00010001); /* FORCEWAKE_RENDER_GEN9 */
        i915_write32(0xa270, 0x00010001); /* FORCEWAKE_MEDIA_GEN9 */
        for (int i = 0; i < 5000; i++) {
            if ((i915_read32(0x0d84) & 1) != 0) /* FORCEWAKE_ACK_RENDER_GEN9 */
                break;
            __asm__ volatile("pause" ::: "memory");
        }
    } else if (dev->gen >= INTEL_GEN7_5) {
        /* Haswell */
        i915_write32(FORCEWAKE_MT, 0x00010001);
        for (int i = 0; i < 5000; i++) {
            if ((i915_read32(FORCEWAKE_ACK_HSW) & 1) != 0)
                break;
            __asm__ volatile("pause" ::: "memory");
        }
    } else {
        /* Sandy Bridge / Ivy Bridge */
        i915_write32(FORCEWAKE_GEN6, 1);
        for (int i = 0; i < 5000; i++) {
            if ((i915_read32(FORCEWAKE_ACK_GEN6) & 1) != 0)
                break;
            __asm__ volatile("pause" ::: "memory");
        }
    }
}

bool i915_pci_probe(pci_device_t *pci_dev) {
    if (!pci_dev || pci_dev->vendor_id != INTEL_VENDOR_ID)
        return false;

    memset(&g_i915_dev, 0, sizeof(i915_device_t));
    spinlock_init(&g_i915_dev.lock);

    g_i915_dev.pci_dev = pci_dev;
    g_i915_dev.device_id = pci_dev->device_id;
    g_i915_dev.gen = i915_get_device_gen(pci_dev->device_id);
    g_i915_dev.name = i915_get_device_name(pci_dev->device_id);

    /* Enable Bus Mastering and MMIO access */
    pci_enable_bus_mastering(pci_dev);
    uint16_t cmd = pci_read16(pci_dev->bus, pci_dev->slot, pci_dev->func, PCI_COMMAND);
    cmd |= (PCI_COMMAND_MMIO | PCI_COMMAND_MASTER);
    pci_write16(pci_dev->bus, pci_dev->slot, pci_dev->func, PCI_COMMAND, cmd);

    /* BAR 0: MMIO Registers & GTT PTEs */
    g_i915_dev.mmio_paddr = pci_dev->bar[0] & ~0xFULL;
    size_t default_mmio_size = (g_i915_dev.gen >= INTEL_GEN8) ? (16 * 1024 * 1024) : (4 * 1024 * 1024);
    g_i915_dev.mmio_size = pci_dev->bar_size[0] ? pci_dev->bar_size[0] : default_mmio_size;
    if (!g_i915_dev.mmio_paddr) {
        klog_err("i915: BAR 0 MMIO address is invalid");
        return false;
    }

    /* Map BAR 0 MMIO pages into kernel page tables with cache disabled */
    size_t mmio_pages = g_i915_dev.mmio_size / PAGE_SIZE;
    if (mmio_pages > 4096) mmio_pages = 4096; /* Up to 16 MiB */
    for (size_t p = 0; p < mmio_pages; p++) {
        uintptr_t phys = g_i915_dev.mmio_paddr + p * PAGE_SIZE;
        uintptr_t virt = (uintptr_t)PHYS_TO_VIRT(phys);
        vmm_map_page(&g_kernel_pagemap, virt, phys,
                     VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_CACHE_DISABLE);
    }
    g_i915_dev.mmio_base = (volatile uint8_t *)PHYS_TO_VIRT(g_i915_dev.mmio_paddr);

    /* BAR 2: Aperture */
    g_i915_dev.aperture_paddr = pci_dev->bar[2] & ~0xFULL;
    g_i915_dev.aperture_size = pci_dev->bar_size[2] ? pci_dev->bar_size[2] : (256 * 1024 * 1024);
    if (g_i915_dev.aperture_paddr) {
        /* Map initial aperture chunk (64 MiB) as Write-Combining */
        size_t aper_pages = (64 * 1024 * 1024) / PAGE_SIZE;
        if (aper_pages > (g_i915_dev.aperture_size / PAGE_SIZE))
            aper_pages = g_i915_dev.aperture_size / PAGE_SIZE;
        for (size_t p = 0; p < aper_pages; p++) {
            uintptr_t phys = g_i915_dev.aperture_paddr + p * PAGE_SIZE;
            uintptr_t virt = (uintptr_t)PHYS_TO_VIRT(phys);
            vmm_map_page(&g_kernel_pagemap, virt, phys,
                         VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_WRITE_COMBINING);
        }
        g_i915_dev.aperture_base = (uint8_t *)PHYS_TO_VIRT(g_i915_dev.aperture_paddr);
    }

    g_i915_dev.is_64bit_gtt = (g_i915_dev.gen >= INTEL_GEN8);
    g_i915_dev.gtt_pte_offset = g_i915_dev.is_64bit_gtt ? GEN8_GTT_PTE_OFFSET : GEN6_GTT_PTE_OFFSET;
    g_i915_dev.gtt_total_size = (uint64_t)512 * 1024 * 1024; /* 512 MiB default GGTT */

    klog_info("i915: Found %s (dev=0x%04x, gen=%d, BAR0=0x%lx, Aperture=0x%lx [%lu MB])",
              g_i915_dev.name, g_i915_dev.device_id, (int)g_i915_dev.gen,
              g_i915_dev.mmio_paddr, g_i915_dev.aperture_paddr,
              g_i915_dev.aperture_size / (1024 * 1024));

    /* Initialize Forcewake on Gen6+ */
    i915_forcewake_get(&g_i915_dev);

    return true;
}
