/*
 * SzpontOS - DRM/KMS Direct Graphics & Evdev Test Utility
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>
#include <poll.h>
#include <errno.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <linux/input.h>

#define COLOR_BG        0x000F111A
#define COLOR_PANEL     0x001A1C2E
#define COLOR_BORDER    0x002E3450
#define COLOR_ACCENT    0x0082AAFF
#define COLOR_TEXT      0x00E0E0E0
#define COLOR_RED       0x00FF5370
#define COLOR_GREEN     0x00C3E88D
#define COLOR_YELLOW    0x00FFCB6B
#define COLOR_CYAN      0x0089DDFF
#define COLOR_MAGENTA   0x00C792EA

static void draw_rect(uint32_t *fb, uint32_t pitch_pixels, int x, int y, int w, int h, uint32_t color, int sw, int sh) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > sw) w = sw - x;
    if (y + h > sh) h = sh - y;
    if (w <= 0 || h <= 0) return;

    for (int r = 0; r < h; r++) {
        uint32_t *row = fb + (y + r) * pitch_pixels + x;
        for (int c = 0; c < w; c++) {
            row[c] = color;
        }
    }
}

static void draw_cursor(uint32_t *fb, uint32_t pitch_pixels, int mx, int my, int sw, int sh) {
    static const uint8_t cursor_shape[16][16] = {
        {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {1,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0},
        {1,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0},
        {1,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0},
        {1,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0},
        {1,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0},
        {1,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0},
        {1,2,2,2,2,1,1,1,1,1,0,0,0,0,0,0},
        {1,2,2,1,2,1,0,0,0,0,0,0,0,0,0,0},
        {1,2,1,0,1,2,1,0,0,0,0,0,0,0,0,0},
        {1,1,0,0,0,1,2,1,0,0,0,0,0,0,0,0},
        {1,0,0,0,0,0,1,2,1,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    };

    for (int cy = 0; cy < 16; cy++) {
        for (int cx = 0; cx < 16; cx++) {
            int px = mx + cx;
            int py = my + cy;
            if (px >= 0 && px < sw && py >= 0 && py < sh) {
                uint8_t pixel = cursor_shape[cy][cx];
                if (pixel == 1) {
                    fb[py * pitch_pixels + px] = 0x00000000; /* Black outline */
                } else if (pixel == 2) {
                    fb[py * pitch_pixels + px] = 0x00FFFFFF; /* White fill */
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("[drmtest] Opening SzpontOS DRM device (/dev/dri/card0)...\n");
    int drm_fd = drmOpen("szpont-drm", NULL);
    if (drm_fd < 0) {
        fprintf(stderr, "[drmtest] Error: Failed to open DRM device: %s\n", strerror(errno));
        return 1;
    }

    uint64_t dumb_cap = 0;
    drmGetCap(drm_fd, DRM_CAP_DUMB_BUFFER, &dumb_cap);
    printf("[drmtest] DRM dumb buffer support: %lu\n", dumb_cap);

    drmModeResPtr res = drmModeGetResources(drm_fd);
    if (!res) {
        fprintf(stderr, "[drmtest] Error: Failed to get DRM resources\n");
        close(drm_fd);
        return 1;
    }

    printf("[drmtest] Connectors: %d, Encoders: %d, CRTCs: %d\n",
           res->count_connectors, res->count_encoders, res->count_crtcs);

    if (res->count_connectors == 0 || res->count_crtcs == 0) {
        fprintf(stderr, "[drmtest] Error: No connectors or CRTCs found\n");
        drmModeFreeResources(res);
        close(drm_fd);
        return 1;
    }

    uint32_t conn_id = res->connectors[0];
    uint32_t crtc_id = res->crtcs[0];

    drmModeConnectorPtr conn = drmModeGetConnector(drm_fd, conn_id);
    if (!conn) {
        fprintf(stderr, "[drmtest] Error: Failed to get connector %u\n", conn_id);
        drmModeFreeResources(res);
        close(drm_fd);
        return 1;
    }

    uint32_t width = 1280;
    uint32_t height = 960;
    drmModeModeInfo mode;
    memset(&mode, 0, sizeof(mode));

    if (conn->count_modes > 0) {
        memcpy(&mode, &conn->modes[0], sizeof(mode));
        width = mode.hdisplay;
        height = mode.vdisplay;
        printf("[drmtest] Detected video mode: %s (%ux%u @ %uHz)\n",
               mode.name, width, height, mode.vrefresh);
    } else {
        printf("[drmtest] Using default resolution: %ux%u\n", width, height);
    }

    /* 1. Allocate Dumb Buffer */
    uint32_t handle = 0;
    uint32_t pitch = 0;
    uint64_t size = 0;

    int ret = drmModeCreateDumb(drm_fd, width, height, 32, 0, &handle, &pitch, &size);
    if (ret != 0) {
        fprintf(stderr, "[drmtest] Error: drmModeCreateDumb failed\n");
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        close(drm_fd);
        return 1;
    }

    printf("[drmtest] Created Dumb Buffer: handle=%u, pitch=%u bytes, size=%lu bytes\n",
           handle, pitch, size);

    /* 2. Map Dumb Buffer into process memory */
    uint64_t offset = 0;
    ret = drmModeMapDumb(drm_fd, handle, &offset);
    if (ret != 0) {
        fprintf(stderr, "[drmtest] Error: drmModeMapDumb failed\n");
        drmModeDestroyDumb(drm_fd, handle);
        close(drm_fd);
        return 1;
    }

    uint32_t *fb_mem = (uint32_t *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd, (off_t)offset);
    if (fb_mem == MAP_FAILED || !fb_mem) {
        fprintf(stderr, "[drmtest] Error: mmap failed on DRM buffer\n");
        drmModeDestroyDumb(drm_fd, handle);
        close(drm_fd);
        return 1;
    }

    printf("[drmtest] Successfully mapped Dumb Buffer at user address %p\n", (void *)fb_mem);

    /* 3. Add Framebuffer and Set CRTC */
    uint32_t fb_id = 0;
    ret = drmModeAddFB(drm_fd, width, height, 24, 32, pitch, handle, &fb_id);
    if (ret != 0) {
        fprintf(stderr, "[drmtest] Error: drmModeAddFB failed\n");
        munmap(fb_mem, size);
        drmModeDestroyDumb(drm_fd, handle);
        close(drm_fd);
        return 1;
    }

    printf("[drmtest] Added FB id=%u, activating CRTC modeset...\n", fb_id);
    drmSetMaster(drm_fd);

    ret = drmModeSetCrtc(drm_fd, crtc_id, fb_id, 0, 0, &conn_id, 1, &mode);
    if (ret != 0) {
        fprintf(stderr, "[drmtest] Warning: drmModeSetCrtc returned %d\n", ret);
    }

    /* Open Mouse Device for interactive test */
    int mouse_fd = open("/dev/input/mice", O_RDONLY | O_NONBLOCK);
    if (mouse_fd < 0) {
        mouse_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    }

    int mouse_x = width / 2;
    int mouse_y = height / 2;
    int ball_x = 200, ball_y = 200;
    int ball_dx = 4, ball_dy = 3;

    uint32_t pitch_pixels = pitch / 4;

    printf("[drmtest] Entering 60 FPS graphics rendering loop (running 300 frames, ~5 seconds)...\n");

    for (int frame = 0; frame < 300; frame++) {
        /* Read mouse input */
        if (mouse_fd >= 0) {
            uint8_t mbuf[3];
            ssize_t n = read(mouse_fd, mbuf, 3);
            if (n >= 3) {
                int8_t mdx = (int8_t)mbuf[1];
                int8_t mdy = (int8_t)mbuf[2];
                mouse_x += mdx;
                mouse_y -= mdy; /* PS/2 Y axis is inverted */
                if (mouse_x < 0) mouse_x = 0;
                if (mouse_x >= (int)width) mouse_x = (int)width - 1;
                if (mouse_y < 0) mouse_y = 0;
                if (mouse_y >= (int)height) mouse_y = (int)height - 1;
            }
        }

        /* 1. Clear background */
        draw_rect(fb_mem, pitch_pixels, 0, 0, width, height, COLOR_BG, width, height);

        /* 2. Top App Bar */
        draw_rect(fb_mem, pitch_pixels, 0, 0, width, 48, COLOR_PANEL, width, height);
        draw_rect(fb_mem, pitch_pixels, 0, 48, width, 2, COLOR_ACCENT, width, height);

        /* 3. Window Card 1 */
        draw_rect(fb_mem, pitch_pixels, 80, 100, 480, 320, COLOR_PANEL, width, height);
        draw_rect(fb_mem, pitch_pixels, 80, 100, 480, 36, COLOR_BORDER, width, height);
        draw_rect(fb_mem, pitch_pixels, 95, 112, 12, 12, COLOR_RED, width, height);
        draw_rect(fb_mem, pitch_pixels, 115, 112, 12, 12, COLOR_YELLOW, width, height);
        draw_rect(fb_mem, pitch_pixels, 135, 112, 12, 12, COLOR_GREEN, width, height);

        /* Color palette swatches in Card 1 */
        uint32_t palette[] = { COLOR_RED, COLOR_YELLOW, COLOR_GREEN, COLOR_CYAN, COLOR_ACCENT, COLOR_MAGENTA };
        for (int i = 0; i < 6; i++) {
            draw_rect(fb_mem, pitch_pixels, 110 + i * 70, 170, 50, 50, palette[i], width, height);
        }

        /* 4. Window Card 2 - Animated Bouncing Ball */
        draw_rect(fb_mem, pitch_pixels, 620, 100, 560, 480, COLOR_PANEL, width, height);
        draw_rect(fb_mem, pitch_pixels, 620, 100, 560, 36, COLOR_BORDER, width, height);

        ball_x += ball_dx;
        ball_y += ball_dy;
        if (ball_x <= 630 || ball_x + 40 >= 620 + 550) ball_dx = -ball_dx;
        if (ball_y <= 146 || ball_y + 40 >= 100 + 470) ball_dy = -ball_dy;

        draw_rect(fb_mem, pitch_pixels, ball_x, ball_y, 40, 40, COLOR_CYAN, width, height);

        /* 5. Draw interactive mouse cursor */
        draw_cursor(fb_mem, pitch_pixels, mouse_x, mouse_y, width, height);

        /* 6. Flush Dirty Rectangles */
        drmModeDirtyFB(drm_fd, fb_id, NULL, 0);

        usleep(16666); /* ~60 FPS */
    }

    printf("[drmtest] Graphics test completed successfully! Restoring text console...\n");

    if (mouse_fd >= 0) close(mouse_fd);
    drmDropMaster(drm_fd);
    munmap(fb_mem, size);
    drmModeRmFB(drm_fd, fb_id);
    drmModeDestroyDumb(drm_fd, handle);
    drmModeFreeConnector(conn);
    drmModeFreeResources(res);
    close(drm_fd);

    return 0;
}
