/*
 * SzpontOS - Intel i915 Graphics Execution Manager (GEM) Buffer Management
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/i915/i915_drv.h>
#include <mm/heap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <sched/sched.h>
#include <sched/process.h>
#include <arch/x86_64/io.h>
#include <kernel/kprint.h>
#include <kernel/string.h>

int i915_gem_init(i915_device_t *dev) {
    if (!dev)
        return -1;

    spinlock_acquire(&dev->lock);
    memset(dev->bos, 0, sizeof(dev->bos));
    dev->num_bos = 0;
    dev->next_bo_handle = 1000;
    spinlock_release(&dev->lock);

    klog_info("i915: GEM subsystem initialized");
    return 0;
}

i915_gem_bo_t *i915_gem_create(i915_device_t *dev, size_t size) {
    if (!dev || size == 0)
        return NULL;

    size_t aligned_size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    size_t num_pages = aligned_size / PAGE_SIZE;

    i915_gem_bo_t *bo = (i915_gem_bo_t *)kmalloc(sizeof(i915_gem_bo_t));
    if (!bo)
        return NULL;
    memset(bo, 0, sizeof(i915_gem_bo_t));

    bo->pages = (uintptr_t *)kmalloc(num_pages * sizeof(uintptr_t));
    if (!bo->pages) {
        kfree(bo);
        return NULL;
    }

    /* Allocate physical pages */
    uintptr_t contig_phys = pmm_alloc_pages(num_pages);
    if (contig_phys) {
        for (size_t i = 0; i < num_pages; i++) {
            bo->pages[i] = contig_phys + i * PAGE_SIZE;
        }
        bo->vaddr = (void *)PHYS_TO_VIRT(contig_phys);
    } else {
        for (size_t i = 0; i < num_pages; i++) {
            bo->pages[i] = pmm_alloc_page();
            if (!bo->pages[i]) {
                for (size_t j = 0; j < i; j++)
                    pmm_free_page(bo->pages[j]);
                kfree(bo->pages);
                kfree(bo);
                return NULL;
            }
        }
        bo->vaddr = (void *)PHYS_TO_VIRT(bo->pages[0]);
    }

    /* Allocate address space in Global GTT */
    int64_t gtt_off = i915_gtt_alloc(dev, aligned_size, PAGE_SIZE);
    if (gtt_off < 0) {
        for (size_t i = 0; i < num_pages; i++)
            pmm_free_page(bo->pages[i]);
        kfree(bo->pages);
        kfree(bo);
        return NULL;
    }

    bo->gtt_offset = (uint64_t)gtt_off;
    bo->size = aligned_size;
    bo->num_pages = num_pages;
    bo->tiling_mode = I915_TILING_NONE;
    bo->cache_level = I915_CACHE_LLC;
    bo->refcount = 1;

    /* Bind to GGTT */
    i915_gtt_bind_pages(dev, bo->gtt_offset, bo->pages, bo->num_pages, bo->cache_level);

    /* If Aperture is present, point vaddr directly into the hardware aperture */
    if (dev->aperture_base && (bo->gtt_offset + bo->size <= dev->aperture_size)) {
        bo->vaddr = (void *)(dev->aperture_base + bo->gtt_offset);
    }

    spinlock_acquire(&dev->lock);
    bo->handle = dev->next_bo_handle++;
    bo->mmap_offset = 0x200000000ULL + ((uint64_t)bo->handle << 16);

    for (size_t i = 0; i < I915_MAX_BOS; i++) {
        if (!dev->bos[i]) {
            dev->bos[i] = bo;
            dev->num_bos++;
            break;
        }
    }
    spinlock_release(&dev->lock);

    return bo;
}

i915_gem_bo_t *i915_gem_find(i915_device_t *dev, uint32_t handle) {
    if (!dev || handle == 0)
        return NULL;

    spinlock_acquire(&dev->lock);
    for (size_t i = 0; i < I915_MAX_BOS; i++) {
        if (dev->bos[i] && dev->bos[i]->handle == handle) {
            spinlock_release(&dev->lock);
            return dev->bos[i];
        }
    }
    spinlock_release(&dev->lock);
    return NULL;
}

void i915_gem_destroy(i915_device_t *dev, uint32_t handle) {
    if (!dev || handle == 0)
        return;

    spinlock_acquire(&dev->lock);
    for (size_t i = 0; i < I915_MAX_BOS; i++) {
        if (dev->bos[i] && dev->bos[i]->handle == handle) {
            i915_gem_bo_t *bo = dev->bos[i];
            bo->refcount--;
            if (bo->refcount <= 0) {
                dev->bos[i] = NULL;
                dev->num_bos--;
                spinlock_release(&dev->lock);

                i915_gtt_unbind(dev, bo->gtt_offset, bo->num_pages);
                i915_gtt_free(dev, bo->gtt_offset, bo->size);

                for (size_t p = 0; p < bo->num_pages; p++) {
                    if (bo->pages[p])
                        pmm_free_page(bo->pages[p]);
                }
                kfree(bo->pages);
                kfree(bo);
                return;
            }
            spinlock_release(&dev->lock);
            return;
        }
    }
    spinlock_release(&dev->lock);
}

int i915_gem_mmap_gtt(i915_device_t *dev, uint32_t handle, uint64_t *offset_out) {
    i915_gem_bo_t *bo = i915_gem_find(dev, handle);
    if (!bo)
        return -1;
    *offset_out = bo->mmap_offset;
    return 0;
}

int i915_gem_set_tiling(i915_device_t *dev, struct drm_i915_gem_set_tiling *args) {
    if (!args)
        return -1;
    i915_gem_bo_t *bo = i915_gem_find(dev, args->handle);
    if (!bo)
        return -1;

    bo->tiling_mode = args->tiling_mode;
    bo->stride = args->stride;
    args->swizzle_mode = I915_BIT_6_SWIZZLE_NONE;
    return 0;
}

int i915_gem_get_tiling(i915_device_t *dev, struct drm_i915_gem_get_tiling *args) {
    if (!args)
        return -1;
    i915_gem_bo_t *bo = i915_gem_find(dev, args->handle);
    if (!bo)
        return -1;

    args->tiling_mode = bo->tiling_mode;
    args->swizzle_mode = I915_BIT_6_SWIZZLE_NONE;
    args->phys_swizzle_mode = I915_BIT_6_SWIZZLE_NONE;
    return 0;
}

int i915_gem_set_domain(i915_device_t *dev, struct drm_i915_gem_set_domain *args) {
    if (!args)
        return -1;
    i915_gem_bo_t *bo = i915_gem_find(dev, args->handle);
    if (!bo)
        return -1;

    bo->read_domains = args->read_domains;
    bo->write_domain = args->write_domain;

    /* Flush cache if transitioning domains across all buffer cachelines */
    if (args->read_domains & I915_GEM_DOMAIN_CPU || args->write_domain & I915_GEM_DOMAIN_CPU) {
        if (bo->vaddr && bo->size > 0) {
            uint8_t *ptr = (uint8_t *)bo->vaddr;
            for (size_t off = 0; off < bo->size; off += 64) {
                __asm__ volatile("clflushopt (%0)" : : "r"(ptr + off) : "memory");
            }
            __asm__ volatile("mfence" ::: "memory");
        }
    }

    return 0;
}

int i915_gem_wait(i915_device_t *dev, struct drm_i915_gem_wait *args) {
    (void)dev;
    (void)args;
    /* In immediate submission architecture with fast completion, wait returns 0 */
    return 0;
}

int i915_gem_busy(i915_device_t *dev, struct drm_i915_gem_busy *args) {
    (void)dev;
    if (!args)
        return -1;
    /* Returns 0 when buffer is idle */
    args->busy = 0;
    return 0;
}

int i915_gem_madvise(i915_device_t *dev, struct drm_i915_gem_madvise *args) {
    (void)dev;
    if (!args)
        return -1;
    args->retained = 1;
    return 0;
}

int i915_gem_register_dumb_bo(i915_device_t *dev, uint32_t handle, size_t size, uintptr_t *pages, size_t num_pages, void *vaddr, uint64_t gtt_offset, uint64_t mmap_offset, uint32_t cache_level) {
    if (!dev || handle == 0)
        return -1;

    i915_gem_bo_t *bo = (i915_gem_bo_t *)kmalloc(sizeof(i915_gem_bo_t));
    if (!bo)
        return -1;
    memset(bo, 0, sizeof(i915_gem_bo_t));

    bo->handle = handle;
    bo->size = size;
    bo->pages = pages;
    bo->num_pages = num_pages;
    bo->vaddr = vaddr;
    bo->gtt_offset = gtt_offset;
    bo->mmap_offset = mmap_offset;
    bo->tiling_mode = I915_TILING_NONE;
    bo->cache_level = cache_level;
    bo->refcount = 1;

    spinlock_acquire(&dev->lock);
    for (size_t i = 0; i < I915_MAX_BOS; i++) {
        if (!dev->bos[i]) {
            dev->bos[i] = bo;
            dev->num_bos++;
            break;
        }
    }
    spinlock_release(&dev->lock);
    return 0;
}

void i915_gem_unregister_dumb_bo(i915_device_t *dev, uint32_t handle) {
    if (!dev || handle == 0)
        return;

    spinlock_acquire(&dev->lock);
    for (size_t i = 0; i < I915_MAX_BOS; i++) {
        if (dev->bos[i] && dev->bos[i]->handle == handle) {
            i915_gem_bo_t *bo = dev->bos[i];
            dev->bos[i] = NULL;
            dev->num_bos--;
            kfree(bo);
            break;
        }
    }
    spinlock_release(&dev->lock);
}

int i915_gem_mmap_user_full(i915_gem_bo_t *bo, void *addr, size_t length, uint64_t offset, uint64_t flags, void **out_vaddr) {
    if (!bo || !out_vaddr || length == 0)
        return -22;

    process_t *proc = sched_get_current_process();
    if (!proc || length > VMM_USER_END - PAGE_SIZE)
        return -22;

    if (proc->mmap_current == 0) {
        proc->mmap_current = 0x0000600000000000ULL;
    }

    size_t start_page = (size_t)(offset / PAGE_SIZE);
    if (start_page >= bo->num_pages)
        return -22;

    size_t pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    if (start_page + pages > bo->num_pages)
        pages = bo->num_pages - start_page;

    uintptr_t vaddr = (uintptr_t)addr;
    if (vaddr == 0) {
        vaddr = proc->mmap_current;
    }
    if ((vaddr & (PAGE_SIZE - 1)) || !vmm_user_range(vaddr, pages * PAGE_SIZE)) {
        return -22;
    }

    uint64_t map_flags = VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER | VMM_FLAG_BORROWED;
    if (bo->cache_level == I915_CACHE_NONE || (flags & 1) /* I915_MMAP_WC */) {
        map_flags |= VMM_FLAG_WRITE_COMBINING;
    }

    for (size_t i = 0; i < pages; i++) {
        uintptr_t phys = bo->pages[start_page + i];
        vmm_release_user_page(proc->pagemap, vaddr + i * PAGE_SIZE);
        if (!vmm_map_page(proc->pagemap, vaddr + i * PAGE_SIZE, phys, map_flags)) {
            for (size_t j = 0; j < i; j++)
                vmm_release_user_page(proc->pagemap, vaddr + j * PAGE_SIZE);
            return -12;
        }
    }

    if (vaddr + pages * PAGE_SIZE > proc->mmap_current)
        proc->mmap_current = vaddr + pages * PAGE_SIZE;

    if (proc == sched_get_current_process()) {
        write_cr3(read_cr3());
    }

    *out_vaddr = (void *)vaddr;
    return 0;
}

int i915_gem_mmap_user(i915_gem_bo_t *bo, void *addr, size_t length, void **out_vaddr) {
    return i915_gem_mmap_user_full(bo, addr, length, 0, 0, out_vaddr);
}
