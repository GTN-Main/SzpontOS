/*
 * SzpontOS - DRM/KMS Kernel Driver Implementation
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/drm.h>
#include <drivers/framebuffer.h>
#include <mm/heap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <sched/process.h>
#include <sched/sched.h>
#include <kernel/string.h>
#include <kernel/kprint.h>
#include <kernel/spinlock.h>

static spinlock_t g_drm_lock = SPINLOCK_INIT;
static drm_dumb_bo_t g_dumb_buffers[DRM_MAX_DUMB_BUFFERS];
static drm_fb_t g_framebuffers[DRM_MAX_FBS];

static drm_crtc_state_t g_crtc;
static drm_connector_state_t g_connector;
static drm_encoder_state_t g_encoder;
static struct drm_mode_modeinfo g_active_mode;

static pid_t g_drm_master_pid = 0;
static bool g_drm_initialized = false;

static uint32_t g_next_bo_handle = 1;
static uint32_t g_next_fb_id = 100;

static void drm_setup_default_mode(void) {
    size_t width = fb_get_width();
    size_t height = fb_get_height();

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

    drm_setup_default_mode();
    g_drm_initialized = true;
    spinlock_release(&g_drm_lock);

    klog_info("DRM/KMS: Initialized (Mode: %s, Connector ID: %u, CRTC ID: %u)",
              g_active_mode.name, g_connector.connector_id, g_crtc.crtc_id);
}

static drm_dumb_bo_t *drm_find_bo(uint32_t handle) {
    if (handle == 0)
        return NULL;
    for (size_t i = 0; i < DRM_MAX_DUMB_BUFFERS; i++) {
        if (g_dumb_buffers[i].allocated && g_dumb_buffers[i].handle == handle) {
            return &g_dumb_buffers[i];
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

static int drm_ioctl_version(struct drm_version *ver) {
    if (!ver)
        return -22; /* EINVAL */

    ver->version_major = 1;
    ver->version_minor = 0;
    ver->version_patchlevel = 0;

    const char *name = "szpont-drm";
    const char *date = "20260830";
    const char *desc = "SzpontOS Kernel Mode Setting & DRM Driver";

    if (ver->name && ver->name_len > 0) {
        size_t n = strlen(name);
        if (n >= ver->name_len)
            n = ver->name_len - 1;
        memcpy(ver->name, name, n);
        ver->name[n] = '\0';
    }
    ver->name_len = strlen(name);

    if (ver->date && ver->date_len > 0) {
        size_t n = strlen(date);
        if (n >= ver->date_len)
            n = ver->date_len - 1;
        memcpy(ver->date, date, n);
        ver->date[n] = '\0';
    }
    ver->date_len = strlen(date);

    if (ver->desc && ver->desc_len > 0) {
        size_t n = strlen(desc);
        if (n >= ver->desc_len)
            n = ver->desc_len - 1;
        memcpy(ver->desc, desc, n);
        ver->desc[n] = '\0';
    }
    ver->desc_len = strlen(desc);

    return 0;
}

static int drm_ioctl_get_cap(struct drm_get_cap *cap) {
    if (!cap)
        return -22;

    switch (cap->capability) {
    case DRM_CAP_DUMB_BUFFER:
        cap->value = 1;
        return 0;
    case DRM_CAP_VBLANK_HIGH_CRTC:
        cap->value = 1;
        return 0;
    case DRM_CAP_DUMB_PREFERRED_DEPTH:
        cap->value = 32;
        return 0;
    case DRM_CAP_DUMB_PREFER_SHADOW:
        cap->value = 1;
        return 0;
    case DRM_CAP_PRIME:
        cap->value = 0;
        return 0;
    case DRM_CAP_TIMESTAMP_MONOTONIC:
        cap->value = 1;
        return 0;
    case DRM_CAP_ASYNC_PAGE_FLIP:
        cap->value = 1;
        return 0;
    case DRM_CAP_CURSOR_WIDTH:
        cap->value = 64;
        return 0;
    case DRM_CAP_CURSOR_HEIGHT:
        cap->value = 64;
        return 0;
    default:
        cap->value = 0;
        return -22;
    }
}

static int drm_ioctl_get_resources(struct drm_mode_card_res *res) {
    if (!res)
        return -22;

    res->min_width = 320;
    res->max_width = 3840;
    res->min_height = 200;
    res->max_height = 2160;

    res->count_connectors = 1;
    res->count_encoders = 1;
    res->count_crtcs = 1;

    /* Count allocated framebuffers */
    uint32_t count_fbs = 0;
    for (size_t i = 0; i < DRM_MAX_FBS; i++) {
        if (g_framebuffers[i].allocated)
            count_fbs++;
    }
    res->count_fbs = count_fbs;

    if (res->connector_id_ptr) {
        uint32_t *conn_ptr = (uint32_t *)(uintptr_t)res->connector_id_ptr;
        conn_ptr[0] = g_connector.connector_id;
    }
    if (res->encoder_id_ptr) {
        uint32_t *enc_ptr = (uint32_t *)(uintptr_t)res->encoder_id_ptr;
        enc_ptr[0] = g_encoder.encoder_id;
    }
    if (res->crtc_id_ptr) {
        uint32_t *crtc_ptr = (uint32_t *)(uintptr_t)res->crtc_id_ptr;
        crtc_ptr[0] = g_crtc.crtc_id;
    }
    if (res->fb_id_ptr && count_fbs > 0) {
        uint32_t *fb_ptr = (uint32_t *)(uintptr_t)res->fb_id_ptr;
        size_t idx = 0;
        for (size_t i = 0; i < DRM_MAX_FBS; i++) {
            if (g_framebuffers[i].allocated) {
                fb_ptr[idx++] = g_framebuffers[i].fb_id;
            }
        }
    }

    return 0;
}

static int drm_ioctl_get_connector(struct drm_mode_get_connector *conn) {
    if (!conn)
        return -22;

    conn->connector_id = g_connector.connector_id;
    conn->connector_type = g_connector.connector_type;
    conn->connector_type_id = 1;
    conn->connection = g_connector.connection;
    conn->mm_width = g_connector.mm_width;
    conn->mm_height = g_connector.mm_height;
    conn->subpixel = 0;

    conn->count_encoders = 1;
    conn->count_modes = 1;
    conn->count_props = 0;
    conn->encoder_id = g_connector.encoder_id;

    if (conn->encoders_ptr) {
        uint32_t *enc_ptr = (uint32_t *)(uintptr_t)conn->encoders_ptr;
        enc_ptr[0] = g_encoder.encoder_id;
    }

    if (conn->modes_ptr && conn->count_modes > 0) {
        struct drm_mode_modeinfo *modes = (struct drm_mode_modeinfo *)(uintptr_t)conn->modes_ptr;
        memcpy(&modes[0], &g_active_mode, sizeof(struct drm_mode_modeinfo));
    }

    return 0;
}

static int drm_ioctl_get_encoder(struct drm_mode_get_encoder *enc) {
    if (!enc)
        return -22;

    enc->encoder_id = g_encoder.encoder_id;
    enc->encoder_type = g_encoder.encoder_type;
    enc->crtc_id = g_encoder.crtc_id;
    enc->possible_crtcs = g_encoder.possible_crtcs;
    enc->possible_clones = 0;
    return 0;
}

static int drm_ioctl_get_crtc(struct drm_mode_crtc *crtc) {
    if (!crtc)
        return -22;

    crtc->crtc_id = g_crtc.crtc_id;
    crtc->fb_id = g_crtc.fb_id;
    crtc->x = g_crtc.x;
    crtc->y = g_crtc.y;
    crtc->gamma_size = 0;
    crtc->mode_valid = g_crtc.mode_valid ? 1 : 0;
    memcpy(&crtc->mode, &g_crtc.mode, sizeof(struct drm_mode_modeinfo));
    return 0;
}

static int drm_ioctl_set_crtc(struct drm_mode_crtc *crtc) {
    if (!crtc)
        return -22;

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
        if (bo && bo->kernel_virt) {
            fb_blit_from_buffer((const uint32_t *)bo->kernel_virt, bo->pitch / 4, 0, 0, bo->width, bo->height);
        }
    }

    spinlock_release(&g_drm_lock);
    return 0;
}

static int drm_ioctl_create_dumb(struct drm_mode_create_dumb *req) {
    if (!req || req->width == 0 || req->height == 0)
        return -22;

    if (req->bpp == 0)
        req->bpp = 32;

    uint32_t pitch = ALIGN_UP(req->width * ((req->bpp + 7) / 8), 64);
    size_t size = ALIGN_UP((size_t)pitch * req->height, PAGE_SIZE);
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

    if (lfb && lfb->address && req->width == fb_get_width() && req->height == fb_get_height()) {
        uintptr_t fb_virt = (uintptr_t)lfb->address;
        phys = VIRT_TO_PHYS(fb_virt);
        virt = (void *)fb_virt;
        pitch = (uint32_t)lfb->pitch;
        size = ALIGN_UP((size_t)pitch * req->height, PAGE_SIZE);
        num_pages = size / PAGE_SIZE;
        is_direct = true;
    } else {
        phys = pmm_alloc_pages(num_pages);
        if (!phys) {
            spinlock_release(&g_drm_lock);
            return -12; /* ENOMEM */
        }
        virt = (void *)PHYS_TO_VIRT(phys);
        memset(virt, 0, size);
    }

    bo->handle = g_next_bo_handle++;
    bo->width = req->width;
    bo->height = req->height;
    bo->bpp = req->bpp;
    bo->pitch = pitch;
    bo->size = size;
    bo->num_pages = num_pages;
    bo->phys_pages = (uintptr_t *)kmalloc(sizeof(uintptr_t) * num_pages);
    for (size_t i = 0; i < num_pages; i++) {
        bo->phys_pages[i] = phys + i * PAGE_SIZE;
    }
    bo->kernel_virt = virt;
    bo->mmap_offset = ((uint64_t)bo->handle) << 12;
    bo->allocated = true;
    bo->is_direct_vram = is_direct;

    req->handle = bo->handle;
    req->pitch = pitch;
    req->size = size;

    spinlock_release(&g_drm_lock);
    return 0;
}

static int drm_ioctl_map_dumb(struct drm_mode_map_dumb *req) {
    if (!req || req->handle == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = drm_find_bo(req->handle);
    if (!bo) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    req->offset = bo->mmap_offset;
    spinlock_release(&g_drm_lock);
    return 0;
}

static int drm_ioctl_destroy_dumb(struct drm_mode_destroy_dumb *req) {
    if (!req || req->handle == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = drm_find_bo(req->handle);
    if (!bo) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    if (bo->phys_pages) {
        if (!bo->is_direct_vram && bo->phys_pages[0]) {
            pmm_free_pages(bo->phys_pages[0], bo->num_pages);
        }
        kfree(bo->phys_pages);
    }
    memset(bo, 0, sizeof(drm_dumb_bo_t));

    spinlock_release(&g_drm_lock);
    return 0;
}

static int drm_ioctl_add_fb(struct drm_mode_fb_cmd *cmd) {
    if (!cmd || cmd->handle == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = drm_find_bo(cmd->handle);
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
    fb->width = cmd->width;
    fb->height = cmd->height;
    fb->pitch = cmd->pitch;
    fb->bpp = cmd->bpp;
    fb->depth = cmd->depth;
    fb->bo_handle = cmd->handle;
    fb->allocated = true;

    cmd->fb_id = fb->fb_id;
    spinlock_release(&g_drm_lock);
    return 0;
}

static int drm_ioctl_rm_fb(uint32_t *fb_id_ptr) {
    if (!fb_id_ptr || *fb_id_ptr == 0)
        return -22;

    spinlock_acquire(&g_drm_lock);
    drm_fb_t *fb = drm_find_fb(*fb_id_ptr);
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

static int drm_ioctl_dirty_fb(struct drm_mode_fb_dirty_cmd *dirty) {
    if (!dirty || dirty->fb_id == 0)
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
        if (dirty->num_clips == 0 || dirty->clips_ptr == 0) {
            /* Blit full buffer to screen */
            fb_blit_from_buffer((const uint32_t *)bo->kernel_virt, bo->pitch / 4, 0, 0, bo->width, bo->height);
        } else {
            /* Blit clipped damage rectangles */
            struct drm_clip_rect *clips = (struct drm_clip_rect *)(uintptr_t)dirty->clips_ptr;
            for (uint32_t i = 0; i < dirty->num_clips; i++) {
                size_t x = clips[i].x1;
                size_t y = clips[i].y1;
                size_t w = (clips[i].x2 > clips[i].x1) ? (clips[i].x2 - clips[i].x1) : 0;
                size_t h = (clips[i].y2 > clips[i].y1) ? (clips[i].y2 - clips[i].y1) : 0;
                if (w > 0 && h > 0) {
                    fb_blit_from_buffer((const uint32_t *)bo->kernel_virt, bo->pitch / 4, x, y, w, h);
                }
            }
        }
    }

    spinlock_release(&g_drm_lock);
    return 0;
}

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

    case DRM_IOCTL_MODE_ADDFB:
        return drm_ioctl_add_fb((struct drm_mode_fb_cmd *)argp);

    case DRM_IOCTL_MODE_RMFB:
        return drm_ioctl_rm_fb((uint32_t *)argp);

    case DRM_IOCTL_MODE_DIRTYFB:
        return drm_ioctl_dirty_fb((struct drm_mode_fb_dirty_cmd *)argp);

    default:
        klog_warn("DRM: Unsupported ioctl 0x%lx", request);
        return -22; /* EINVAL */
    }
}

int drm_mmap(void *addr, size_t length, int prot, int flags, off_t offset, void **out_vaddr) {
    (void)prot;
    (void)flags;

    process_t *proc = sched_get_current_process();
    if (!proc || !out_vaddr || length == 0)
        return -22;

    uint32_t handle = (uint32_t)(offset >> 12);
    spinlock_acquire(&g_drm_lock);
    drm_dumb_bo_t *bo = drm_find_bo(handle);
    if (!bo) {
        spinlock_release(&g_drm_lock);
        return -22;
    }

    if (proc->mmap_current == 0) {
        proc->mmap_current = 0x0000600000000000ULL;
    }

    size_t pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages > bo->num_pages)
        pages = bo->num_pages;

    uintptr_t vaddr = (uintptr_t)addr;
    if (vaddr == 0) {
        vaddr = proc->mmap_current;
        proc->mmap_current += pages * PAGE_SIZE;
    }

    for (size_t i = 0; i < pages; i++) {
        uintptr_t phys = bo->phys_pages[i];
        vmm_map_page(proc->pagemap, vaddr + i * PAGE_SIZE, phys,
                     VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER);
    }

    spinlock_release(&g_drm_lock);
    *out_vaddr = (void *)vaddr;
    return 0;
}
