/*
 * SzpontOS - Intel i915 Global Graphics Translation Table (GGTT) Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/i915/i915_drv.h>
#include <drivers/i915/i915_reg.h>
#include <mm/vmm.h>
#include <kernel/kprint.h>

#define GTT_PAGE_SIZE           4096
#define GTT_RESERVED_START      (16 * 1024 * 1024) /* Reserve first 16 MiB for stolen/HWS */

int i915_gtt_init(i915_device_t *dev) {
    if (!dev || !dev->mmio_base)
        return -1;

    dev->next_gtt_alloc = GTT_RESERVED_START;
    klog_info("i915: GGTT initialized (PTE offset 0x%x, mode: %s)",
              dev->gtt_pte_offset, dev->is_64bit_gtt ? "64-bit Gen8+" : "32-bit Gen6/7");
    return 0;
}

int64_t i915_gtt_alloc(i915_device_t *dev, size_t size, size_t alignment) {
    if (!dev || size == 0)
        return -1;

    if (alignment < GTT_PAGE_SIZE)
        alignment = GTT_PAGE_SIZE;

    spinlock_acquire(&dev->lock);

    /* Align next allocation */
    uint64_t offset = (dev->next_gtt_alloc + alignment - 1) & ~(alignment - 1);
    size_t aligned_size = (size + GTT_PAGE_SIZE - 1) & ~(GTT_PAGE_SIZE - 1);

    if (offset + aligned_size > dev->gtt_total_size) {
        spinlock_release(&dev->lock);
        klog_err("i915: Out of GGTT address space (requested %lu bytes)", size);
        return -1;
    }

    dev->next_gtt_alloc = offset + aligned_size;
    spinlock_release(&dev->lock);

    return (int64_t)offset;
}

void i915_gtt_free(i915_device_t *dev, uint64_t offset, size_t size) {
    (void)dev;
    (void)offset;
    (void)size;
    /* Basic allocator: in future can maintain bitmap/free-list */
}

int i915_gtt_bind_pages(i915_device_t *dev, uint64_t offset, uintptr_t *pages, size_t num_pages, uint32_t cache_level) {
    if (!dev || !dev->mmio_base || !pages || num_pages == 0)
        return -1;

    uint64_t page_idx = offset / GTT_PAGE_SIZE;
    volatile uint8_t *pte_base = dev->mmio_base + dev->gtt_pte_offset;

    if (dev->is_64bit_gtt) {
        volatile uint64_t *ptes = (volatile uint64_t *)pte_base;
        for (size_t i = 0; i < num_pages; i++) {
            uint64_t pte = (pages[i] & PHYS_ADDR_MASK) | GEN8_PAGE_PRESENT | GEN8_PAGE_RW;
            if (cache_level != I915_CACHE_NONE) {
                pte |= GEN8_PAGE_LLC;
            }
            ptes[page_idx + i] = pte;
        }
    } else {
        volatile uint32_t *ptes = (volatile uint32_t *)pte_base;
        for (size_t i = 0; i < num_pages; i++) {
            uint32_t pte = (uint32_t)(pages[i] & 0xFFFFF000ULL) | GEN6_PTE_VALID;
            if (cache_level != I915_CACHE_NONE) {
                pte |= GEN6_PTE_CACHE_LLC;
            }
            ptes[page_idx + i] = pte;
        }
    }

    /* Post GTT writes and flush CPU write buffers */
    __asm__ volatile("mfence" ::: "memory");
    i915_posting_read(dev->gtt_pte_offset);

    return 0;
}

void i915_gtt_unbind(i915_device_t *dev, uint64_t offset, size_t num_pages) {
    if (!dev || !dev->mmio_base || num_pages == 0)
        return;

    uint64_t page_idx = offset / GTT_PAGE_SIZE;
    volatile uint8_t *pte_base = dev->mmio_base + dev->gtt_pte_offset;

    if (dev->is_64bit_gtt) {
        volatile uint64_t *ptes = (volatile uint64_t *)pte_base;
        for (size_t i = 0; i < num_pages; i++) {
            ptes[page_idx + i] = 0;
        }
    } else {
        volatile uint32_t *ptes = (volatile uint32_t *)pte_base;
        for (size_t i = 0; i < num_pages; i++) {
            ptes[page_idx + i] = 0;
        }
    }

    __asm__ volatile("mfence" ::: "memory");
    i915_posting_read(dev->gtt_pte_offset);
}

void i915_gtt_chipset_flush(i915_device_t *dev) {
    if (!dev || !dev->mmio_base)
        return;
    __asm__ volatile("mfence" ::: "memory");
    i915_posting_read(RING_HEAD(RCS_RING_BASE));
}
