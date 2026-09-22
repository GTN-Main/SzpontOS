/*
 * SzpontOS - Intel i915 DRM IOCTL Dispatcher
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/i915/i915_drv.h>
#include <mm/usercopy.h>
#include <mm/heap.h>
#include <kernel/kprint.h>
#include <kernel/string.h>

static int i915_ioctl_getparam(i915_device_t *dev, struct drm_i915_getparam *param) {
    if (!dev || !param || !param->value)
        return -1;

    int value = 0;
    switch (param->param) {
        case I915_PARAM_CHIPSET_ID:
            if (dev->gen >= INTEL_GEN9) {
                value = 0x22b0; /* Alias Gen9+ (Skylake/Kaby Lake) as Gen8 Cherryview for Mesa crocus */
            } else {
                value = (int)dev->device_id;
            }
            break;
        case I915_PARAM_HAS_GEM:
        case I915_PARAM_HAS_EXECBUF2:
        case I915_PARAM_HAS_PINNED_BATCHES:
        case I915_PARAM_HAS_EXEC_CONSTANTS:
        case I915_PARAM_HAS_RELAXED_DELTA:
        case I915_PARAM_HAS_GEN7_SOL_RESET:
        case I915_PARAM_HAS_WAIT_TIMEOUT:
        case I915_PARAM_HAS_SEMAPHORES:
        case I915_PARAM_HAS_PRIME_VMAP_FLUSH:
        case I915_PARAM_HAS_PINNED_CONTEXTS:
        case I915_PARAM_HAS_EXEC_NO_RELOC:
        case I915_PARAM_HAS_EXEC_HANDLE_LUT:
        case I915_PARAM_HAS_EXEC_SOFTPIN:
        case I915_PARAM_HAS_EXEC_FENCE:
        case I915_PARAM_HAS_EXEC_CAPTURE:
        case I915_PARAM_HAS_EXEC_BATCH_FIRST:
        case I915_PARAM_HAS_EXEC_FENCE_ARRAY:
        case I915_PARAM_HAS_CONTEXT_ISOLATION:
        case I915_PARAM_MMAP_GTT_VERSION:
        case I915_PARAM_MMAP_VERSION:
            value = 1;
            break;
        case I915_PARAM_NUM_FENCES_AVAIL:
            value = 16;
            break;
        case I915_PARAM_HAS_LLC:
            value = (dev->gen >= INTEL_GEN6) ? 1 : 0;
            break;
        case I915_PARAM_HAS_ALIASING_PPGTT:
            value = (dev->gen >= INTEL_GEN6) ? 1 : 0;
            break;
        case I915_PARAM_CMD_PARSER_VERSION:
            value = 10;
            break;
        case I915_PARAM_SUBSLICE_TOTAL:
            value = 2;
            break;
        case I915_PARAM_SLICE_MASK:
            value = 1;
            break;
        case I915_PARAM_SUBSLICE_MASK:
            value = 0x3; /* 2 subslices */
            break;
        case I915_PARAM_EU_TOTAL:
            value = (dev->gen >= INTEL_GEN8) ? 24 : 16;
            break;
        case I915_PARAM_REVISION:
            value = 0;
            break;
        case I915_PARAM_HAS_USERPTR_PROBE:
            value = 0;
            break;
        case I915_PARAM_EXEC_ASYNC:
            value = 1;
            break;
        case I915_PARAM_CS_TIMESTAMP_FREQUENCY:
            value = 12500000; /* 12.5 MHz */
            break;
        default:
            return -1;
    }

    if (!copy_to_user((uintptr_t)param->value, &value, sizeof(int)))
        return -1;

    return 0;
}

int i915_ioctl_dispatch(uint32_t cmd, void *arg) {
    i915_device_t *dev = i915_get_device();
    if (!dev)
        return -1;

    switch (cmd) {
        case DRM_IOCTL_I915_GETPARAM: {
            struct drm_i915_getparam *param = (struct drm_i915_getparam *)arg;
            return i915_ioctl_getparam(dev, param);
        }

        case DRM_IOCTL_I915_SETPARAM:
            return 0;

        case DRM_IOCTL_I915_GEM_CREATE: {
            struct drm_i915_gem_create *create = (struct drm_i915_gem_create *)arg;
            if (!create || create->size == 0)
                return -1;

            i915_gem_bo_t *bo = i915_gem_create(dev, create->size);
            if (!bo)
                return -1;

            create->handle = bo->handle;
            create->size = bo->size;
            return 0;
        }

        case DRM_IOCTL_I915_GEM_PREAD: {
            struct drm_i915_gem_pread *pread = (struct drm_i915_gem_pread *)arg;
            if (!pread || pread->size == 0)
                return -1;

            i915_gem_bo_t *bo = i915_gem_find(dev, pread->handle);
            if (!bo || (pread->offset + pread->size > bo->size) || !bo->vaddr)
                return -1;

            if (!copy_to_user((uintptr_t)pread->data_ptr, (uint8_t *)bo->vaddr + pread->offset, pread->size))
                return -1;
            return 0;
        }

        case DRM_IOCTL_I915_GEM_PWRITE: {
            struct drm_i915_gem_pwrite *pwrite = (struct drm_i915_gem_pwrite *)arg;
            if (!pwrite || pwrite->size == 0)
                return -1;

            i915_gem_bo_t *bo = i915_gem_find(dev, pwrite->handle);
            if (!bo || (pwrite->offset + pwrite->size > bo->size) || !bo->vaddr)
                return -1;

            if (!copy_from_user((uint8_t *)bo->vaddr + pwrite->offset, (uintptr_t)pwrite->data_ptr, pwrite->size))
                return -1;

            __asm__ volatile("clflushopt (%0)" : : "r"((uint8_t *)bo->vaddr + pwrite->offset) : "memory");
            return 0;
        }

        case DRM_IOCTL_I915_GEM_MMAP: {
            struct drm_i915_gem_mmap *m = (struct drm_i915_gem_mmap *)arg;
            if (!m || m->handle == 0 || m->size == 0)
                return -22;

            i915_gem_bo_t *bo = i915_gem_find(dev, m->handle);
            if (!bo)
                return -2; /* -ENOENT */

            void *vaddr = NULL;
            int ret = i915_gem_mmap_user_full(bo, NULL, m->size, m->offset, m->flags, &vaddr);
            if (ret != 0)
                return ret;

            m->addr_ptr = (uint64_t)(uintptr_t)vaddr;
            return 0;
        }

        case DRM_IOCTL_I915_GEM_MMAP_OFFSET: {
            struct drm_i915_gem_mmap_offset *mmo = (struct drm_i915_gem_mmap_offset *)arg;
            if (!mmo || mmo->handle == 0)
                return -22;

            i915_gem_bo_t *bo = i915_gem_find(dev, mmo->handle);
            if (!bo)
                return -2;

            mmo->offset = bo->mmap_offset;
            return 0;
        }

        case DRM_IOCTL_I915_GEM_MMAP_GTT: {
            struct drm_i915_gem_mmap_gtt *mmap_gtt = (struct drm_i915_gem_mmap_gtt *)arg;
            if (!mmap_gtt)
                return -1;
            return i915_gem_mmap_gtt(dev, mmap_gtt->handle, &mmap_gtt->offset);
        }

        case DRM_IOCTL_I915_GEM_SET_TILING:
            return i915_gem_set_tiling(dev, (struct drm_i915_gem_set_tiling *)arg);

        case DRM_IOCTL_I915_GEM_GET_TILING:
            return i915_gem_get_tiling(dev, (struct drm_i915_gem_get_tiling *)arg);

        case DRM_IOCTL_I915_GEM_GET_APERTURE: {
            struct drm_i915_gem_get_aperture *aper = (struct drm_i915_gem_get_aperture *)arg;
            if (!aper)
                return -1;
            aper->aper_size = dev->aperture_size;
            aper->aper_available_size = (dev->aperture_size > dev->next_gtt_alloc) ?
                                        (dev->aperture_size - dev->next_gtt_alloc) : (64 * 1024 * 1024);
            return 0;
        }

        case DRM_IOCTL_I915_GEM_SET_DOMAIN:
            return i915_gem_set_domain(dev, (struct drm_i915_gem_set_domain *)arg);

        case DRM_IOCTL_I915_GEM_BUSY:
            return i915_gem_busy(dev, (struct drm_i915_gem_busy *)arg);

        case DRM_IOCTL_I915_GEM_WAIT:
            return i915_gem_wait(dev, (struct drm_i915_gem_wait *)arg);

        case DRM_IOCTL_I915_GEM_MADVISE:
            return i915_gem_madvise(dev, (struct drm_i915_gem_madvise *)arg);

        case DRM_IOCTL_I915_GEM_SET_CACHING: {
            struct drm_i915_gem_caching *c = (struct drm_i915_gem_caching *)arg;
            if (!c || c->handle == 0)
                return -22;

            i915_gem_bo_t *bo = i915_gem_find(dev, c->handle);
            if (!bo)
                return -2;

            bo->cache_level = c->caching;
            return 0;
        }

        case DRM_IOCTL_I915_GEM_GET_CACHING: {
            struct drm_i915_gem_caching *c = (struct drm_i915_gem_caching *)arg;
            if (!c || c->handle == 0)
                return -22;

            i915_gem_bo_t *bo = i915_gem_find(dev, c->handle);
            if (!bo)
                return -2;

            c->caching = bo->cache_level;
            return 0;
        }

        case DRM_IOCTL_I915_GEM_CONTEXT_CREATE: {
            struct drm_i915_gem_context_create *ctx = (struct drm_i915_gem_context_create *)arg;
            if (!ctx)
                return -1;
            ctx->ctx_id = 1;
            return 0;
        }

        case DRM_IOCTL_I915_GEM_CONTEXT_DESTROY:
            return 0;

        case DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM: {
            struct drm_i915_gem_context_param *cp = (struct drm_i915_gem_context_param *)arg;
            if (!cp)
                return -22;

            switch (cp->param) {
                case I915_CONTEXT_PARAM_GTT_SIZE:
                    cp->value = dev->gtt_total_size ? dev->gtt_total_size : (1ULL << 32);
                    break;
                default:
                    cp->value = 0;
                    break;
            }
            return 0;
        }

        case DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM:
            return 0;

        case DRM_IOCTL_I915_QUERY:
            return -95; /* -EOPNOTSUPP */

        case DRM_IOCTL_I915_GEM_EXECBUFFER2: {
            struct drm_i915_gem_execbuffer2 *eb = (struct drm_i915_gem_execbuffer2 *)arg;
            if (!eb || eb->buffer_count == 0 || eb->buffer_count > 256 || !eb->buffers_ptr)
                return -1;

            size_t objs_size = eb->buffer_count * sizeof(struct drm_i915_gem_exec_object2);
            struct drm_i915_gem_exec_object2 *exec_objs = (struct drm_i915_gem_exec_object2 *)kmalloc(objs_size);
            if (!exec_objs)
                return -1;

            if (!copy_from_user(exec_objs, (uintptr_t)eb->buffers_ptr, objs_size)) {
                kfree(exec_objs);
                return -1;
            }

            int ret = i915_execbuf(dev, eb, exec_objs);

            /* Copy back updated offsets to userspace */
            copy_to_user((uintptr_t)eb->buffers_ptr, exec_objs, objs_size);
            kfree(exec_objs);

            return ret;
        }

        default:
            klog_warn("i915: Unknown or unhandled IOCTL 0x%08x", cmd);
            return -1;
    }
}
