/*
 * SzpontOS - Intel i915 Command Streamer Ring Buffer Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/i915/i915_drv.h>
#include <drivers/i915/i915_reg.h>
#include <drivers/rtc.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <kernel/kprint.h>
#include <kernel/string.h>

#define RING_PAGES (I915_RING_BUFFER_SIZE / 4096)

int i915_ring_init(i915_device_t *dev, int ring_id) {
    if (!dev || ring_id < 0 || ring_id >= I915_NUM_RINGS)
        return -1;

    i915_ring_t *ring = &dev->rings[ring_id];
    memset(ring, 0, sizeof(i915_ring_t));

    switch (ring_id) {
        case I915_RING_RCS: ring->mmio_base = RCS_RING_BASE; break;
        case I915_RING_BCS: ring->mmio_base = BCS_RING_BASE; break;
        case I915_RING_VCS: ring->mmio_base = VCS_RING_BASE; break;
        default: return -1;
    }

    /* Allocate ring buffer pages */
    uintptr_t phys = pmm_alloc_pages(RING_PAGES);
    if (!phys) {
        klog_err("i915: Failed to allocate ring buffer pages for ring %d", ring_id);
        return -1;
    }
    ring->paddr = phys;
    ring->vaddr = (void *)PHYS_TO_VIRT(phys);
    ring->size = I915_RING_BUFFER_SIZE;
    ring->head = 0;
    ring->tail = 0;
    ring->space = ring->size - 8;
    ring->last_seqno = 1;
    memset(ring->vaddr, 0, ring->size);

    /* Allocate address space in Global GTT for ring buffer */
    int64_t gtt_off = i915_gtt_alloc(dev, ring->size, 4096);
    if (gtt_off < 0) {
        pmm_free_pages(phys, RING_PAGES);
        return -1;
    }
    ring->gtt_offset = (uint64_t)gtt_off;

    uintptr_t ring_pages[RING_PAGES];
    for (size_t i = 0; i < RING_PAGES; i++) {
        ring_pages[i] = ring->paddr + i * 4096;
    }
    i915_gtt_bind_pages(dev, ring->gtt_offset, ring_pages, RING_PAGES, I915_CACHE_LLC);

    /* Allocate Hardware Status Page (HWS) */
    uintptr_t hws_phys = pmm_alloc_page();
    if (!hws_phys) {
        i915_gtt_free(dev, ring->gtt_offset, ring->size);
        pmm_free_pages(phys, RING_PAGES);
        return -1;
    }
    ring->hws_paddr = hws_phys;
    ring->hws_vaddr = (uint32_t *)PHYS_TO_VIRT(hws_phys);
    memset(ring->hws_vaddr, 0, 4096);

    /* Reset ring head & tail registers */
    i915_write32(RING_CTL(ring->mmio_base), 0);
    i915_write32(RING_HEAD(ring->mmio_base), 0);
    i915_write32(RING_TAIL(ring->mmio_base), 0);

    /* Program HWS Address */
    if (dev->gen >= INTEL_GEN6) {
        i915_write32(RING_HWS_PGA_GEN6(ring->mmio_base), (uint32_t)(ring->hws_paddr & 0xFFFFF000ULL));
    } else {
        i915_write32(RING_HWS_PGA(ring->mmio_base), (uint32_t)(ring->hws_paddr & 0xFFFFF000ULL));
    }

    /* Program Ring Start Address with GGTT offset */
    i915_write32(RING_START(ring->mmio_base), (uint32_t)ring->gtt_offset);

    /* Enable Ring: set page count, report 64K and valid bit */
    uint32_t ctl = RING_NR_PAGES(RING_PAGES) | RING_REPORT_64K | RING_VALID;
    i915_write32(RING_CTL(ring->mmio_base), ctl);
    i915_posting_read(RING_CTL(ring->mmio_base));

    ring->active = true;
    klog_info("i915: Command Streamer ring %d initialized (MMIO 0x%x, size %u KB, GGTT 0x%lx)",
              ring_id, ring->mmio_base, ring->size / 1024, ring->gtt_offset);

    return 0;
}

int i915_ring_begin(i915_device_t *dev, int ring_id, uint32_t num_dwords) {
    if (!dev || ring_id < 0 || ring_id >= I915_NUM_RINGS)
        return -1;

    i915_ring_t *ring = &dev->rings[ring_id];
    if (!ring->active)
        return -1;

    uint32_t bytes_needed = num_dwords * 4;
    ring->head = i915_read32(RING_HEAD(ring->mmio_base)) & 0x001FFFFC;

    int wait_iters = 100000;
    while (wait_iters-- > 0) {
        uint32_t head = ring->head;
        uint32_t tail = ring->tail;
        uint32_t free_space = (head > tail) ? (head - tail - 8) : (ring->size - tail + head - 8);

        if (free_space >= bytes_needed)
            return 0;

        ring->head = i915_read32(RING_HEAD(ring->mmio_base)) & 0x001FFFFC;
    }

    klog_warn("i915: Ring %d wait timeout (tail=%u, head=%u)", ring_id, ring->tail, ring->head);
    return -1;
}

void i915_ring_emit(i915_device_t *dev, int ring_id, uint32_t dword) {
    if (!dev || ring_id < 0 || ring_id >= I915_NUM_RINGS)
        return;

    i915_ring_t *ring = &dev->rings[ring_id];
    uint32_t *ring_buf = (uint32_t *)ring->vaddr;

    ring_buf[ring->tail / 4] = dword;
    ring->tail = (ring->tail + 4) % ring->size;
}

void i915_ring_advance(i915_device_t *dev, int ring_id) {
    if (!dev || ring_id < 0 || ring_id >= I915_NUM_RINGS)
        return;

    i915_ring_t *ring = &dev->rings[ring_id];

    /* Ensure memory write is committed to RAM/coherent cache */
    __asm__ volatile("mfence" ::: "memory");

    /* Update hardware tail pointer */
    i915_write32(RING_TAIL(ring->mmio_base), ring->tail);
    i915_posting_read(RING_TAIL(ring->mmio_base));
}

uint32_t i915_ring_emit_seqno(i915_device_t *dev, int ring_id) {
    if (!dev || ring_id < 0 || ring_id >= I915_NUM_RINGS)
        return 0;

    i915_ring_t *ring = &dev->rings[ring_id];
    uint32_t seqno = ++ring->last_seqno;

    if (i915_ring_begin(dev, ring_id, 4) != 0)
        return seqno;

    i915_ring_emit(dev, ring_id, MI_STORE_DWORD_INDEX | MI_STORE_DWORD_INDEX_USE_GGTT);
    i915_ring_emit(dev, ring_id, I915_GEM_HWS_INDEX << 2);
    i915_ring_emit(dev, ring_id, seqno);
    i915_ring_emit(dev, ring_id, MI_USER_INTERRUPT);

    i915_ring_advance(dev, ring_id);
    return seqno;
}

int i915_ring_sync(i915_device_t *dev, int ring_id, uint32_t seqno, uint64_t timeout_ns) {
    if (!dev || ring_id < 0 || ring_id >= I915_NUM_RINGS)
        return -1;

    i915_ring_t *ring = &dev->rings[ring_id];
    (void)timeout_ns;

    for (int i = 0; i < 50000; i++) {
        if (ring->hws_vaddr && ring->hws_vaddr[I915_GEM_HWS_INDEX] >= seqno)
            return 0;

        uint32_t head = i915_read32(RING_HEAD(ring->mmio_base)) & 0x001FFFFC;
        uint32_t tail = i915_read32(RING_TAIL(ring->mmio_base)) & 0x001FFFFC;
        if (head == tail)
            return 0;
    }

    return 0;
}
