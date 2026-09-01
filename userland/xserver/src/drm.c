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

    /* 1. Allocate Front Buffer */
    if (!allocate_dumb_buffer(g_server.drm_fd, g_server.width, g_server.height, 32,
                             &g_server.front_dumb_handle, (uint32_t *)&g_server.pitch,
                             &g_server.front_fb_id, &g_server.front_fb_mapped)) {
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        close(g_server.drm_fd);
        return false;
    }

    /* 2. Allocate Back Buffer */
    if (!allocate_dumb_buffer(g_server.drm_fd, g_server.width, g_server.height, 32,
                             &g_server.back_dumb_handle, (uint32_t *)&g_server.pitch,
                             &g_server.back_fb_id, &g_server.back_fb_mapped)) {
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        close(g_server.drm_fd);
        return false;
    }

    /* Set active render pointers to back buffer */
    g_server.fb_id = g_server.back_fb_id;
    g_server.dumb_handle = g_server.back_dumb_handle;
    g_server.fb_mapped = g_server.back_fb_mapped;

    /* Allocate Shadow Buffer */
    g_server.shadow_fb = (uint32_t *)calloc((size_t)(g_server.width * g_server.height), sizeof(uint32_t));
    if (!g_server.shadow_fb) {
        fprintf(stderr, "[SzpontX11] Failed to allocate shadow framebuffer\n");
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        close(g_server.drm_fd);
        return false;
    }

    /* Set CRTC mode scanning out from front buffer */
    if (drmModeSetCrtc(g_server.drm_fd, g_server.crtc_id, g_server.front_fb_id, 0, 0,
                       &g_server.conn_id, 1, &mode) < 0) {
        perror("[SzpontX11] drmModeSetCrtc failed");
    }

    drmModeFreeConnector(conn);
    drmModeFreeResources(res);

    printf("[SzpontX11] DRM/KMS Hardware Double Buffering ready (Front FB: %u, Back FB: %u)\n",
           g_server.front_fb_id, g_server.back_fb_id);
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
        }
        if (g_server.back_fb_mapped) {
            munmap(g_server.back_fb_mapped, (size_t)g_server.pitch * g_server.height);
            g_server.back_fb_mapped = NULL;
        }
        if (g_server.front_fb_id) {
            drmModeRmFB(g_server.drm_fd, g_server.front_fb_id);
            g_server.front_fb_id = 0;
        }
        if (g_server.back_fb_id) {
            drmModeRmFB(g_server.drm_fd, g_server.back_fb_id);
            g_server.back_fb_id = 0;
        }
        if (g_server.front_dumb_handle) {
            struct drm_mode_destroy_dumb req = {.handle = g_server.front_dumb_handle};
            drmIoctl(g_server.drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &req);
            g_server.front_dumb_handle = 0;
        }
        if (g_server.back_dumb_handle) {
            struct drm_mode_destroy_dumb req = {.handle = g_server.back_dumb_handle};
            drmIoctl(g_server.drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &req);
            g_server.back_dumb_handle = 0;
        }
        close(g_server.drm_fd);
        g_server.drm_fd = -1;
    }
}

void drm_swap_buffers(void) {
    if (g_server.drm_fd < 0 || !g_server.front_fb_mapped || !g_server.back_fb_mapped)
        return;

    /* Hardware Page Flip: Switch scanout to back buffer */
    drmModePageFlip(g_server.drm_fd, g_server.crtc_id, g_server.back_fb_id, 0, NULL);

    /* Swap Front and Back buffer pointers */
    uint32_t tmp_fb = g_server.front_fb_id;
    uint32_t tmp_handle = g_server.front_dumb_handle;
    uint32_t *tmp_map = g_server.front_fb_mapped;

    g_server.front_fb_id = g_server.back_fb_id;
    g_server.front_dumb_handle = g_server.back_dumb_handle;
    g_server.front_fb_mapped = g_server.back_fb_mapped;

    g_server.back_fb_id = tmp_fb;
    g_server.back_dumb_handle = tmp_handle;
    g_server.back_fb_mapped = tmp_map;

    g_server.fb_id = g_server.back_fb_id;
    g_server.dumb_handle = g_server.back_dumb_handle;
    g_server.fb_mapped = g_server.back_fb_mapped;
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

    drmModeClip clip;
    clip.x1 = (uint16_t)x0;
    clip.y1 = (uint16_t)y0;
    clip.x2 = (uint16_t)x1;
    clip.y2 = (uint16_t)y1;
    drmModeDirtyFB(g_server.drm_fd, g_server.fb_id, &clip, 1);
}

void drm_flush_screen(void) {
    if (!g_server.fb_mapped || !g_server.shadow_fb) return;
    memcpy(g_server.fb_mapped, g_server.shadow_fb,
           (size_t)(g_server.width * g_server.height) * sizeof(uint32_t));
    drm_swap_buffers();
}
