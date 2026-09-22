/*
 * SzpontOS - DRM/KMS Kernel Driver Implementation
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/drm.h>
#include <drivers/framebuffer.h>
#include <drivers/virtio_gpu.h>
#include <drivers/i915/i915_drv.h>
#include <drivers/rtc.h>
#include <mm/heap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/usercopy.h>
#include <sched/process.h>
#include <sched/sched.h>
#include <arch/x86_64/io.h>
#include <kernel/string.h>
#include <kernel/kprint.h>
#include <kernel/spinlock.h>

static inline void drm_blit_to_screen(const uint32_t *src, size_t pitch_pixels, size_t dst_x, size_t dst_y, size_t w, size_t h) {
    if (i915_is_active()) {
        i915_display_blit(src, pitch_pixels, dst_x, dst_y, w, h);
    } else if (virtio_gpu_is_active()) {
        virtio_gpu_blit(src, pitch_pixels, dst_x, dst_y, w, h);
    } else {
        fb_blit_from_buffer(src, pitch_pixels, dst_x, dst_y, w, h);
    }
}

static inline bool drm_copy_to_user(uintptr_t uaddr, const void *kbuf, size_t size) {
    if (uaddr == 0 || size == 0) return true;
    if (uaddr > USER_ADDR_MAX) {
        memcpy((void *)uaddr, kbuf, size);
        return true;
    }
    return copy_to_user(uaddr, kbuf, size);
}

static inline bool drm_copy_from_user(void *kbuf, uintptr_t uaddr, size_t size) {
    if (uaddr == 0 || size == 0) return true;
    if (uaddr > USER_ADDR_MAX) {
        memcpy(kbuf, (const void *)uaddr, size);
        return true;
    }
    return copy_from_user(kbuf, uaddr, size);
}

static spinlock_t g_drm_lock = SPINLOCK_INIT;
static drm_dumb_bo_t g_dumb_buffers[DRM_MAX_DUMB_BUFFERS];
static drm_fb_t g_framebuffers[DRM_MAX_FBS];

static drm_syncobj_t g_syncobjs[DRM_MAX_SYNCOBJS];
static uint32_t g_next_syncobj_handle = 1;

static drm_event_queue_t g_drm_events;
static uint32_t g_vblank_sequence = 1;

static vfs_ops_t g_dmabuf_ops;
static vfs_ops_t g_syncfile_ops;

static drm_crtc_state_t g_crtc;
static drm_connector_state_t g_connector;
static drm_encoder_state_t g_encoder;
static struct drm_mode_modeinfo g_active_mode;

static pid_t g_drm_master_pid = 0;
static bool g_drm_initialized = false;

static uint32_t g_next_bo_handle = 1;
static uint32_t g_next_fb_id = 100;

static void drm_setup_default_mode(void) {
    size_t width = 0;
    size_t height = 0;

    if (i915_is_active()) {
        uint32_t iw = 0, ih = 0;
        i915_get_resolution(&iw, &ih);
        width = iw;
        height = ih;
    } else if (virtio_gpu_is_active()) {
        uint32_t vw = 0, vh = 0;
        virtio_gpu_get_resolution(&vw, &vh);
        width = vw;
        height = vh;
    } else {
        width = fb_get_width();
        height = fb_get_height();
    }

    if (width == 0 || height == 0) {
        width = 1280;
        height = 960;
    }

    memset(&g_active_mode, 0, sizeof(struct drm_mode_modeinfo));
    g_active_mode.clock = 60000;
    g_active_mode.hdisplay = (uint16_t)width;
    g_active_mode.hsync_start = (uint16_t)(width + 16);
    g_active_mode.hsync_end = (uint16_t)(width + 32);
    g_active_mode.htotal = (uint16_t)(width + 48);
    g_active_mode.vdisplay = (uint16_t)height;
    g_active_mode.vsync_start = (uint16_t)(height + 1);
    g_active_mode.vsync_end = (uint16_t)(height + 3);
    g_active_mode.vtotal = (uint16_t)(height + 5);
    g_active_mode.vrefresh = 60;
    g_active_mode.type = DRM_MODE_TYPE_PREFERRED;
    ksnprintf(g_active_mode.name, sizeof(g_active_mode.name), "%lux%lu", width, height);

    g_connector.connector_id = 1;
    g_connector.connector_type = DRM_MODE_CONNECTOR_VIRTUAL;
    g_connector.connection = DRM_MODE_CONNECTED;
    g_connector.encoder_id = 2;
    g_connector.mm_width = (uint32_t)(width * 254 / 960);
    g_connector.mm_height = (uint32_t)(height * 254 / 960);

    g_encoder.encoder_id = 2;
    g_encoder.encoder_type = 1; /* DRM_MODE_ENCODER_NONE / DAC */
    g_encoder.crtc_id = 3;
    g_encoder.possible_crtcs = 1;

    g_crtc.crtc_id = 3;
    g_crtc.fb_id = 0;
    g_crtc.x = 0;
    g_crtc.y = 0;
    g_crtc.mode_valid = true;
    memcpy(&g_crtc.mode, &g_active_mode, sizeof(struct drm_mode_modeinfo));
}

void drm_init(void) {
    spinlock_acquire(&g_drm_lock);
    memset(g_dumb_buffers, 0, sizeof(g_dumb_buffers));
    memset(g_framebuffers, 0, sizeof(g_framebuffers));
    memset(g_syncobjs, 0, sizeof(g_syncobjs));
    memset(&g_drm_events, 0, sizeof(g_drm_events));

    drm_setup_default_mode();
    g_drm_initialized = true;
    spinlock_release(&g_drm_lock);

    if (i915_is_active()) {
        klog_info("DRM/KMS: Initialized with Intel i915 hardware backend (Mode: %s, Connector ID: %u, CRTC ID: %u)",
                  g_active_mode.name, g_connector.connector_id, g_crtc.crtc_id);
    } else if (virtio_gpu_is_active()) {
        klog_info("DRM/KMS: Initialized with VirtIO-GPU hardware backend (Mode: %s, Connector ID: %u, CRTC ID: %u)",
                  g_active_mode.name, g_connector.connector_id, g_crtc.crtc_id);
    } else {
        klog_info("DRM/KMS: Initialized with Limine Framebuffer fallback (Mode: %s, Connector ID: %u, CRTC ID: %u)",
                  g_active_mode.name, g_connector.connector_id, g_crtc.crtc_id);
    }
}

static drm_dumb_bo_t *drm_find_bo(uint32_t handle) {
    if (handle == 0)
        return NULL;
    for (size_t i = 0; i < DRM_MAX_DUMB_BUFFERS; i++) {
        if (g_dumb_buffers[i].allocated && g_dumb_buffers[i].handle == handle) {
            return &g_dumb_buffers[i];
        }
    }

    /* Check if this handle belongs to an i915 hardware GEM buffer */
    if (i915_is_active()) {
        i915_device_t *idev = i915_get_device();
        if (idev) {
            i915_gem_bo_t *gbo = i915_gem_find(idev, handle);
            if (gbo) {
                for (size_t i = 0; i < DRM_MAX_DUMB_BUFFERS; i++) {
                    if (g_dumb_buffers[i].allocated && g_dumb_buffers[i].handle == handle) {
                        return &g_dumb_buffers[i];
                    }
                }
                for (size_t i = 0; i < DRM_MAX_DUMB_BUFFERS; i++) {
                    if (!g_dumb_buffers[i].allocated) {
                        drm_dumb_bo_t *bo = &g_dumb_buffers[i];
                        memset(bo, 0, sizeof(drm_dumb_bo_t));
                        bo->handle = gbo->handle;
                        bo->size = gbo->size;
                        bo->num_pages = gbo->num_pages;
                        bo->phys_pages = gbo->pages;
                        bo->kernel_virt = gbo->vaddr;
                        bo->pitch = gbo->stride ? gbo->stride : 0;
                        bo->mmap_offset = gbo->mmap_offset;
                        bo->i915_gtt_offset = gbo->gtt_offset;
                        bo->refcount = 1;
                        bo->allocated = true;
                        bo->is_direct_vram = false;
                        bo->is_gem_wrapper = true;
                        return bo;
                    }
                }
            }
        }
    }

    return NULL;
}

static drm_fb_t *drm_find_fb(uint32_t fb_id) {
    if (fb_id == 0)
        return NULL;
    for (size_t i = 0; i < DRM_MAX_FBS; i++) {
        if (g_framebuffers[i].allocated && g_framebuffers[i].fb_id == fb_id) {
            return &g_framebuffers[i];
        }
    }
    return NULL;
}

static int drm_ioctl_version(struct drm_version *user_ver) {
    if (!user_ver)
        return -22; /* EINVAL */

    struct drm_version ver;
    if (!drm_copy_from_user(&ver, (uintptr_t)user_ver, sizeof(ver)))
        return -14;

    const char *name;
    const char *date = "20260830";
    const char *desc;

    if (i915_is_active()) {
        ver.version_major = 1;
        ver.version_minor = 6;
        ver.version_patchlevel = 0;
        name = "i915";
        desc = "Intel Graphics";
        date = "20201103";
    } else if (virtio_gpu_is_active()) {
        ver.version_major = 0;
        ver.version_minor = 0; /* Important: 0 avoids fence FD path in Mesa virgl */
        ver.version_patchlevel = 0;
        name = "virtio_gpu";
        desc = "SzpontOS VirtIO-GPU DRM Driver";
    } else {
        ver.version_major = 1;
        ver.version_minor = 0;
        ver.version_patchlevel = 0;
        name = "szpont-drm";
        desc = "SzpontOS Kernel Mode Setting & DRM Driver";
    }

    if (ver.name && ver.name_len > 0) {
        size_t n = strlen(name);
        if (n > ver.name_len)
            n = ver.name_len;
        drm_copy_to_user((uintptr_t)ver.name, name, n);
        if (n < ver.name_len) {
            char zero = '\0';
            drm_copy_to_user((uintptr_t)ver.name + n, &zero, 1);
        }
    }
    ver.name_len = strlen(name);

    if (ver.date && ver.date_len > 0) {
        size_t n = strlen(date);
        if (n > ver.date_len)
            n = ver.date_len;
        drm_copy_to_user((uintptr_t)ver.date, date, n);
        if (n < ver.date_len) {
            char zero = '\0';
            drm_copy_to_user((uintptr_t)ver.date + n, &zero, 1);
        }
    }
    ver.date_len = strlen(date);

    if (ver.desc && ver.desc_len > 0) {
        size_t n = strlen(desc);
        if (n > ver.desc_len)
            n = ver.desc_len;
        drm_copy_to_user((uintptr_t)ver.desc, desc, n);
        if (n < ver.desc_len) {
            char zero = '\0';
            drm_copy_to_user((uintptr_t)ver.desc + n, &zero, 1);
        }
    }
    ver.desc_len = strlen(desc);

    if (!drm_copy_to_user((uintptr_t)user_ver, &ver, sizeof(ver)))
        return -14;

    return 0;
}

static int drm_ioctl_get_cap(struct drm_get_cap *user_cap) {
    if (!user_cap)
        return -22;

    struct drm_get_cap cap;
    if (!drm_copy_from_user(&cap, (uintptr_t)user_cap, sizeof(cap)))
        return -14;

    switch (cap.capability) {
    case DRM_CAP_DUMB_BUFFER:
        cap.value = 1;
        break;
    case DRM_CAP_VBLANK_HIGH_CRTC:
        cap.value = 1;
        break;
    case DRM_CAP_DUMB_PREFERRED_DEPTH:
        cap.value = 32;
        break;
    case DRM_CAP_DUMB_PREFER_SHADOW:
        cap.value = 1;
        break;
    case DRM_CAP_PRIME:
        cap.value = DRM_PRIME_CAP_IMPORT | DRM_PRIME_CAP_EXPORT;
        break;
    case DRM_CAP_TIMESTAMP_MONOTONIC:
        cap.value = 1;
        break;
    case DRM_CAP_ASYNC_PAGE_FLIP:
        cap.value = 1;
        break;
    case DRM_CAP_CURSOR_WIDTH:
        cap.value = 64;
        break;
    case DRM_CAP_CURSOR_HEIGHT:
        cap.value = 64;
        break;
    case DRM_CAP_ADDFB2_MODIFIERS:
        cap.value = 1;
        break;
    case DRM_CAP_PAGE_FLIP_TARGET:
        cap.value = 1;
        break;
    case DRM_CAP_CRTC_IN_VBLANK_EVENT:
        cap.value = 1;
        break;
    case DRM_CAP_SYNCOBJ:
        cap.value = 1;
        break;
    case DRM_CAP_SYNCOBJ_TIMELINE:
        cap.value = 0;
        break;
    default:
        cap.value = 0;
        drm_copy_to_user((uintptr_t)user_cap, &cap, sizeof(cap));
        return -22;
    }

    if (!drm_copy_to_user((uintptr_t)user_cap, &cap, sizeof(cap)))
        return -14;

    return 0;
}

static int drm_ioctl_get_resources(struct drm_mode_card_res *user_res) {
    if (!user_res)
        return -22;

    struct drm_mode_card_res res;
    if (!drm_copy_from_user(&res, (uintptr_t)user_res, sizeof(res)))
        return -14;

    res.min_width = 320;
    res.max_width = 3840;
    res.min_height = 200;
    res.max_height = 2160;

    res.count_connectors = 1;
    res.count_encoders = 1;
    res.count_crtcs = 1;

    /* Count allocated framebuffers */
    uint32_t count_fbs = 0;
    for (size_t i = 0; i < DRM_MAX_FBS; i++) {
        if (g_framebuffers[i].allocated)
            count_fbs++;
    }
    res.count_fbs = count_fbs;

    if (res.connector_id_ptr) {
        uint32_t cid = g_connector.connector_id;
        drm_copy_to_user((uintptr_t)res.connector_id_ptr, &cid, sizeof(uint32_t));
    }
    if (res.encoder_id_ptr) {
        uint32_t eid = g_encoder.encoder_id;
        drm_copy_to_user((uintptr_t)res.encoder_id_ptr, &eid, sizeof(uint32_t));
    }
    if (res.crtc_id_ptr) {
        uint32_t crtid = g_crtc.crtc_id;
        drm_copy_to_user((uintptr_t)res.crtc_id_ptr, &crtid, sizeof(uint32_t));
    }
    if (res.fb_id_ptr && count_fbs > 0) {
        uint32_t fb_list[DRM_MAX_FBS];
        size_t idx = 0;
        for (size_t i = 0; i < DRM_MAX_FBS; i++) {
            if (g_framebuffers[i].allocated) {
                fb_list[idx++] = g_framebuffers[i].fb_id;
            }
        }
        drm_copy_to_user((uintptr_t)res.fb_id_ptr, fb_list, sizeof(uint32_t) * idx);
    }

    if (!drm_copy_to_user((uintptr_t)user_res, &res, sizeof(res)))
        return -14;

    return 0;
}

static int drm_ioctl_get_connector(struct drm_mode_get_connector *user_conn) {
    if (!user_conn)
        return -22;

    struct drm_mode_get_connector conn;
    if (!drm_copy_from_user(&conn, (uintptr_t)user_conn, sizeof(conn)))
        return -14;

    conn.connector_id = g_connector.connector_id;
    conn.connector_type = g_connector.connector_type;
    conn.connector_type_id = 1;
    conn.connection = g_connector.connection;
    conn.mm_width = g_connector.mm_width;
    conn.mm_height = g_connector.mm_height;
    conn.subpixel = 0;

    conn.count_encoders = 1;
    conn.count_modes = 1;
    conn.count_props = 0;
    conn.encoder_id = g_connector.encoder_id;

    if (conn.encoders_ptr) {
        uint32_t eid = g_encoder.encoder_id;
        drm_copy_to_user((uintptr_t)conn.encoders_ptr, &eid, sizeof(uint32_t));
    }

    if (conn.modes_ptr && conn.count_modes > 0) {
        drm_copy_to_user((uintptr_t)conn.modes_ptr, &g_active_mode, sizeof(struct drm_mode_modeinfo));
    }

    if (!drm_copy_to_user((uintptr_t)user_conn, &conn, sizeof(conn)))
        return -14;

    return 0;
}

static int drm_ioctl_get_encoder(struct drm_mode_get_encoder *user_enc) {
    if (!user_enc)
        return -22;

    struct drm_mode_get_encoder enc;
    if (!drm_copy_from_user(&enc, (uintptr_t)user_enc, sizeof(enc)))
        return -14;

    enc.encoder_id = g_encoder.encoder_id;
    enc.encoder_type = g_encoder.encoder_type;
    enc.crtc_id = g_encoder.crtc_id;
    enc.possible_crtcs = g_encoder.possible_crtcs;
    enc.possible_clones = 0;

    if (!drm_copy_to_user((uintptr_t)user_enc, &enc, sizeof(enc)))
        return -14;

    return 0;
}

static int drm_ioctl_get_crtc(struct drm_mode_crtc *user_crtc) {
    if (!user_crtc)
        return -22;

    struct drm_mode_crtc crtc;
    if (!drm_copy_from_user(&crtc, (uintptr_t)user_crtc, sizeof(crtc)))
        return -14;

    crtc.crtc_id = g_crtc.crtc_id;
    crtc.fb_id = g_crtc.fb_id;
    crtc.x = g_crtc.x;
    crtc.y = g_crtc.y;
    crtc.gamma_size = 0;
    crtc.mode_valid = g_crtc.mode_valid ? 1 : 0;
    memcpy(&crtc.mode, &g_crtc.mode, sizeof(struct drm_mode_modeinfo));

    if (!drm_copy_to_user((uintptr_t)user_crtc, &crtc, sizeof(crtc)))
        return -14;

    return 0;
}

static int drm_ioctl_set_crtc(struct drm_mode_crtc *user_crtc) {
    if (!user_crtc)
        return -22;

    struct drm_mode_crtc kcrtc;
    if (!drm_copy_from_user(&kcrtc, (uintptr_t)user_crtc, sizeof(kcrtc)))
        return -14;
    struct drm_mode_crtc *crtc = &kcrtc;

    spinlock_acquire(&g_drm_lock);

    g_crtc.fb_id = crtc->fb_id;
    g_crtc.x = crtc->x;
    g_crtc.y = crtc->y;

    if (crtc->mode_valid) {
        memcpy(&g_crtc.mode, &crtc->mode, sizeof(struct drm_mode_modeinfo));
        memcpy(&g_active_mode, &crtc->mode, sizeof(struct drm_mode_modeinfo));
        g_crtc.mode_valid = true;
    }

    /* Activate graphics mode and flush FB */
    fb_set_graphics_mode(true);

    drm_fb_t *fb = drm_find_fb(g_crtc.fb_id);
    if (fb) {
        drm_dumb_bo_t *bo = drm_find_bo(fb->bo_handle);
        if (bo) {
            if (i915_is_active() && bo->i915_gtt_offset) {
                i915_display_set_mode(i915_get_device(), bo->width, bo->height, (uint32_t)bo->i915_gtt_offset, bo->pitch);
            }
            if (bo->kernel_virt && !bo->is_direct_vram) {
                uint32_t w = bo->width ? bo->width : fb_get_width();
                uint32_t h = bo->height ? bo->height : fb_get_height();
                uint32_t stride_pixels = bo->pitch ? (bo->pitch / 4) : fb_get_width();
                drm_blit_to_screen((const uint32_t *)bo->kernel_virt, stride_pixels, 0, 0, w, h);
            }
        }
    }

    spinlock_release(&g_drm_lock);
    return 0;
}

static int drm_ioctl_create_dumb(struct drm_mode_create_dumb *user_req) {
    if (!user_req)
        return -22;

    struct drm_mode_create_dumb req;
    if (!drm_copy_from_user(&req, (uintptr_t)user_req, sizeof(req)))
        return -14;

    if (req.width == 0 || req.height == 0)
        return -22;

    if (req.bpp == 0)
        req.bpp = 32;

    uint32_t pitch = ALIGN_UP(req.width * ((req.bpp + 7) / 8), 64);
    size_t size = ALIGN_UP((size_t)pitch * req.height, PAGE_SIZE);
    size_t num_pages = size / PAGE_SIZE;

    spinlock_acquire(&g_drm_lock);

    drm_dumb_bo_t *bo = NULL;
    for (size_t i = 0; i < DRM_MAX_DUMB_BUFFERS; i++) {
        if (!g_dumb_buffers[i].allocated) {
            bo = &g_dumb_buffers[i];
            break;
        }
    }

    if (!bo) {
        spinlock_release(&g_drm_lock);
        return -12; /* ENOMEM */
    }

    struct limine_framebuffer *lfb = fb_get_limine();
    bool is_direct = false;
    uintptr_t phys = 0;
    void *virt = NULL;

    bool direct_already_claimed = false;
    for (size_t i = 0; i < DRM_MAX_DUMB_BUFFERS; i++) {
        if (g_dumb_buffers[i].allocated && g_dumb_buffers[i].is_direct_vram) {
            direct_already_claimed = true;
            break;
        }
    }

    if (!i915_is_active() && !virtio_gpu_is_active() && !direct_already_claimed && lfb && lfb->address && req.width == fb_get_width() && req.height == fb_get_height()) {
        uintptr_t fb_virt = (uintptr_t)lfb->address;
        uintptr_t fb_phys = vmm_virt_to_phys(&g_kernel_pagemap, fb_virt);
        if (!fb_phys) {
            fb_phys = VIRT_TO_PHYS(fb_virt);
        }
        phys = fb_phys;
        virt = (void *)fb_virt;
        pitch = (uint32_t)lfb->pitch;
        size = ALIGN_UP((size_t)pitch * req.height, PAGE_SIZE);
        num_pages = size / PAGE_SIZE;
        is_direct = true;
    } else {
        phys = (uintptr_t)pmm_alloc_pages(num_pages);
        if (!phys) {
            spinlock_release(&g_drm_lock);
            return -12; /* ENOMEM */
        }
        virt = (void *)PHYS_TO_VIRT(phys);
        memset(virt, 0, size);
    }

    bo->handle = g_next_bo_handle++;
    bo->width = req.width;
    bo->height = req.height;
    bo->bpp = req.bpp;
    bo->pitch = pitch;
    bo->size = size;
    bo->num_pages = num_pages;
    bo->phys_pages = (uintptr_t *)kmalloc(sizeof(uintptr_t) * num_pages);
    for (size_t i = 0; i < num_pages; i++) {
        bo->phys_pages[i] = phys + i * PAGE_SIZE;
    }
    bo->kernel_virt = virt;
    bo->mmap_offset = ((uint64_t)bo->handle) << 12;
    bo->refcount = 1;
    bo->allocated = true;
    bo->is_direct_vram = is_direct;

    if (i915_is_active()) {
        i915_device_t *idev = i915_get_device();
        if (idev) {
            int64_t gtt_off = i915_gtt_alloc(idev, size, 4096);
            if (gtt_off >= 0) {
                bo->i915_gtt_offset = (uint64_t)gtt_off;
                uint32_t cache_mode = bo->is_direct_vram ? I915_CACHE_NONE : I915_CACHE_LLC;
                i915_gtt_bind_pages(idev, bo->i915_gtt_offset, bo->phys_pages, num_pages, cache_mode);
                i915_gem_register_dumb_bo(idev, bo->handle, size, bo->phys_pages, num_pages, bo->kernel_virt, bo->i915_gtt_offset, bo->mmap_offset, cache_mode);
            }
        }
    }

    req.handle = bo->handle;
    req.pitch = pitch;
    req.size = size;

    spinlock_release(&g_drm_lock);

    if (!drm_copy_to_user((uintptr_t)user_req, &req, sizeof(req)))
        return -14;

    return 0;
}

static int drm_ioctl_map_dumb(struct drm_mode_map_dumb *user_req) {
    if (!user_req)
        return -22;

    struct drm_mode_map_dumb req;
    if (!drm_copy_from_user(&req, (uintptr_t)user_req, sizeof(req)))
        return -14;

    if (req.handle == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = drm_find_bo(req.handle);
    if (!bo) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    req.offset = bo->mmap_offset;
    spinlock_release(&g_drm_lock);

    if (!drm_copy_to_user((uintptr_t)user_req, &req, sizeof(req)))
        return -14;

    return 0;
}

static int drm_bo_unref_locked(drm_dumb_bo_t *bo) {
    if (!bo || !bo->allocated)
        return -22;

    bo->refcount--;
    if (bo->refcount <= 0) {
        if (bo->is_gem_wrapper) {
            if (i915_is_active()) {
                i915_device_t *idev = i915_get_device();
                if (idev) {
                    i915_gem_destroy(idev, bo->handle);
                }
            }
            memset(bo, 0, sizeof(drm_dumb_bo_t));
            return 0;
        }

        if (bo->i915_gtt_offset && i915_is_active()) {
            i915_device_t *idev = i915_get_device();
            if (idev) {
                i915_gtt_unbind(idev, bo->i915_gtt_offset, bo->num_pages);
                i915_gtt_free(idev, bo->i915_gtt_offset, bo->size);
                i915_gem_unregister_dumb_bo(idev, bo->handle);
            }
            bo->i915_gtt_offset = 0;
        }
        if (bo->phys_pages) {
            if (!bo->is_direct_vram && bo->phys_pages[0]) {
                pmm_free_pages(bo->phys_pages[0], bo->num_pages);
            }
            kfree(bo->phys_pages);
        }
        memset(bo, 0, sizeof(drm_dumb_bo_t));
    }
    return 0;
}

static int drm_ioctl_destroy_dumb(struct drm_mode_destroy_dumb *user_req) {
    if (!user_req)
        return -22;

    struct drm_mode_destroy_dumb req;
    if (!drm_copy_from_user(&req, (uintptr_t)user_req, sizeof(req)))
        return -14;

    if (req.handle == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = drm_find_bo(req.handle);
    if (!bo) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    int ret = drm_bo_unref_locked(bo);
    spinlock_release(&g_drm_lock);
    return ret;
}

static int drm_ioctl_gem_close(struct drm_gem_close *user_req) {
    if (!user_req)
        return -22;

    struct drm_gem_close req;
    if (!drm_copy_from_user(&req, (uintptr_t)user_req, sizeof(req)))
        return -14;

    if (req.handle == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = drm_find_bo(req.handle);
    if (!bo) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    int ret = drm_bo_unref_locked(bo);
    spinlock_release(&g_drm_lock);
    return ret;
}

static int drm_ioctl_add_fb(struct drm_mode_fb_cmd *user_cmd) {
    if (!user_cmd)
        return -22;

    struct drm_mode_fb_cmd cmd;
    if (!drm_copy_from_user(&cmd, (uintptr_t)user_cmd, sizeof(cmd)))
        return -14;

    if (cmd.handle == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = drm_find_bo(cmd.handle);
    if (!bo) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    drm_fb_t *fb = NULL;
    for (size_t i = 0; i < DRM_MAX_FBS; i++) {
        if (!g_framebuffers[i].allocated) {
            fb = &g_framebuffers[i];
            break;
        }
    }

    if (!fb) {
        spinlock_release(&g_drm_lock);
        return -12;
    }

    fb->fb_id = g_next_fb_id++;
    fb->width = cmd.width;
    fb->height = cmd.height;
    fb->pitch = cmd.pitch;
    fb->bpp = cmd.bpp;
    fb->depth = cmd.depth;
    fb->pixel_format = 0;
    fb->bo_handle = cmd.handle;
    fb->allocated = true;

    if (bo->width == 0) bo->width = cmd.width;
    if (bo->height == 0) bo->height = cmd.height;
    if (bo->pitch == 0) bo->pitch = cmd.pitch;
    if (bo->bpp == 0) bo->bpp = cmd.bpp;
    if (bo->is_gem_wrapper && i915_is_active()) {
        i915_device_t *idev = i915_get_device();
        if (idev) {
            i915_gem_bo_t *gbo = i915_gem_find(idev, bo->handle);
            if (gbo && !gbo->stride) gbo->stride = bo->pitch;
        }
    }

    cmd.fb_id = fb->fb_id;
    spinlock_release(&g_drm_lock);

    if (!drm_copy_to_user((uintptr_t)user_cmd, &cmd, sizeof(cmd)))
        return -14;

    return 0;
}

static int drm_ioctl_add_fb2(struct drm_mode_fb_cmd2 *user_cmd) {
    if (!user_cmd)
        return -22;

    struct drm_mode_fb_cmd2 cmd;
    if (!drm_copy_from_user(&cmd, (uintptr_t)user_cmd, sizeof(cmd)))
        return -14;

    if (cmd.handles[0] == 0 || cmd.width == 0 || cmd.height == 0)
        return -22;

    uint32_t handle = cmd.handles[0];
    uint32_t pitch = cmd.pitches[0];
    uint32_t pixel_format = cmd.pixel_format;

    uint32_t bpp = 32;
    uint32_t depth = 24;
    if (pixel_format == DRM_FORMAT_RGB565 || pixel_format == DRM_FORMAT_BGR565) {
        bpp = 16;
        depth = 16;
    } else if (pixel_format == DRM_FORMAT_ARGB8888 || pixel_format == DRM_FORMAT_ABGR8888) {
        bpp = 32;
        depth = 32;
    } else if (pixel_format == DRM_FORMAT_XRGB8888 || pixel_format == DRM_FORMAT_XBGR8888 || pixel_format == 0) {
        bpp = 32;
        depth = 24;
    }

    if (pitch == 0) {
        pitch = ALIGN_UP(cmd.width * ((bpp + 7) / 8), 64);
    }

    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = drm_find_bo(handle);
    if (!bo) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    drm_fb_t *fb = NULL;
    for (size_t i = 0; i < DRM_MAX_FBS; i++) {
        if (!g_framebuffers[i].allocated) {
            fb = &g_framebuffers[i];
            break;
        }
    }

    if (!fb) {
        spinlock_release(&g_drm_lock);
        return -12;
    }

    fb->fb_id = g_next_fb_id++;
    fb->width = cmd.width;
    fb->height = cmd.height;
    fb->pitch = pitch;
    fb->bpp = bpp;
    fb->depth = depth;
    fb->pixel_format = pixel_format;
    fb->bo_handle = handle;
    fb->allocated = true;

    if (bo->width == 0) bo->width = cmd.width;
    if (bo->height == 0) bo->height = cmd.height;
    if (bo->pitch == 0) bo->pitch = pitch;
    if (bo->bpp == 0) bo->bpp = bpp;
    if (bo->is_gem_wrapper && i915_is_active()) {
        i915_device_t *idev = i915_get_device();
        if (idev) {
            i915_gem_bo_t *gbo = i915_gem_find(idev, bo->handle);
            if (gbo && !gbo->stride) gbo->stride = bo->pitch;
        }
    }

    cmd.fb_id = fb->fb_id;
    spinlock_release(&g_drm_lock);

    if (!drm_copy_to_user((uintptr_t)user_cmd, &cmd, sizeof(cmd)))
        return -14;

    return 0;
}

static int drm_ioctl_rm_fb(uint32_t *user_fb_id_ptr) {
    if (!user_fb_id_ptr)
        return -22;

    uint32_t fb_id = 0;
    if (!drm_copy_from_user(&fb_id, (uintptr_t)user_fb_id_ptr, sizeof(uint32_t)))
        return -14;

    if (fb_id == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_fb_t *fb = drm_find_fb(fb_id);
    if (!fb) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    if (g_crtc.fb_id == fb->fb_id) {
        g_crtc.fb_id = 0;
    }
    memset(fb, 0, sizeof(drm_fb_t));

    spinlock_release(&g_drm_lock);
    return 0;
}

static int drm_ioctl_dirty_fb(struct drm_mode_fb_dirty_cmd *user_dirty) {
    if (!user_dirty)
        return -22;

    struct drm_mode_fb_dirty_cmd kdirty;
    if (!drm_copy_from_user(&kdirty, (uintptr_t)user_dirty, sizeof(kdirty)))
        return -14;
    struct drm_mode_fb_dirty_cmd *dirty = &kdirty;
    if (dirty->fb_id == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_fb_t *fb = drm_find_fb(dirty->fb_id);
    if (!fb) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    drm_dumb_bo_t *bo = drm_find_bo(fb->bo_handle);
    if (!bo || !bo->kernel_virt) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    if (!bo->is_direct_vram) {
        uint32_t bo_w = bo->width ? bo->width : fb_get_width();
        uint32_t bo_h = bo->height ? bo->height : fb_get_height();
        uint32_t bo_stride = bo->pitch ? (bo->pitch / 4) : fb_get_width();

        if (dirty->num_clips == 0 || dirty->clips_ptr == 0) {
            /* Blit full buffer to screen */
            drm_blit_to_screen((const uint32_t *)bo->kernel_virt, bo_stride, 0, 0, bo_w, bo_h);
        } else {
            /* Blit clipped damage rectangles safely copied from user memory */
            uint32_t num_clips = dirty->num_clips;
            if (num_clips > 256) num_clips = 256;
            struct drm_clip_rect clips_buf[16];
            struct drm_clip_rect *clips = clips_buf;
            bool allocated = false;
            if (num_clips > 16) {
                clips = (struct drm_clip_rect *)kmalloc(num_clips * sizeof(struct drm_clip_rect));
                allocated = true;
            }
            bool copy_ok = (clips != NULL) && drm_copy_from_user(clips, (uintptr_t)dirty->clips_ptr, num_clips * sizeof(struct drm_clip_rect));
            if (copy_ok) {
                for (uint32_t i = 0; i < num_clips; i++) {
                    size_t x = clips[i].x1;
                    size_t y = clips[i].y1;
                    size_t w = (clips[i].x2 > clips[i].x1) ? (clips[i].x2 - clips[i].x1) : 0;
                    size_t h = (clips[i].y2 > clips[i].y1) ? (clips[i].y2 - clips[i].y1) : 0;
                    if (x < bo_w && y < bo_h && w > 0 && h > 0) {
                        if (x + w > bo_w) w = bo_w - x;
                        if (y + h > bo_h) h = bo_h - y;
                        drm_blit_to_screen((const uint32_t *)bo->kernel_virt, bo_stride, x, y, w, h);
                    }
                }
            } else {
                /* If copy_from_user or allocation failed, fall back to full screen blit */
                drm_blit_to_screen((const uint32_t *)bo->kernel_virt, bo_stride, 0, 0, bo_w, bo_h);
            }
            if (allocated) {
                kfree(clips);
            }
        }
    }

    spinlock_release(&g_drm_lock);
    return 0;
}

static void drm_queue_flip_event(uint32_t crtc_id, uint64_t user_data) {
    if (g_drm_events.count >= DRM_MAX_EVENTS) {
        g_drm_events.head = (g_drm_events.head + 1) % DRM_MAX_EVENTS;
        g_drm_events.count--;
    }

    uint64_t now_ns = rtc_get_monotonic_ns();
    uint32_t sec = (uint32_t)(now_ns / 1000000000ULL);
    uint32_t usec = (uint32_t)((now_ns % 1000000000ULL) / 1000);

    struct drm_event_vblank *ev = &g_drm_events.events[g_drm_events.tail];
    ev->base.type = DRM_EVENT_FLIP_COMPLETE;
    ev->base.length = sizeof(struct drm_event_vblank);
    ev->user_data = user_data;
    ev->tv_sec = sec;
    ev->tv_usec = usec;
    ev->sequence = g_vblank_sequence++;
    ev->crtc_id = crtc_id;

    g_drm_events.tail = (g_drm_events.tail + 1) % DRM_MAX_EVENTS;
    g_drm_events.count++;
}

static int drm_ioctl_page_flip(struct drm_mode_crtc_page_flip *user_flip) {
    if (!user_flip)
        return -22;

    struct drm_mode_crtc_page_flip kflip;
    if (!drm_copy_from_user(&kflip, (uintptr_t)user_flip, sizeof(kflip)))
        return -14;
    struct drm_mode_crtc_page_flip *flip = &kflip;
    if (flip->crtc_id == 0 || flip->fb_id == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);

    drm_fb_t *fb = drm_find_fb(flip->fb_id);
    if (!fb) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    drm_dumb_bo_t *bo = drm_find_bo(fb->bo_handle);
    if (!bo || !bo->kernel_virt) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    /* Atomically update CRTC framebuffer scanout ID */
    g_crtc.fb_id = flip->fb_id;

    /* If i915 hardware GTT scanout is active, update device state */
    if (i915_is_active() && bo->i915_gtt_offset) {
        i915_display_page_flip(i915_get_device(), (uint32_t)bo->i915_gtt_offset);
    }
    if (!bo->is_direct_vram) {
        uint32_t w = bo->width ? bo->width : fb_get_width();
        uint32_t h = bo->height ? bo->height : fb_get_height();
        uint32_t stride_pixels = bo->pitch ? (bo->pitch / 4) : fb_get_width();
        drm_blit_to_screen((const uint32_t *)bo->kernel_virt, stride_pixels, 0, 0, w, h);
    }

    if (flip->flags & DRM_MODE_PAGE_FLIP_EVENT) {
        drm_queue_flip_event(flip->crtc_id, flip->user_data);
    }

    spinlock_release(&g_drm_lock);
    return 0;
}

static int drm_ioctl_get_plane_res(struct drm_mode_get_plane_res *user_res) {
    if (!user_res)
        return -22;

    struct drm_mode_get_plane_res res;
    if (!drm_copy_from_user(&res, (uintptr_t)user_res, sizeof(res)))
        return -14;

    if (res.count_planes == 0 || res.plane_id_ptr == 0) {
        res.count_planes = 1;
        if (!drm_copy_to_user((uintptr_t)user_res, &res, sizeof(res)))
            return -14;
        return 0;
    }

    uint32_t plane_id = 1;
    res.count_planes = 1;
    if (!drm_copy_to_user((uintptr_t)res.plane_id_ptr, &plane_id, sizeof(uint32_t)))
        return -14;
    if (!drm_copy_to_user((uintptr_t)user_res, &res, sizeof(res)))
        return -14;
    return 0;
}

static int drm_ioctl_get_plane(struct drm_mode_get_plane *user_p) {
    if (!user_p)
        return -22;

    struct drm_mode_get_plane p;
    if (!drm_copy_from_user(&p, (uintptr_t)user_p, sizeof(p)))
        return -14;

    p.plane_id = 1;
    p.crtc_id = g_crtc.crtc_id;
    p.fb_id = g_crtc.fb_id;
    p.possible_crtcs = 1;
    p.gamma_size = 0;

    if (p.count_format_types == 0 || p.format_type_ptr == 0) {
        p.count_format_types = 2;
        if (!drm_copy_to_user((uintptr_t)user_p, &p, sizeof(p)))
            return -14;
        return 0;
    }

    uint32_t formats[2] = {DRM_FORMAT_XRGB8888, DRM_FORMAT_ARGB8888};
    p.count_format_types = 2;
    if (!drm_copy_to_user((uintptr_t)p.format_type_ptr, formats, sizeof(formats)))
        return -14;
    if (!drm_copy_to_user((uintptr_t)user_p, &p, sizeof(p)))
        return -14;
    return 0;
}

static int drm_ioctl_set_plane(struct drm_mode_set_plane *user_plane) {
    if (!user_plane)
        return -22;

    struct drm_mode_set_plane kplane;
    if (!drm_copy_from_user(&kplane, (uintptr_t)user_plane, sizeof(kplane)))
        return -14;
    struct drm_mode_set_plane *plane = &kplane;

    spinlock_acquire(&g_drm_lock);

    if (plane->fb_id != 0) {
        drm_fb_t *fb = drm_find_fb(plane->fb_id);
        if (!fb) {
            spinlock_release(&g_drm_lock);
            return -22;
        }

        g_crtc.fb_id = plane->fb_id;
        g_crtc.x = plane->crtc_x;
        g_crtc.y = plane->crtc_y;

        fb_set_graphics_mode(true);

        drm_dumb_bo_t *bo = drm_find_bo(fb->bo_handle);
        if (bo && bo->kernel_virt) {
            if (!bo->is_direct_vram) {
                uint32_t src_w = plane->src_w >> 16;
                uint32_t src_h = plane->src_h >> 16;
                if (src_w == 0) src_w = bo->width;
                if (src_h == 0) src_h = bo->height;

                uint32_t crtc_w = plane->crtc_w ? plane->crtc_w : src_w;
                uint32_t crtc_h = plane->crtc_h ? plane->crtc_h : src_h;

                uint32_t plane_stride = bo->pitch ? (bo->pitch / 4) : fb_get_width();
                drm_blit_to_screen((const uint32_t *)bo->kernel_virt, plane_stride,
                                   plane->crtc_x, plane->crtc_y,
                                   crtc_w, crtc_h);
            }
        }
    } else {
        g_crtc.fb_id = 0;
    }

    spinlock_release(&g_drm_lock);
    return 0;
}

static int drm_ioctl_obj_get_properties(struct drm_mode_obj_get_properties *user_p) {
    if (!user_p)
        return -22;
    struct drm_mode_obj_get_properties p;
    if (!drm_copy_from_user(&p, (uintptr_t)user_p, sizeof(p)))
        return -14;
    p.count_props = 0;
    if (!drm_copy_to_user((uintptr_t)user_p, &p, sizeof(p)))
        return -14;
    return 0;
}

static int drm_ioctl_obj_set_property(struct drm_mode_obj_set_property *p) {
    if (!p)
        return -22;
    return 0;
}

static int drm_ioctl_cursor(struct drm_mode_cursor *c) {
    if (!c)
        return -22;
    /* -ENXIO informs Xorg modesetting driver to fall back to software cursor */
    return -6;
}

static int drm_ioctl_cursor2(struct drm_mode_cursor2 *c) {
    if (!c)
        return -22;
    return -6;
}

static int drm_ioctl_atomic(struct drm_mode_atomic *atom) {
    if (!atom)
        return -22;
    return 0;
}

#define DRM_MAX_BLOBS 64

typedef struct {
    uint32_t blob_id;
    uint32_t length;
    void *data;
    bool allocated;
} drm_blob_t;

static drm_blob_t g_drm_blobs[DRM_MAX_BLOBS];
static uint32_t g_next_blob_id = 1;

static int drm_ioctl_create_blob(struct drm_mode_create_blob *user_blob) {
    if (!user_blob)
        return -22;

    struct drm_mode_create_blob blob;
    if (!drm_copy_from_user(&blob, (uintptr_t)user_blob, sizeof(blob)))
        return -14;

    if (blob.length == 0 || blob.data == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_blob_t *slot = NULL;
    for (size_t i = 0; i < DRM_MAX_BLOBS; i++) {
        if (!g_drm_blobs[i].allocated) {
            slot = &g_drm_blobs[i];
            break;
        }
    }
    if (!slot) {
        spinlock_release(&g_drm_lock);
        return -12; /* ENOMEM */
    }

    void *kdata = kmalloc(blob.length);
    if (!kdata) {
        spinlock_release(&g_drm_lock);
        return -12;
    }

    if (!drm_copy_from_user(kdata, (uintptr_t)blob.data, blob.length)) {
        kfree(kdata);
        spinlock_release(&g_drm_lock);
        return -14;
    }

    slot->blob_id = g_next_blob_id++;
    slot->length = blob.length;
    slot->data = kdata;
    slot->allocated = true;
    blob.blob_id = slot->blob_id;

    spinlock_release(&g_drm_lock);

    if (!drm_copy_to_user((uintptr_t)user_blob, &blob, sizeof(blob)))
        return -14;
    return 0;
}

static int drm_ioctl_get_prop_blob(struct drm_mode_get_blob *user_blob) {
    if (!user_blob)
        return -22;

    struct drm_mode_get_blob blob;
    if (!drm_copy_from_user(&blob, (uintptr_t)user_blob, sizeof(blob)))
        return -14;

    if (blob.blob_id == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_blob_t *found = NULL;
    for (size_t i = 0; i < DRM_MAX_BLOBS; i++) {
        if (g_drm_blobs[i].allocated && g_drm_blobs[i].blob_id == blob.blob_id) {
            found = &g_drm_blobs[i];
            break;
        }
    }
    if (!found) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    if (blob.data != 0 && blob.length >= found->length) {
        if (!drm_copy_to_user((uintptr_t)blob.data, found->data, found->length)) {
            spinlock_release(&g_drm_lock);
            return -14;
        }
    }
    blob.length = found->length;
    spinlock_release(&g_drm_lock);

    if (!drm_copy_to_user((uintptr_t)user_blob, &blob, sizeof(blob)))
        return -14;
    return 0;
}

static int drm_ioctl_destroy_blob(struct drm_mode_destroy_blob *user_blob) {
    if (!user_blob)
        return -22;

    struct drm_mode_destroy_blob blob;
    if (!drm_copy_from_user(&blob, (uintptr_t)user_blob, sizeof(blob)))
        return -14;

    if (blob.blob_id == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    for (size_t i = 0; i < DRM_MAX_BLOBS; i++) {
        if (g_drm_blobs[i].allocated && g_drm_blobs[i].blob_id == blob.blob_id) {
            if (g_drm_blobs[i].data) {
                kfree(g_drm_blobs[i].data);
            }
            memset(&g_drm_blobs[i], 0, sizeof(drm_blob_t));
            spinlock_release(&g_drm_lock);
            return 0;
        }
    }
    spinlock_release(&g_drm_lock);
    return -22;
}

static int drm_ioctl_set_gamma(struct drm_mode_crtc_lut *lut) {
    if (!lut)
        return -22;
    return 0;
}

static int drm_ioctl_get_property(struct drm_mode_get_property *p) {
    if (!p)
        return -22;
    return -22;
}

static int drm_ioctl_set_property(struct drm_mode_connector_set_property *p) {
    if (!p)
        return -22;
    return 0;
}

static int drm_ioctl_create_lease(struct drm_mode_create_lease *lease) {
    if (!lease)
        return -22;
    return -38; /* ENOSYS */
}

static int drm_ioctl_list_lessees(struct drm_mode_list_lessees *list) {
    if (!list)
        return -22;
    list->count_lessees = 0;
    return 0;
}

static int drm_ioctl_revoke_lease(struct drm_mode_revoke_lease *lease) {
    if (!lease)
        return -22;
    return -38; /* ENOSYS */
}

ssize_t drm_read(void *buffer, size_t size) {
    if (!buffer || size < sizeof(struct drm_event_vblank))
        return -22;

    spinlock_acquire(&g_drm_lock);
    if (g_drm_events.count == 0) {
        spinlock_release(&g_drm_lock);
        return -11; /* EAGAIN */
    }

    struct drm_event_vblank ev = g_drm_events.events[g_drm_events.head];
    g_drm_events.head = (g_drm_events.head + 1) % DRM_MAX_EVENTS;
    g_drm_events.count--;
    spinlock_release(&g_drm_lock);

    if (!drm_copy_to_user((uintptr_t)buffer, &ev, sizeof(struct drm_event_vblank)))
        return -14;

    return (ssize_t)sizeof(struct drm_event_vblank);
}

bool drm_has_events(void) {
    return g_drm_events.count > 0;
}

/* ==============================================================================
 * PRIME Subsystem (dma-buf Buffer Sharing)
 * ============================================================================== */

static int drm_mmap_bo_locked(drm_dumb_bo_t *bo, void *addr, size_t length, int prot, int flags, void **out_vaddr) {
    (void)prot;
    (void)flags;

    process_t *proc = sched_get_current_process();
    if (!proc || !out_vaddr || length == 0 || length > VMM_USER_END - PAGE_SIZE)
        return -22;

    if (proc->mmap_current == 0) {
        proc->mmap_current = 0x0000600000000000ULL;
    }

    size_t pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages > bo->num_pages)
        return -22;

    uintptr_t vaddr = (uintptr_t)addr;
    if (vaddr == 0) {
        vaddr = proc->mmap_current;
    }
    if ((vaddr & (PAGE_SIZE - 1)) || !vmm_user_range(vaddr, pages * PAGE_SIZE)) {
        return -22;
    }

    uint64_t map_flags = VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER | VMM_FLAG_BORROWED;
    if (bo->is_direct_vram) {
        map_flags |= VMM_FLAG_WRITE_COMBINING;
    }

    for (size_t i = 0; i < pages; i++) {
        uintptr_t phys = bo->phys_pages[i];
        vmm_release_user_page(proc->pagemap, vaddr + i * PAGE_SIZE);
        if (!vmm_map_page(proc->pagemap, vaddr + i * PAGE_SIZE, phys, map_flags)) {
            for (size_t j = 0; j < i; j++)
                vmm_release_user_page(proc->pagemap, vaddr + j * PAGE_SIZE);
            return -12;
        }
    }

    if (vaddr + pages * PAGE_SIZE > proc->mmap_current)
        proc->mmap_current = vaddr + pages * PAGE_SIZE;

    /* Invalidate TLB if mapping into current address space */
    if (proc == sched_get_current_process()) {
        write_cr3(read_cr3());
    }

    *out_vaddr = (void *)vaddr;
    return 0;
}

static int dmabuf_mmap(vfs_node_t *node, void *addr, size_t length, int prot, int flags, off_t offset, void **out_vaddr) {
    (void)offset;
    if (!node || !node->device_data)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = (drm_dumb_bo_t *)node->device_data;
    if (!bo || !bo->allocated) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    int ret = drm_mmap_bo_locked(bo, addr, length, prot, flags, out_vaddr);
    spinlock_release(&g_drm_lock);
    return ret;
}

static int dmabuf_close(vfs_node_t *node) {
    if (!node || !node->device_data)
        return 0;

    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = (drm_dumb_bo_t *)node->device_data;
    drm_bo_unref_locked(bo);
    spinlock_release(&g_drm_lock);

    kfree(node);
    return 0;
}

static vfs_ops_t g_dmabuf_ops = {
    .read = NULL,
    .write = NULL,
    .open = NULL,
    .close = dmabuf_close,
    .readdir = NULL,
    .finddir = NULL,
    .create = NULL,
    .mkdir = NULL,
    .chmod = NULL,
    .chown = NULL,
    .unlink = NULL,
    .ioctl = NULL,
    .rename = NULL,
    .rmdir = NULL,
    .truncate = NULL,
    .symlink = NULL,
    .readlink = NULL,
    .link = NULL,
    .access = NULL,
    .mmap = dmabuf_mmap,
};

bool drm_is_dmabuf_node(vfs_node_t *node) {
    return node && node->ops == &g_dmabuf_ops;
}

static int drm_ioctl_prime_handle_to_fd(struct drm_prime_handle *user_req) {
    if (!user_req)
        return -22;

    struct drm_prime_handle req;
    if (!drm_copy_from_user(&req, (uintptr_t)user_req, sizeof(req)))
        return -14;

    if (req.handle == 0)
        return -22;

    process_t *proc = sched_get_current_process();
    if (!proc)
        return -1;

    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = drm_find_bo(req.handle);
    if (!bo || !bo->allocated) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    int fd = -1;
    for (int i = 0; i < MAX_FD; i++) {
        if (!proc->fds[i]) {
            fd = i;
            break;
        }
    }
    if (fd == -1) {
        spinlock_release(&g_drm_lock);
        return -24; /* EMFILE */
    }

    vfs_node_t *node = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    strcpy(node->name, "dmabuf");
    node->flags = VFS_TYPE_CHARDEVICE;
    node->permissions = 0666;
    node->ops = &g_dmabuf_ops;
    node->device_data = bo;
    node->length = bo->size;

    file_descriptor_t *fdesc = (file_descriptor_t *)kzalloc(sizeof(file_descriptor_t));
    fdesc->node = node;
    fdesc->flags = O_RDWR | (req.flags & 0x80000);
    fdesc->refcount = 1;

    proc->fds[fd] = fdesc;
    proc->fd_cloexec[fd] = (req.flags & 0x80000) ? true : false;

    bo->refcount++;
    req.fd = fd;

    spinlock_release(&g_drm_lock);

    if (!drm_copy_to_user((uintptr_t)user_req, &req, sizeof(req)))
        return -14;

    return 0;
}

static int drm_ioctl_prime_fd_to_handle(struct drm_prime_handle *user_req) {
    if (!user_req)
        return -22;

    struct drm_prime_handle req;
    if (!drm_copy_from_user(&req, (uintptr_t)user_req, sizeof(req)))
        return -14;

    if (req.fd < 0)
        return -22;

    process_t *proc = sched_get_current_process();
    if (!proc || req.fd >= MAX_FD || !proc->fds[req.fd])
        return -9; /* EBADF */

    file_descriptor_t *fdesc = proc->fds[req.fd];
    if (!fdesc->node || fdesc->node->ops != &g_dmabuf_ops || !fdesc->node->device_data)
        return -22; /* EINVAL */

    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = (drm_dumb_bo_t *)fdesc->node->device_data;
    if (!bo || !bo->allocated) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    bo->refcount++;
    req.handle = bo->handle;
    spinlock_release(&g_drm_lock);

    if (!drm_copy_to_user((uintptr_t)user_req, &req, sizeof(req)))
        return -14;

    return 0;
}

/* ==============================================================================
 * Syncobj Subsystem
 * ============================================================================== */

static drm_syncobj_t *drm_find_syncobj(uint32_t handle) {
    if (handle == 0)
        return NULL;
    for (size_t i = 0; i < DRM_MAX_SYNCOBJS; i++) {
        if (g_syncobjs[i].allocated && g_syncobjs[i].handle == handle)
            return &g_syncobjs[i];
    }
    return NULL;
}

static int drm_ioctl_syncobj_create(struct drm_syncobj_create *user_req) {
    if (!user_req)
        return -22;

    struct drm_syncobj_create req;
    if (!drm_copy_from_user(&req, (uintptr_t)user_req, sizeof(req)))
        return -14;

    spinlock_acquire(&g_drm_lock);
    drm_syncobj_t *so = NULL;
    for (size_t i = 0; i < DRM_MAX_SYNCOBJS; i++) {
        if (!g_syncobjs[i].allocated) {
            so = &g_syncobjs[i];
            break;
        }
    }
    if (!so) {
        spinlock_release(&g_drm_lock);
        return -12; /* ENOMEM */
    }

    so->handle = g_next_syncobj_handle++;
    so->allocated = true;
    so->signaled = (req.flags & DRM_SYNCOBJ_CREATE_SIGNALED) ? true : false;

    req.handle = so->handle;
    spinlock_release(&g_drm_lock);

    if (!drm_copy_to_user((uintptr_t)user_req, &req, sizeof(req)))
        return -14;

    return 0;
}

static int drm_ioctl_syncobj_destroy(struct drm_syncobj_destroy *user_req) {
    if (!user_req)
        return -22;

    struct drm_syncobj_destroy req;
    if (!drm_copy_from_user(&req, (uintptr_t)user_req, sizeof(req)))
        return -14;

    if (req.handle == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_syncobj_t *so = drm_find_syncobj(req.handle);
    if (!so) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    memset(so, 0, sizeof(drm_syncobj_t));
    spinlock_release(&g_drm_lock);
    return 0;
}

static int drm_ioctl_syncobj_wait(struct drm_syncobj_wait *user_req) {
    if (!user_req)
        return -22;

    struct drm_syncobj_wait req;
    if (!drm_copy_from_user(&req, (uintptr_t)user_req, sizeof(req)))
        return -14;

    if (req.handles == 0 || req.count_handles == 0 || req.count_handles > 64)
        return -22;

    uint32_t handles[64];
    if (!drm_copy_from_user(handles, (uintptr_t)req.handles, sizeof(uint32_t) * req.count_handles))
        return -14; /* EFAULT */

    bool wait_all = (req.flags & DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL) != 0;
    uint64_t start_ns = rtc_get_monotonic_ns();
    uint64_t timeout_ns = (req.timeout_nsec > 0) ? (uint64_t)req.timeout_nsec : 0;

    while (1) {
        spinlock_acquire(&g_drm_lock);
        uint32_t ready_count = 0;
        uint32_t first_ready = 0;

        for (uint32_t i = 0; i < req.count_handles; i++) {
            drm_syncobj_t *so = drm_find_syncobj(handles[i]);
            if (!so) {
                spinlock_release(&g_drm_lock);
                return -22;
            }
            if (so->signaled) {
                if (ready_count == 0)
                    first_ready = i;
                ready_count++;
            }
        }

        bool satisfied = wait_all ? (ready_count == req.count_handles) : (ready_count > 0);
        if (satisfied) {
            req.first_signaled = first_ready;
            spinlock_release(&g_drm_lock);
            drm_copy_to_user((uintptr_t)user_req, &req, sizeof(req));
            return 0;
        }

        spinlock_release(&g_drm_lock);

        uint64_t now_ns = rtc_get_monotonic_ns();
        if (timeout_ns == 0 || now_ns - start_ns >= timeout_ns) {
            return -62; /* ETIME */
        }

        thread_sleep(1);
    }
}

static int syncfile_close(vfs_node_t *node) {
    if (node) {
        kfree(node);
    }
    return 0;
}

static vfs_ops_t g_syncfile_ops = {
    .read = NULL,
    .write = NULL,
    .open = NULL,
    .close = syncfile_close,
    .readdir = NULL,
    .finddir = NULL,
    .create = NULL,
    .mkdir = NULL,
    .chmod = NULL,
    .chown = NULL,
    .unlink = NULL,
    .ioctl = NULL,
    .rename = NULL,
    .rmdir = NULL,
    .truncate = NULL,
    .symlink = NULL,
    .readlink = NULL,
    .link = NULL,
    .access = NULL,
    .mmap = NULL,
};

static int drm_ioctl_syncobj_handle_to_fd(struct drm_syncobj_handle *user_req) {
    if (!user_req)
        return -22;

    struct drm_syncobj_handle req;
    if (!drm_copy_from_user(&req, (uintptr_t)user_req, sizeof(req)))
        return -14;

    if (req.handle == 0)
        return -22;

    process_t *proc = sched_get_current_process();
    if (!proc)
        return -1;

    spinlock_acquire(&g_drm_lock);
    drm_syncobj_t *so = drm_find_syncobj(req.handle);
    if (!so) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    int fd = -1;
    for (int i = 0; i < MAX_FD; i++) {
        if (!proc->fds[i]) {
            fd = i;
            break;
        }
    }
    if (fd == -1) {
        spinlock_release(&g_drm_lock);
        return -24;
    }

    vfs_node_t *node = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    strcpy(node->name, "sync_file");
    node->flags = VFS_TYPE_CHARDEVICE;
    node->permissions = 0666;
    node->ops = &g_syncfile_ops;
    node->device_data = so;

    file_descriptor_t *fdesc = (file_descriptor_t *)kzalloc(sizeof(file_descriptor_t));
    fdesc->node = node;
    fdesc->flags = O_RDWR | (req.flags & 0x80000);
    fdesc->refcount = 1;

    proc->fds[fd] = fdesc;
    req.fd = fd;

    spinlock_release(&g_drm_lock);

    if (!drm_copy_to_user((uintptr_t)user_req, &req, sizeof(req)))
        return -14;

    return 0;
}

static int drm_ioctl_syncobj_fd_to_handle(struct drm_syncobj_handle *user_req) {
    if (!user_req)
        return -22;

    struct drm_syncobj_handle req;
    if (!drm_copy_from_user(&req, (uintptr_t)user_req, sizeof(req)))
        return -14;

    if (req.fd < 0)
        return -22;

    process_t *proc = sched_get_current_process();
    if (!proc || req.fd >= MAX_FD || !proc->fds[req.fd])
        return -9;

    file_descriptor_t *fdesc = proc->fds[req.fd];
    if (!fdesc->node || fdesc->node->ops != &g_syncfile_ops || !fdesc->node->device_data)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_syncobj_t *so = (drm_syncobj_t *)fdesc->node->device_data;
    if (!so || !so->allocated) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    req.handle = so->handle;
    spinlock_release(&g_drm_lock);

    if (!drm_copy_to_user((uintptr_t)user_req, &req, sizeof(req)))
        return -14;

    return 0;
}

/* ==============================================================================
 * VirtIO-GPU / Virgl 3D Acceleration IOCTLs
 * ============================================================================== */

static int drm_ioctl_virtgpu_map(struct drm_virtgpu_map *user_req) {
    if (!user_req)
        return -22;

    struct drm_virtgpu_map req;
    if (!drm_copy_from_user(&req, (uintptr_t)user_req, sizeof(req)))
        return -14;

    if (req.handle == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = drm_find_bo(req.handle);
    if (!bo) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    req.offset = bo->mmap_offset;
    spinlock_release(&g_drm_lock);

    if (!drm_copy_to_user((uintptr_t)user_req, &req, sizeof(req)))
        return -14;

    return 0;
}

static int drm_ioctl_virtgpu_getparam(struct drm_virtgpu_getparam *param) {
    if (!param)
        return -22;

    struct drm_virtgpu_getparam kparam;
    if (!drm_copy_from_user(&kparam, (uintptr_t)param, sizeof(kparam)))
        return -14;

    int value = 0;
    switch (kparam.param) {
    case VIRTGPU_PARAM_3D_FEATURES:
        value = virtio_gpu_has_virgl() ? 1 : 0;
        break;
    case VIRTGPU_PARAM_CAPSET_QUERY_FIX:
        value = 1;
        break;
    case VIRTGPU_PARAM_SUPPORTED_CAPSET_IDs:
        value = (int)virtio_gpu_get_supported_capsets();
        break;
    case VIRTGPU_PARAM_RESOURCE_BLOB:
    case VIRTGPU_PARAM_HOST_VISIBLE:
    case VIRTGPU_PARAM_CROSS_DEVICE:
    case VIRTGPU_PARAM_CONTEXT_INIT:
    case VIRTGPU_PARAM_EXPLICIT_DEBUG_NAME:
        value = 0;
        break;
    default:
        return -22;
    }

    uint64_t val64 = (uint64_t)value;
    if (!drm_copy_to_user((uintptr_t)kparam.value, &val64, sizeof(uint64_t)))
        return -14; /* EFAULT */

    return 0;
}

static int drm_ioctl_virtgpu_resource_create(struct drm_virtgpu_resource_create *user_rc) {
    if (!user_rc)
        return -22;

    struct drm_virtgpu_resource_create rc;
    if (!drm_copy_from_user(&rc, (uintptr_t)user_rc, sizeof(rc)))
        return -14;

    if (rc.width == 0) rc.width = 1;
    if (rc.height == 0) rc.height = 1;

    if (rc.stride == 0) {
        rc.stride = ALIGN_UP(rc.width * 4, 64);
    }
    if (rc.size == 0) {
        rc.size = (uint32_t)ALIGN_UP((size_t)rc.stride * rc.height, PAGE_SIZE);
    }
    if (rc.size == 0) {
        rc.size = PAGE_SIZE;
    }

    size_t size = ALIGN_UP(rc.size, PAGE_SIZE);
    size_t num_pages = size / PAGE_SIZE;

    spinlock_acquire(&g_drm_lock);

    drm_dumb_bo_t *bo = NULL;
    for (size_t i = 0; i < DRM_MAX_DUMB_BUFFERS; i++) {
        if (!g_dumb_buffers[i].allocated) {
            bo = &g_dumb_buffers[i];
            break;
        }
    }

    if (!bo) {
        spinlock_release(&g_drm_lock);
        return -12; /* ENOMEM */
    }

    uintptr_t phys = (uintptr_t)pmm_alloc_pages(num_pages);
    if (!phys) {
        spinlock_release(&g_drm_lock);
        return -12; /* ENOMEM */
    }

    void *virt = (void *)PHYS_TO_VIRT(phys);
    memset(virt, 0, size);

    uint32_t res_id = virtio_gpu_alloc_resource_id();

    bo->handle = g_next_bo_handle++;
    bo->width = rc.width;
    bo->height = rc.height;
    bo->bpp = 32;
    bo->pitch = rc.stride;
    bo->size = size;
    bo->num_pages = num_pages;
    bo->phys_pages = (uintptr_t *)kmalloc(sizeof(uintptr_t) * num_pages);
    for (size_t i = 0; i < num_pages; i++) {
        bo->phys_pages[i] = phys + (i * PAGE_SIZE);
    }
    bo->kernel_virt = virt;
    bo->mmap_offset = ((uint64_t)bo->handle) << 12;
    bo->refcount = 1;
    bo->allocated = true;
    bo->is_direct_vram = false;
    bo->hw_res_handle = res_id;
    bo->is_3d = virtio_gpu_has_virgl();

    if (virtio_gpu_has_virgl()) {
        virtio_gpu_create_3d_resource(res_id, rc.target, rc.format, rc.bind,
                                      rc.width, rc.height, rc.depth, rc.array_size,
                                      rc.last_level, rc.nr_samples, rc.flags,
                                      phys, size);
    }

    rc.res_handle = res_id;
    rc.bo_handle = bo->handle;
    rc.size = (uint32_t)size;

    spinlock_release(&g_drm_lock);

    if (!drm_copy_to_user((uintptr_t)user_rc, &rc, sizeof(rc)))
        return -14;

    return 0;
}

static int drm_ioctl_virtgpu_resource_info(struct drm_virtgpu_resource_info *user_ri) {
    if (!user_ri)
        return -22;

    struct drm_virtgpu_resource_info ri;
    if (!drm_copy_from_user(&ri, (uintptr_t)user_ri, sizeof(ri)))
        return -14;

    if (ri.bo_handle == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = drm_find_bo(ri.bo_handle);
    if (!bo) {
        spinlock_release(&g_drm_lock);
        return -2; /* ENOENT */
    }

    ri.res_handle = bo->hw_res_handle ? bo->hw_res_handle : bo->handle;
    ri.size = (uint32_t)bo->size;
    ri.blob_mem = 0;
    spinlock_release(&g_drm_lock);

    if (!drm_copy_to_user((uintptr_t)user_ri, &ri, sizeof(ri)))
        return -14;

    return 0;
}

static int drm_ioctl_virtgpu_transfer_to_host(struct drm_virtgpu_3d_transfer_to_host *user_args) {
    if (!user_args)
        return -22;

    struct drm_virtgpu_3d_transfer_to_host args;
    if (!drm_copy_from_user(&args, (uintptr_t)user_args, sizeof(args)))
        return -14;

    if (args.bo_handle == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = drm_find_bo(args.bo_handle);
    if (!bo) {
        spinlock_release(&g_drm_lock);
        return -2; /* ENOENT */
    }

    if (virtio_gpu_has_virgl() && bo->hw_res_handle) {
        virtio_gpu_box_t box;
        box.x = args.box.x;
        box.y = args.box.y;
        box.z = args.box.z;
        box.w = args.box.w;
        box.h = args.box.h;
        box.d = args.box.d ? args.box.d : 1;
        uint32_t res = bo->hw_res_handle;
        spinlock_release(&g_drm_lock);

        virtio_gpu_transfer_to_host_3d(1, res, args.offset, args.level,
                                       args.stride, args.layer_stride, &box);
        return 0;
    }

    /* Fallback for 2D mode */
    if (args.box.w > 0 && args.box.h > 0) {
        virtio_gpu_flush(args.box.x, args.box.y, args.box.w, args.box.h);
    }
    spinlock_release(&g_drm_lock);
    return 0;
}

static int drm_ioctl_virtgpu_transfer_from_host(struct drm_virtgpu_3d_transfer_from_host *user_args) {
    if (!user_args)
        return -22;

    struct drm_virtgpu_3d_transfer_from_host args;
    if (!drm_copy_from_user(&args, (uintptr_t)user_args, sizeof(args)))
        return -14;

    if (args.bo_handle == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = drm_find_bo(args.bo_handle);
    if (!bo) {
        spinlock_release(&g_drm_lock);
        return -2; /* ENOENT */
    }

    if (virtio_gpu_has_virgl() && bo->hw_res_handle) {
        virtio_gpu_box_t box;
        box.x = args.box.x;
        box.y = args.box.y;
        box.z = args.box.z;
        box.w = args.box.w;
        box.h = args.box.h;
        box.d = args.box.d ? args.box.d : 1;
        uint32_t res = bo->hw_res_handle;
        spinlock_release(&g_drm_lock);

        virtio_gpu_transfer_from_host_3d(1, res, args.offset, args.level,
                                         args.stride, args.layer_stride, &box);
        return 0;
    }

    spinlock_release(&g_drm_lock);
    return 0;
}

static int drm_ioctl_virtgpu_execbuffer(struct drm_virtgpu_execbuffer *user_exbuf) {
    if (!user_exbuf)
        return -22;

    struct drm_virtgpu_execbuffer exbuf;
    if (!drm_copy_from_user(&exbuf, (uintptr_t)user_exbuf, sizeof(exbuf)))
        return -14;

    if (exbuf.size == 0 || exbuf.command == 0)
        return -22;

    if (!virtio_gpu_has_virgl())
        return -38; /* ENOSYS */

    if (exbuf.size > (512 * 1024))
        return -22;

    void *kcmd = kmalloc(exbuf.size);
    if (!kcmd)
        return -12; /* ENOMEM */

    if (!drm_copy_from_user(kcmd, (uintptr_t)exbuf.command, exbuf.size)) {
        kfree(kcmd);
        return -14; /* EFAULT */
    }

    uint64_t fence_id = virtio_gpu_alloc_fence_id();
    int ret = virtio_gpu_submit_3d(1, kcmd, exbuf.size, fence_id);
    kfree(kcmd);

    if (ret != 0)
        return -22;

    if (exbuf.flags & VIRTGPU_EXECBUF_FENCE_FD_OUT) {
        exbuf.fence_fd = -1;
        drm_copy_to_user((uintptr_t)user_exbuf, &exbuf, sizeof(exbuf));
    }

    return 0;
}

static int drm_ioctl_virtgpu_wait(struct drm_virtgpu_3d_wait *user_args) {
    if (!user_args)
        return -22;

    struct drm_virtgpu_3d_wait args;
    if (!drm_copy_from_user(&args, (uintptr_t)user_args, sizeof(args)))
        return -14;

    if (args.handle == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = drm_find_bo(args.handle);
    if (!bo) {
        spinlock_release(&g_drm_lock);
        return -2; /* ENOENT */
    }
    spinlock_release(&g_drm_lock);
    return 0;
}

static int drm_ioctl_virtgpu_get_caps(struct drm_virtgpu_get_caps *user_args) {
    if (!user_args)
        return -22;

    struct drm_virtgpu_get_caps args;
    if (!drm_copy_from_user(&args, (uintptr_t)user_args, sizeof(args)))
        return -14;

    if (args.size == 0 || args.addr == 0)
        return -22;

    if (!virtio_gpu_has_virgl())
        return -38; /* ENOSYS */

    void *kbuf = kmalloc(args.size);
    if (!kbuf)
        return -12;

    int ret_size = virtio_gpu_get_caps(args.cap_set_id, args.cap_set_ver, kbuf, args.size);
    if (ret_size < 0) {
        kfree(kbuf);
        return -22;
    }

    if (!drm_copy_to_user((uintptr_t)args.addr, kbuf, (size_t)ret_size)) {
        kfree(kbuf);
        return -14; /* EFAULT */
    }

    kfree(kbuf);
    return 0;
}

static int drm_ioctl_virtgpu_context_init(struct drm_virtgpu_context_init *args) {
    (void)args;
    return 0;
}

/* ==============================================================================
 * DRM Ioctl Dispatchers
 * ============================================================================== */

int drm_ioctl(uint64_t request, void *argp) {
    if (!g_drm_initialized) {
        drm_init();
    }

    switch (request) {
    case DRM_IOCTL_VERSION:
        return drm_ioctl_version((struct drm_version *)argp);

    case DRM_IOCTL_GET_CAP:
        return drm_ioctl_get_cap((struct drm_get_cap *)argp);

    case DRM_IOCTL_SET_CLIENT_CAP:
        return 0;

    case DRM_IOCTL_SET_MASTER:
        g_drm_master_pid = sched_get_current_process() ? sched_get_current_process()->pid : 0;
        fb_set_graphics_mode(true);
        return 0;

    case DRM_IOCTL_DROP_MASTER:
        g_drm_master_pid = 0;
        fb_set_graphics_mode(false);
        return 0;

    case DRM_IOCTL_MODE_GETRESOURCES:
        return drm_ioctl_get_resources((struct drm_mode_card_res *)argp);

    case DRM_IOCTL_MODE_GETCONNECTOR:
        return drm_ioctl_get_connector((struct drm_mode_get_connector *)argp);

    case DRM_IOCTL_MODE_GETENCODER:
        return drm_ioctl_get_encoder((struct drm_mode_get_encoder *)argp);

    case DRM_IOCTL_MODE_GETCRTC:
        return drm_ioctl_get_crtc((struct drm_mode_crtc *)argp);

    case DRM_IOCTL_MODE_SETCRTC:
        return drm_ioctl_set_crtc((struct drm_mode_crtc *)argp);

    case DRM_IOCTL_MODE_CREATE_DUMB:
        return drm_ioctl_create_dumb((struct drm_mode_create_dumb *)argp);

    case DRM_IOCTL_MODE_MAP_DUMB:
        return drm_ioctl_map_dumb((struct drm_mode_map_dumb *)argp);

    case DRM_IOCTL_MODE_DESTROY_DUMB:
        return drm_ioctl_destroy_dumb((struct drm_mode_destroy_dumb *)argp);

    case DRM_IOCTL_GEM_CLOSE:
        return drm_ioctl_gem_close((struct drm_gem_close *)argp);

    case DRM_IOCTL_MODE_ADDFB:
        return drm_ioctl_add_fb((struct drm_mode_fb_cmd *)argp);

    case DRM_IOCTL_MODE_ADDFB2:
        return drm_ioctl_add_fb2((struct drm_mode_fb_cmd2 *)argp);

    case DRM_IOCTL_MODE_RMFB:
        return drm_ioctl_rm_fb((uint32_t *)argp);

    case DRM_IOCTL_MODE_PAGE_FLIP:
        return drm_ioctl_page_flip((struct drm_mode_crtc_page_flip *)argp);

    case DRM_IOCTL_MODE_DIRTYFB:
        return drm_ioctl_dirty_fb((struct drm_mode_fb_dirty_cmd *)argp);

    case DRM_IOCTL_PRIME_HANDLE_TO_FD:
        return drm_ioctl_prime_handle_to_fd((struct drm_prime_handle *)argp);

    case DRM_IOCTL_PRIME_FD_TO_HANDLE:
        return drm_ioctl_prime_fd_to_handle((struct drm_prime_handle *)argp);

    case DRM_IOCTL_SYNCOBJ_CREATE:
        return drm_ioctl_syncobj_create((struct drm_syncobj_create *)argp);

    case DRM_IOCTL_SYNCOBJ_DESTROY:
        return drm_ioctl_syncobj_destroy((struct drm_syncobj_destroy *)argp);

    case DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD:
        return drm_ioctl_syncobj_handle_to_fd((struct drm_syncobj_handle *)argp);

    case DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE:
        return drm_ioctl_syncobj_fd_to_handle((struct drm_syncobj_handle *)argp);

    case DRM_IOCTL_SYNCOBJ_WAIT:
        return drm_ioctl_syncobj_wait((struct drm_syncobj_wait *)argp);

    case DRM_IOCTL_MODE_GETPLANERESOURCES:
        return drm_ioctl_get_plane_res((struct drm_mode_get_plane_res *)argp);

    case DRM_IOCTL_MODE_GETPLANE:
        return drm_ioctl_get_plane((struct drm_mode_get_plane *)argp);

    case DRM_IOCTL_MODE_SETPLANE:
        return drm_ioctl_set_plane((struct drm_mode_set_plane *)argp);

    case DRM_IOCTL_MODE_OBJ_GETPROPERTIES:
        return drm_ioctl_obj_get_properties((struct drm_mode_obj_get_properties *)argp);

    case DRM_IOCTL_MODE_OBJ_SETPROPERTY:
        return drm_ioctl_obj_set_property((struct drm_mode_obj_set_property *)argp);

    case DRM_IOCTL_MODE_CURSOR:
        return drm_ioctl_cursor((struct drm_mode_cursor *)argp);

    case DRM_IOCTL_MODE_CURSOR2:
        return drm_ioctl_cursor2((struct drm_mode_cursor2 *)argp);

    case DRM_IOCTL_MODE_ATOMIC:
        return drm_ioctl_atomic((struct drm_mode_atomic *)argp);

    case DRM_IOCTL_MODE_CREATEPROPBLOB:
        return drm_ioctl_create_blob((struct drm_mode_create_blob *)argp);

    case DRM_IOCTL_MODE_GETPROPBLOB:
        return drm_ioctl_get_prop_blob((struct drm_mode_get_blob *)argp);

    case DRM_IOCTL_MODE_DESTROYPROPBLOB:
        return drm_ioctl_destroy_blob((struct drm_mode_destroy_blob *)argp);

    case DRM_IOCTL_MODE_GETPROPERTY:
        return drm_ioctl_get_property((struct drm_mode_get_property *)argp);

    case DRM_IOCTL_MODE_SETPROPERTY:
        return drm_ioctl_set_property((struct drm_mode_connector_set_property *)argp);

    case DRM_IOCTL_MODE_SETGAMMA:
        return drm_ioctl_set_gamma((struct drm_mode_crtc_lut *)argp);

    case DRM_IOCTL_MODE_CREATE_LEASE:
        return drm_ioctl_create_lease((struct drm_mode_create_lease *)argp);

    case DRM_IOCTL_MODE_LIST_LESSEES:
        return drm_ioctl_list_lessees((struct drm_mode_list_lessees *)argp);

    case DRM_IOCTL_MODE_REVOKE_LEASE:
        return drm_ioctl_revoke_lease((struct drm_mode_revoke_lease *)argp);

    case DRM_IOCTL_VIRTGPU_MAP:
        return drm_ioctl_virtgpu_map((struct drm_virtgpu_map *)argp);

    case DRM_IOCTL_VIRTGPU_GETPARAM:
        return drm_ioctl_virtgpu_getparam((struct drm_virtgpu_getparam *)argp);

    case DRM_IOCTL_VIRTGPU_RESOURCE_CREATE:
        return drm_ioctl_virtgpu_resource_create((struct drm_virtgpu_resource_create *)argp);

    case DRM_IOCTL_VIRTGPU_RESOURCE_INFO:
        return drm_ioctl_virtgpu_resource_info((struct drm_virtgpu_resource_info *)argp);

    case DRM_IOCTL_VIRTGPU_TRANSFER_TO_HOST:
        return drm_ioctl_virtgpu_transfer_to_host((struct drm_virtgpu_3d_transfer_to_host *)argp);

    case DRM_IOCTL_VIRTGPU_TRANSFER_FROM_HOST:
        return drm_ioctl_virtgpu_transfer_from_host((struct drm_virtgpu_3d_transfer_from_host *)argp);

    case DRM_IOCTL_VIRTGPU_EXECBUFFER:
        return drm_ioctl_virtgpu_execbuffer((struct drm_virtgpu_execbuffer *)argp);

    case DRM_IOCTL_VIRTGPU_WAIT:
        return drm_ioctl_virtgpu_wait((struct drm_virtgpu_3d_wait *)argp);

    case DRM_IOCTL_VIRTGPU_GET_CAPS:
        return drm_ioctl_virtgpu_get_caps((struct drm_virtgpu_get_caps *)argp);

    case DRM_IOCTL_VIRTGPU_CONTEXT_INIT:
        return drm_ioctl_virtgpu_context_init((struct drm_virtgpu_context_init *)argp);

    /* Intel i915 Graphics Driver IOCTLs */
    case DRM_IOCTL_I915_GETPARAM:
    case DRM_IOCTL_I915_SETPARAM:
    case DRM_IOCTL_I915_GEM_CREATE:
    case DRM_IOCTL_I915_GEM_PREAD:
    case DRM_IOCTL_I915_GEM_PWRITE:
    case DRM_IOCTL_I915_GEM_MMAP:
    case DRM_IOCTL_I915_GEM_MMAP_GTT:
    case DRM_IOCTL_I915_GEM_MMAP_OFFSET:
    case DRM_IOCTL_I915_GEM_SET_TILING:
    case DRM_IOCTL_I915_GEM_GET_TILING:
    case DRM_IOCTL_I915_GEM_GET_APERTURE:
    case DRM_IOCTL_I915_GEM_SET_DOMAIN:
    case DRM_IOCTL_I915_GEM_BUSY:
    case DRM_IOCTL_I915_GEM_MADVISE:
    case DRM_IOCTL_I915_GEM_EXECBUFFER:
    case DRM_IOCTL_I915_GEM_EXECBUFFER2:
    case DRM_IOCTL_I915_GEM_CONTEXT_CREATE:
    case DRM_IOCTL_I915_GEM_CONTEXT_DESTROY:
    case DRM_IOCTL_I915_GEM_WAIT:
    case DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM:
    case DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM:
    case DRM_IOCTL_I915_GEM_SET_CACHING:
    case DRM_IOCTL_I915_GEM_GET_CACHING:
    case DRM_IOCTL_I915_QUERY:
        return i915_ioctl_dispatch((uint32_t)request, argp);

    default:
        klog_warn("DRM: Unsupported ioctl 0x%lx", request);
        return -22; /* EINVAL */
    }
}

int drm_render_ioctl(uint64_t request, void *argp) {
    if (!g_drm_initialized) {
        drm_init();
    }

    switch (request) {
    case DRM_IOCTL_VERSION:
        return drm_ioctl_version((struct drm_version *)argp);

    case DRM_IOCTL_GET_CAP:
        return drm_ioctl_get_cap((struct drm_get_cap *)argp);

    case DRM_IOCTL_SET_CLIENT_CAP:
        return 0;

    case DRM_IOCTL_MODE_CREATE_DUMB:
        return drm_ioctl_create_dumb((struct drm_mode_create_dumb *)argp);

    case DRM_IOCTL_MODE_MAP_DUMB:
        return drm_ioctl_map_dumb((struct drm_mode_map_dumb *)argp);

    case DRM_IOCTL_MODE_DESTROY_DUMB:
        return drm_ioctl_destroy_dumb((struct drm_mode_destroy_dumb *)argp);

    case DRM_IOCTL_GEM_CLOSE:
        return drm_ioctl_gem_close((struct drm_gem_close *)argp);

    case DRM_IOCTL_PRIME_HANDLE_TO_FD:
        return drm_ioctl_prime_handle_to_fd((struct drm_prime_handle *)argp);

    case DRM_IOCTL_PRIME_FD_TO_HANDLE:
        return drm_ioctl_prime_fd_to_handle((struct drm_prime_handle *)argp);

    case DRM_IOCTL_SYNCOBJ_CREATE:
        return drm_ioctl_syncobj_create((struct drm_syncobj_create *)argp);

    case DRM_IOCTL_SYNCOBJ_DESTROY:
        return drm_ioctl_syncobj_destroy((struct drm_syncobj_destroy *)argp);

    case DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD:
        return drm_ioctl_syncobj_handle_to_fd((struct drm_syncobj_handle *)argp);

    case DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE:
        return drm_ioctl_syncobj_fd_to_handle((struct drm_syncobj_handle *)argp);

    case DRM_IOCTL_SYNCOBJ_WAIT:
        return drm_ioctl_syncobj_wait((struct drm_syncobj_wait *)argp);

    /* Render nodes explicitly forbid mode setting and master ioctls */
    case DRM_IOCTL_SET_MASTER:
    case DRM_IOCTL_DROP_MASTER:
    case DRM_IOCTL_MODE_SETCRTC:
    case DRM_IOCTL_MODE_PAGE_FLIP:
        return -13; /* EACCES */

    default:
        return drm_ioctl(request, argp);
    }
}

int drm_mmap(void *addr, size_t length, int prot, int flags, off_t offset, void **out_vaddr) {
    if (!out_vaddr || length == 0)
        return -22;

    if (i915_is_active() && (uint64_t)offset >= 0x200000000ULL) {
        i915_device_t *idev = i915_get_device();
        if (idev) {
            uint32_t gem_handle = (uint32_t)(((uint64_t)offset - 0x200000000ULL) >> 16);
            i915_gem_bo_t *gbo = i915_gem_find(idev, gem_handle);
            if (gbo) {
                return i915_gem_mmap_user(gbo, addr, length, out_vaddr);
            }
        }
    }

    uint32_t handle = (uint32_t)(offset >> 12);
    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = drm_find_bo(handle);
    if (!bo || !bo->allocated) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    int ret = drm_mmap_bo_locked(bo, addr, length, prot, flags, out_vaddr);
    spinlock_release(&g_drm_lock);
    return ret;
}

int drm_release(void) {
    spinlock_acquire(&g_drm_lock);
    pid_t cur_pid = sched_get_current_process() ? sched_get_current_process()->pid : 0;
    if (g_drm_master_pid == cur_pid || g_drm_master_pid == 0) {
        g_drm_master_pid = 0;
        fb_set_graphics_mode(false);
    }
    spinlock_release(&g_drm_lock);
    return 0;
}
