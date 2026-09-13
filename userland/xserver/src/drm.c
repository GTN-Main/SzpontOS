/*
 * SzpontOS - SzpontX11 Native X11 Server
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * DRM/KMS Hardware Display Backend, Hardware Double Buffering & Zero-Copy Flipping
 */

#include "xserver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

static bool allocate_dumb_buffer(int fd, int width, int height, int bpp,
                                 uint32_t *out_handle, uint32_t *out_pitch,
                                 uint32_t *out_fb_id, uint32_t **out_mapped) {
    struct drm_mode_create_dumb create_req;
    memset(&create_req, 0, sizeof(create_req));
    create_req.width = width;
    create_req.height = height;
    create_req.bpp = bpp;

    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req) < 0) {
        perror("[SzpontX11] DRM_IOCTL_MODE_CREATE_DUMB failed");
        return false;
    }

    uint32_t handle = create_req.handle;
    uint32_t pitch = create_req.pitch;
    uint32_t fb_id = 0;

    if (drmModeAddFB(fd, width, height, 24, 32, pitch, handle, &fb_id) < 0) {
        perror("[SzpontX11] drmModeAddFB failed");
        struct drm_mode_destroy_dumb destroy_req = {.handle = handle};
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
        return false;
    }

    struct drm_mode_map_dumb map_req;
    memset(&map_req, 0, sizeof(map_req));
    map_req.handle = handle;

    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req) < 0) {
        perror("[SzpontX11] DRM_IOCTL_MODE_MAP_DUMB failed");
        drmModeRmFB(fd, fb_id);
        struct drm_mode_destroy_dumb destroy_req = {.handle = handle};
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
        return false;
    }

    uint32_t *mapped = (uint32_t *)mmap(NULL, (size_t)create_req.size,
                                        PROT_READ | PROT_WRITE, MAP_SHARED,
                                        fd, (off_t)map_req.offset);
    if (mapped == MAP_FAILED || !mapped) {
        perror("[SzpontX11] mmap DRM framebuffer failed");
        drmModeRmFB(fd, fb_id);
        struct drm_mode_destroy_dumb destroy_req = {.handle = handle};
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
        return false;
    }

    *out_handle = handle;
    *out_pitch = pitch;
    *out_fb_id = fb_id;
    *out_mapped = mapped;
    return true;
}

bool drm_init_display(void) {
    g_server.drm_fd = open("/dev/dri/card0", O_RDWR);
    if (g_server.drm_fd < 0) {
        perror("[SzpontX11] Error opening /dev/dri/card0");
        return false;
    }

    drmSetMaster(g_server.drm_fd);

    drmModeRes *res = drmModeGetResources(g_server.drm_fd);
    if (!res) {
        fprintf(stderr, "[SzpontX11] Failed to retrieve DRM resources\n");
        close(g_server.drm_fd);
        return false;
    }

    drmModeConnector *conn = NULL;
    for (int i = 0; i < res->count_connectors; i++) {
        conn = drmModeGetConnector(g_server.drm_fd, res->connectors[i]);
        if (conn && conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
            break;
        }
        if (conn) {
            drmModeFreeConnector(conn);
            conn = NULL;
        }
    }

    if (!conn) {
        fprintf(stderr, "[SzpontX11] No connected DRM connector found!\n");
        drmModeFreeResources(res);
        close(g_server.drm_fd);
        return false;
    }

    drmModeModeInfo mode = conn->modes[0];
    g_server.width = mode.hdisplay;
    g_server.height = mode.vdisplay;
    g_server.bpp = 32;
    g_server.pitch = g_server.width * 4;
    g_server.conn_id = conn->connector_id;
    g_server.crtc_id = res->crtcs[0];

    printf("[SzpontX11] DRM Mode Selected: %dx%d @ %dHz (Connector ID: %u, CRTC ID: %u)\n",
           g_server.width, g_server.height, mode.vrefresh ? mode.vrefresh : 60,
           g_server.conn_id, g_server.crtc_id);

    /* 1. Allocate Scanout VRAM Framebuffer */
    if (!allocate_dumb_buffer(g_server.drm_fd, g_server.width, g_server.height, 32,
                             &g_server.front_dumb_handle, (uint32_t *)&g_server.pitch,
                             &g_server.front_fb_id, &g_server.front_fb_mapped)) {
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        close(g_server.drm_fd);
        return false;
    }

    g_server.back_dumb_handle = g_server.front_dumb_handle;
    g_server.back_fb_id = g_server.front_fb_id;
    g_server.back_fb_mapped = g_server.front_fb_mapped;

    /* Set active render pointers to direct VRAM */
    g_server.fb_id = g_server.front_fb_id;
    g_server.dumb_handle = g_server.front_dumb_handle;
    g_server.fb_mapped = g_server.front_fb_mapped;

    /* Allocate Shadow Buffer in RAM for flicker-free compositing */
    g_server.shadow_fb = (uint32_t *)calloc((size_t)(g_server.width * g_server.height), sizeof(uint32_t));
    if (!g_server.shadow_fb) {
        fprintf(stderr, "[SzpontX11] Failed to allocate shadow framebuffer\n");
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        close(g_server.drm_fd);
        return false;
    }

    /* Set CRTC mode scanning out from direct VRAM buffer */
    if (drmModeSetCrtc(g_server.drm_fd, g_server.crtc_id, g_server.front_fb_id, 0, 0,
                       &g_server.conn_id, 1, &mode) < 0) {
        perror("[SzpontX11] drmModeSetCrtc failed");
    }

    drmModeFreeConnector(conn);
    drmModeFreeResources(res);

    printf("[SzpontX11] DRM/KMS Direct Scanout Framebuffer ready (FB ID: %u, VRAM %dx%d)\n",
           g_server.front_fb_id, g_server.width, g_server.height);
    return true;
}

void drm_cleanup_display(void) {
    if (g_server.shadow_fb) {
        free(g_server.shadow_fb);
        g_server.shadow_fb = NULL;
    }
    if (g_server.drm_fd >= 0) {
        drmDropMaster(g_server.drm_fd);
        if (g_server.front_fb_mapped) {
            munmap(g_server.front_fb_mapped, (size_t)g_server.pitch * g_server.height);
            g_server.front_fb_mapped = NULL;
            g_server.back_fb_mapped = NULL;
            g_server.fb_mapped = NULL;
        }
        if (g_server.front_fb_id) {
            drmModeRmFB(g_server.drm_fd, g_server.front_fb_id);
            g_server.front_fb_id = 0;
            g_server.back_fb_id = 0;
            g_server.fb_id = 0;
        }
        if (g_server.front_dumb_handle) {
            struct drm_mode_destroy_dumb req = {.handle = g_server.front_dumb_handle};
            drmIoctl(g_server.drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &req);
            g_server.front_dumb_handle = 0;
            g_server.back_dumb_handle = 0;
            g_server.dumb_handle = 0;
        }
        close(g_server.drm_fd);
        g_server.drm_fd = -1;
    }
}

void drm_swap_buffers(void) {
    /* Direct presentation from shadow buffer: no flipping needed */
}

void drm_flush_rect(int x, int y, int w, int h) {
    if (!g_server.fb_mapped || !g_server.shadow_fb) return;

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w > g_server.width) ? g_server.width : (x + w);
    int y1 = (y + h > g_server.height) ? g_server.height : (y + h);

    if (x0 >= x1 || y0 >= y1) return;

    int stride = g_server.pitch / 4;
    for (int r = y0; r < y1; r++) {
        memcpy(&g_server.fb_mapped[r * stride + x0],
               &g_server.shadow_fb[r * stride + x0],
               (size_t)(x1 - x0) * sizeof(uint32_t));
    }
    __asm__ volatile("sfence" ::: "memory");
}

void drm_flush_screen(void) {
    if (!g_server.fb_mapped || !g_server.shadow_fb) return;
    memcpy(g_server.fb_mapped, g_server.shadow_fb,
           (size_t)(g_server.width * g_server.height) * sizeof(uint32_t));
    __asm__ volatile("sfence" ::: "memory");
}
