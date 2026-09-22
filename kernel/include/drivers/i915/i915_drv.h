/*
 * SzpontOS - Intel i915 DRM/KMS Graphics Driver Main Header
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_I915_DRV_H
#define SZPONTOS_DRIVERS_I915_DRV_H

#include <kernel/types.h>
#include <kernel/spinlock.h>
#include <drivers/pci.h>
#include <drm/drm.h>
#include <drm/i915_drm.h>
#include <drivers/i915/i915_pciids.h>
#include <drivers/i915/i915_reg.h>

#define I915_RING_RCS          0   /* Render Command Streamer (3D / Compute) */
#define I915_RING_BCS          1   /* Blitter Command Streamer (2D / Blt) */
#define I915_RING_VCS          2   /* Video Command Streamer */
#define I915_NUM_RINGS         3

#define I915_MAX_BOS           512
#define I915_RING_BUFFER_SIZE  (128 * 1024) /* 128 KiB */

/*
 * Ring buffer state representation
 */
typedef struct i915_ring {
    uint32_t mmio_base;
    void *vaddr;
    uintptr_t paddr;
    uint32_t size;
    uint32_t head;
    uint32_t tail;
    uint32_t space;
    uint32_t *hws_vaddr;
    uintptr_t hws_paddr;
    uint64_t gtt_offset;
    uint32_t last_seqno;
    bool active;
} i915_ring_t;

/*
 * Graphics Execution Manager (GEM) Buffer Object
 */
typedef struct i915_gem_bo {
    uint32_t handle;
    uint64_t size;
    uint64_t gtt_offset;
    uintptr_t *pages;
    size_t num_pages;
    void *vaddr;
    uint32_t tiling_mode;
    uint32_t stride;
    uint32_t read_domains;
    uint32_t write_domain;
    uint32_t cache_level;
    uint64_t mmap_offset;
    bool is_dumb;
    bool is_batch;
    int refcount;
} i915_gem_bo_t;

/*
 * Master Intel GPU Device State
 */
typedef struct i915_device {
    pci_device_t *pci_dev;
    uint16_t device_id;
    intel_gen_t gen;
    const char *name;

    /* BAR 0: Registers & GTT PTEs */
    uintptr_t mmio_paddr;
    size_t mmio_size;
    volatile uint8_t *mmio_base;

    /* BAR 2: Aperture / Direct VRAM access */
    uintptr_t aperture_paddr;
    size_t aperture_size;
    uint8_t *aperture_base;

    /* Global GTT (GGTT) */
    uint64_t gtt_total_size;
    uint64_t gtt_stolen_size;
    uint32_t gtt_pte_offset;
    bool is_64bit_gtt;
    uint64_t next_gtt_alloc;

    /* Command Streamers */
    i915_ring_t rings[I915_NUM_RINGS];

    /* GEM Buffer Objects */
    i915_gem_bo_t *bos[I915_MAX_BOS];
    size_t num_bos;
    uint32_t next_bo_handle;

    /* Lock & Status */
    spinlock_t lock;
    bool active;

    /* Display Subsystem */
    uint32_t display_width;
    uint32_t display_height;
    uint32_t display_bpp;
    uint32_t display_pitch;
    uint32_t current_fb_gtt_offset;
} i915_device_t;

/* =========================================================================
 * Core Driver Prototypes
 * ========================================================================= */
bool i915_pci_probe(pci_device_t *pci_dev);
bool i915_init(pci_device_t *pci_dev);
bool i915_is_active(void);
i915_device_t *i915_get_device(void);

uint32_t i915_read32(uint32_t reg);
void i915_write32(uint32_t reg, uint32_t val);
void i915_posting_read(uint32_t reg);

/* =========================================================================
 * Global GTT (GGTT) Prototypes
 * ========================================================================= */
int i915_gtt_init(i915_device_t *dev);
int64_t i915_gtt_alloc(i915_device_t *dev, size_t size, size_t alignment);
void i915_gtt_free(i915_device_t *dev, uint64_t offset, size_t size);
int i915_gtt_bind_pages(i915_device_t *dev, uint64_t offset, uintptr_t *pages, size_t num_pages, uint32_t cache_level);
void i915_gtt_unbind(i915_device_t *dev, uint64_t offset, size_t num_pages);
void i915_gtt_chipset_flush(i915_device_t *dev);

/* =========================================================================
 * GEM Subsystem Prototypes
 * ========================================================================= */
int i915_gem_init(i915_device_t *dev);
i915_gem_bo_t *i915_gem_create(i915_device_t *dev, size_t size);
i915_gem_bo_t *i915_gem_find(i915_device_t *dev, uint32_t handle);
void i915_gem_destroy(i915_device_t *dev, uint32_t handle);
int i915_gem_mmap_gtt(i915_device_t *dev, uint32_t handle, uint64_t *offset_out);
int i915_gem_set_tiling(i915_device_t *dev, struct drm_i915_gem_set_tiling *args);
int i915_gem_get_tiling(i915_device_t *dev, struct drm_i915_gem_get_tiling *args);
int i915_gem_set_domain(i915_device_t *dev, struct drm_i915_gem_set_domain *args);
int i915_gem_wait(i915_device_t *dev, struct drm_i915_gem_wait *args);
int i915_gem_busy(i915_device_t *dev, struct drm_i915_gem_busy *args);
int i915_gem_madvise(i915_device_t *dev, struct drm_i915_gem_madvise *args);
int i915_gem_register_dumb_bo(i915_device_t *dev, uint32_t handle, size_t size, uintptr_t *pages, size_t num_pages, void *vaddr, uint64_t gtt_offset, uint64_t mmap_offset, uint32_t cache_level);
void i915_gem_unregister_dumb_bo(i915_device_t *dev, uint32_t handle);
int i915_gem_mmap_user(i915_gem_bo_t *bo, void *addr, size_t length, void **out_vaddr);
int i915_gem_mmap_user_full(i915_gem_bo_t *bo, void *addr, size_t length, uint64_t offset, uint64_t flags, void **out_vaddr);

/* =========================================================================
 * Ring Buffer & Command Streamer Prototypes
 * ========================================================================= */
int i915_ring_init(i915_device_t *dev, int ring_id);
int i915_ring_begin(i915_device_t *dev, int ring_id, uint32_t num_dwords);
void i915_ring_emit(i915_device_t *dev, int ring_id, uint32_t dword);
void i915_ring_advance(i915_device_t *dev, int ring_id);
uint32_t i915_ring_emit_seqno(i915_device_t *dev, int ring_id);
int i915_ring_sync(i915_device_t *dev, int ring_id, uint32_t seqno, uint64_t timeout_ns);

/* =========================================================================
 * ExecBuffer & Batch Submission Prototypes
 * ========================================================================= */
int i915_execbuf(i915_device_t *dev, struct drm_i915_gem_execbuffer2 *eb, struct drm_i915_gem_exec_object2 *exec_objs);

/* =========================================================================
 * Display & Modesetting Prototypes
 * ========================================================================= */
int i915_display_init(i915_device_t *dev);
void i915_display_blit(const uint32_t *src, size_t pitch_pixels, size_t dst_x, size_t dst_y, size_t w, size_t h);
void i915_get_resolution(uint32_t *w, uint32_t *h);
int i915_display_set_mode(i915_device_t *dev, uint32_t width, uint32_t height, uint32_t gtt_offset, uint32_t pitch);
int i915_display_page_flip(i915_device_t *dev, uint32_t gtt_offset);

/* =========================================================================
 * Top-Level DRM IOCTL Dispatcher
 * ========================================================================= */
int i915_ioctl_dispatch(uint32_t cmd, void *arg);

#endif /* SZPONTOS_DRIVERS_I915_DRV_H */
