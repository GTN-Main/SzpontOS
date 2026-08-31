/*
 * SzpontOS - SzpontX11 Native X11 Server
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * DRM/KMS Hardware Display Backend & Dumb Buffer Memory Mapping
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

    /* Allocate Dumb Buffer */
    struct drm_mode_create_dumb create_req;
    memset(&create_req, 0, sizeof(create_req));
    create_req.width = g_server.width;
    create_req.height = g_server.height;
    create_req.bpp = 32;

    if (drmIoctl(g_server.drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req) < 0) {
        perror("[SzpontX11] DRM_IOCTL_MODE_CREATE_DUMB failed");
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        close(g_server.drm_fd);
        return false;
    }

    g_server.dumb_handle = create_req.handle;
    g_server.pitch = create_req.pitch;

    if (drmModeAddFB(g_server.drm_fd, g_server.width, g_server.height, 24, 32,
                     g_server.pitch, g_server.dumb_handle, &g_server.fb_id) < 0) {
        perror("[SzpontX11] drmModeAddFB failed");
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        close(g_server.drm_fd);
        return false;
    }

    /* Map Dumb Buffer into process memory */
    struct drm_mode_map_dumb map_req;
    memset(&map_req, 0, sizeof(map_req));
    map_req.handle = g_server.dumb_handle;

    if (drmIoctl(g_server.drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req) < 0) {
        perror("[SzpontX11] DRM_IOCTL_MODE_MAP_DUMB failed");
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        close(g_server.drm_fd);
        return false;
    }

    g_server.fb_mapped = (uint32_t *)mmap(NULL, (size_t)create_req.size,
                                          PROT_READ | PROT_WRITE, MAP_SHARED,
                                          g_server.drm_fd, (off_t)map_req.offset);

    if (g_server.fb_mapped == MAP_FAILED || !g_server.fb_mapped) {
        perror("[SzpontX11] mmap DRM framebuffer failed");
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        close(g_server.drm_fd);
        return false;
    }

    /* Allocate Shadow Buffer */
    g_server.shadow_fb = (uint32_t *)calloc((size_t)(g_server.width * g_server.height), sizeof(uint32_t));
    if (!g_server.shadow_fb) {
        fprintf(stderr, "[SzpontX11] Failed to allocate shadow framebuffer\n");
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        close(g_server.drm_fd);
        return false;
    }

    /* Apply mode to CRTC */
    if (drmModeSetCrtc(g_server.drm_fd, g_server.crtc_id, g_server.fb_id, 0, 0,
                       &g_server.conn_id, 1, &mode) < 0) {
        perror("[SzpontX11] drmModeSetCrtc failed");
    }

    drmModeFreeConnector(conn);
    drmModeFreeResources(res);

    printf("[SzpontX11] DRM/KMS Hardware Acceleration ready (%d KiB Framebuffer)\n",
           (int)(create_req.size / 1024));
    return true;
}

void drm_cleanup_display(void) {
    if (g_server.shadow_fb) {
        free(g_server.shadow_fb);
        g_server.shadow_fb = NULL;
    }
    if (g_server.drm_fd >= 0) {
        drmDropMaster(g_server.drm_fd);
        if (g_server.fb_mapped) {
            munmap(g_server.fb_mapped, (size_t)g_server.pitch * g_server.height);
            g_server.fb_mapped = NULL;
        }
        if (g_server.fb_id) {
            drmModeRmFB(g_server.drm_fd, g_server.fb_id);
            g_server.fb_id = 0;
        }
        if (g_server.dumb_handle) {
            struct drm_mode_destroy_dumb destroy_req;
            memset(&destroy_req, 0, sizeof(destroy_req));
            destroy_req.handle = g_server.dumb_handle;
            drmIoctl(g_server.drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
            g_server.dumb_handle = 0;
        }
        close(g_server.drm_fd);
        g_server.drm_fd = -1;
    }
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
    drmModeDirtyFB(g_server.drm_fd, g_server.fb_id, NULL, 0);
}
