/*
 * SzpontOS - Intel i915 GEM ExecBuffer2 Batch Submission & Relocation Processing
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/i915/i915_drv.h>
#include <drivers/i915/i915_reg.h>
#include <mm/usercopy.h>
#include <mm/heap.h>
#include <kernel/kprint.h>
#include <kernel/string.h>

#define MAX_EXEC_OBJECTS 256
#define MAX_RELOCS_PER_OBJ 1024

int i915_execbuf(i915_device_t *dev, struct drm_i915_gem_execbuffer2 *eb, struct drm_i915_gem_exec_object2 *exec_objs) {
    if (!dev || !eb || !exec_objs || eb->buffer_count == 0 || eb->buffer_count > MAX_EXEC_OBJECTS)
        return -1;

    /* Select destination ring buffer */
    int ring_id = I915_RING_RCS;
    uint32_t ring_flag = eb->flags & I915_EXEC_RING_MASK;
    if (ring_flag == I915_EXEC_BLT) {
        ring_id = I915_RING_BCS;
    } else if (ring_flag == I915_EXEC_BSD) {
        ring_id = I915_RING_VCS;
    }

    i915_ring_t *ring = &dev->rings[ring_id];
    if (!ring->active) {
        /* If BCS/VCS not initialized, fallback to RCS */
        ring_id = I915_RING_RCS;
        ring = &dev->rings[ring_id];
        if (!ring->active)
            return -1;
    }

    /* Process relocations for each exec object */
    for (uint32_t i = 0; i < eb->buffer_count; i++) {
        struct drm_i915_gem_exec_object2 *obj = &exec_objs[i];
        i915_gem_bo_t *bo = i915_gem_find(dev, obj->handle);
        if (!bo) {
            klog_warn("i915_execbuf: Invalid BO handle %u at index %u", obj->handle, i);
            return -1;
        }

        /* Update object offset to its real GGTT offset */
        obj->offset = bo->gtt_offset;

        if (obj->relocation_count > 0 && obj->relocs_ptr) {
            if (obj->relocation_count > MAX_RELOCS_PER_OBJ)
                return -1;

            size_t reloc_bytes = obj->relocation_count * sizeof(struct drm_i915_gem_relocation_entry);
            struct drm_i915_gem_relocation_entry *relocs = (struct drm_i915_gem_relocation_entry *)kmalloc(reloc_bytes);
            if (!relocs)
                return -1;

            if (!copy_from_user(relocs, (uintptr_t)obj->relocs_ptr, reloc_bytes)) {
                kfree(relocs);
                return -1;
            }

            bool patched = false;
            for (uint32_t r = 0; r < obj->relocation_count; r++) {
                struct drm_i915_gem_relocation_entry *reloc = &relocs[r];
                uint32_t target_handle;
                if (eb->flags & I915_EXEC_HANDLE_LUT) {
                    if (reloc->target_handle >= eb->buffer_count)
                        continue;
                    target_handle = exec_objs[reloc->target_handle].handle;
                } else {
                    target_handle = reloc->target_handle;
                }

                i915_gem_bo_t *target_bo = i915_gem_find(dev, target_handle);
                if (!target_bo)
                    continue;

                uint64_t target_addr = target_bo->gtt_offset + reloc->delta;

                /* Apply relocation patch if target offset doesn't match presumed offset */
                if (reloc->presumed_offset != target_bo->gtt_offset || (reloc->write_domain != 0)) {
                    if (dev->is_64bit_gtt) {
                        if (reloc->offset + 8 <= bo->size && bo->vaddr) {
                            uint64_t *patch_ptr = (uint64_t *)((uint8_t *)bo->vaddr + reloc->offset);
                            *patch_ptr = target_addr;
                            reloc->presumed_offset = target_bo->gtt_offset;
                            patched = true;
                        }
                    } else {
                        if (reloc->offset + 4 <= bo->size && bo->vaddr) {
                            uint32_t *patch_ptr = (uint32_t *)((uint8_t *)bo->vaddr + reloc->offset);
                            *patch_ptr = (uint32_t)target_addr;
                            reloc->presumed_offset = target_bo->gtt_offset;
                            patched = true;
                        }
                    }
                }
            }

            if (patched && bo->vaddr) {
                copy_to_user((uintptr_t)obj->relocs_ptr, relocs, reloc_bytes);
                uint8_t *p = (uint8_t *)bo->vaddr;
                for (size_t off = 0; off < bo->size; off += 64) {
                    __asm__ volatile("clflush (%0)" : : "r"(p + off) : "memory");
                }
            }

            kfree(relocs);
        }
    }

    /* Identify batch buffer: default is last object in array */
    uint32_t batch_idx = (eb->flags & I915_EXEC_BATCH_FIRST) ? 0 : (eb->buffer_count - 1);
    i915_gem_bo_t *batch_bo = i915_gem_find(dev, exec_objs[batch_idx].handle);
    if (!batch_bo)
        return -1;

    /* Flush batch buffer cache to main memory so CS sees fresh commands */
    if (batch_bo->vaddr) {
        uint8_t *bp = (uint8_t *)batch_bo->vaddr;
        for (size_t off = 0; off < batch_bo->size; off += 64) {
            __asm__ volatile("clflush (%0)" : : "r"(bp + off) : "memory");
        }
    }

    uint64_t batch_start = batch_bo->gtt_offset + eb->batch_start_offset;

    /* Emit Batch Buffer Start Command into selected ring */
    if (dev->is_64bit_gtt) {
        if (i915_ring_begin(dev, ring_id, 4) != 0)
            return -1;

        i915_ring_emit(dev, ring_id, MI_BATCH_BUFFER_START_GEN8);
        i915_ring_emit(dev, ring_id, (uint32_t)(batch_start & 0xFFFFFFFFULL));
        i915_ring_emit(dev, ring_id, (uint32_t)(batch_start >> 32));
        i915_ring_emit(dev, ring_id, MI_NOOP);
    } else {
        if (i915_ring_begin(dev, ring_id, 4) != 0)
            return -1;

        i915_ring_emit(dev, ring_id, MI_BATCH_BUFFER_START | (1 << 0));
        i915_ring_emit(dev, ring_id, (uint32_t)(batch_start & 0xFFFFFFFFULL));
        i915_ring_emit(dev, ring_id, MI_NOOP);
        i915_ring_emit(dev, ring_id, MI_NOOP);
    }

    i915_ring_advance(dev, ring_id);

    /* Emit sequence number fence */
    uint32_t seqno = i915_ring_emit_seqno(dev, ring_id);
    (void)seqno;

    return 0;
}
