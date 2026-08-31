/*
 * SzpontOS - SzpontX11 Native X11 Server
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * 2D Graphics Rasterizer, Font Engine, Compositor and Mouse Cursor Overlay
 */

#include "xserver.h"
#include "font8x16.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

void draw_init_system(void) {
    /* Ready */
}

static inline uint32_t apply_rop(uint32_t src, uint32_t dst, uint8_t rop) {
    switch (rop) {
    case GXclear:        return 0x00000000;
    case GXand:          return src & dst;
    case GXandReverse:   return src & ~dst;
    case GXcopy:         return src;
    case GXandInverted:  return ~src & dst;
    case GXnoop:         return dst;
    case GXxor:          return src ^ dst;
    case GXor:           return src | dst;
    case GXnor:          return ~(src | dst);
    case GXequiv:        return ~(src ^ dst);
    case GXinvert:       return ~dst;
    case GXorReverse:    return src | ~dst;
    case GXcopyInverted: return ~src;
    case GXorInverted:   return ~src | dst;
    case GXnand:         return ~(src & dst);
    case GXset:          return 0xFFFFFFFF;
    default:             return src;
    }
}

void draw_fill_rect(uint32_t *dst, int pitch, int dst_w, int dst_h, int x, int y, int w, int h, uint32_t color, uint8_t rop) {
    if (!dst || w <= 0 || h <= 0) return;

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w > dst_w) ? dst_w : (x + w);
    int y1 = (y + h > dst_h) ? dst_h : (y + h);

    if (x0 >= x1 || y0 >= y1) return;

    int stride = pitch / 4;
    for (int r = y0; r < y1; r++) {
        uint32_t *row = &dst[r * stride];
        if (rop == GXcopy) {
            for (int c = x0; c < x1; c++) {
                row[c] = color;
            }
        } else {
            for (int c = x0; c < x1; c++) {
                row[c] = apply_rop(color, row[c], rop);
            }
        }
    }
}

void draw_rect(uint32_t *dst, int pitch, int dst_w, int dst_h, int x, int y, int w, int h, uint32_t color, int line_width) {
    if (line_width < 1) line_width = 1;
    draw_fill_rect(dst, pitch, dst_w, dst_h, x, y, w, line_width, color, GXcopy);
    draw_fill_rect(dst, pitch, dst_w, dst_h, x, y + h - line_width, w, line_width, color, GXcopy);
    draw_fill_rect(dst, pitch, dst_w, dst_h, x, y, line_width, h, color, GXcopy);
    draw_fill_rect(dst, pitch, dst_w, dst_h, x + w - line_width, y, line_width, h, color, GXcopy);
}

void draw_line(uint32_t *dst, int pitch, int dst_w, int dst_h, int x1, int y1, int x2, int y2, uint32_t color, int line_width) {
    (void)line_width;
    if (!dst) return;
    int stride = pitch / 4;

    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        if (x1 >= 0 && x1 < dst_w && y1 >= 0 && y1 < dst_h) {
            dst[y1 * stride + x1] = color;
        }
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void draw_arc(uint32_t *dst, int pitch, int dst_w, int dst_h, int x, int y, int w, int h, int angle1, int angle2, uint32_t color, bool fill) {
    (void)angle1; (void)angle2;
    if (!dst || w <= 0 || h <= 0) return;
    int stride = pitch / 4;

    int a = w / 2;
    int b = h / 2;
    int cx = x + a;
    int cy = y + b;

    if (fill) {
        for (int dy = -b; dy <= b; dy++) {
            int py = cy + dy;
            if (py < 0 || py >= dst_h) continue;
            int dx = (int)(a * sqrtf(1.0f - (float)(dy * dy) / (float)(b * b > 0 ? b * b : 1)));
            int px0 = cx - dx;
            int px1 = cx + dx;
            if (px0 < 0) px0 = 0;
            if (px1 >= dst_w) px1 = dst_w - 1;
            for (int px = px0; px <= px1; px++) {
                dst[py * stride + px] = color;
            }
        }
    } else {
        int steps = (w + h) * 2;
        if (steps < 32) steps = 32;
        for (int i = 0; i < steps; i++) {
            float rad = (float)i * (6.2831853f / (float)steps);
            int px = cx + (int)(cosf(rad) * a);
            int py = cy + (int)(sinf(rad) * b);
            if (px >= 0 && px < dst_w && py >= 0 && py < dst_h) {
                dst[py * stride + px] = color;
            }
        }
    }
}

void draw_glyph(uint32_t *dst, int pitch, int dst_w, int dst_h, int x, int y, uint8_t char_code, uint32_t fg, uint32_t bg, bool transparent) {
    if (!dst) return;
    int stride = pitch / 4;
    const uint8_t *glyph = g_szpont_font8x16[char_code];

    for (int r = 0; r < 16; r++) {
        int py = y + r;
        if (py < 0 || py >= dst_h) continue;
        uint8_t row = glyph[r];
        for (int c = 0; c < 8; c++) {
            int px = x + c;
            if (px < 0 || px >= dst_w) continue;
            if ((row >> (7 - c)) & 1) {
                dst[py * stride + px] = fg;
            } else if (!transparent) {
                dst[py * stride + px] = bg;
            }
        }
    }
}

void draw_text(uint32_t *dst, int pitch, int dst_w, int dst_h, int x, int y, const char *str, size_t len, uint32_t fg, uint32_t bg, bool transparent) {
    if (!dst || !str || len == 0) return;
    int cur_x = x;
    int cur_y = y;

    for (size_t i = 0; i < len; i++) {
        uint8_t ch = (uint8_t)str[i];
        if (ch == '\n') {
            cur_x = x;
            cur_y += 16;
            continue;
        }
        draw_glyph(dst, pitch, dst_w, dst_h, cur_x, cur_y, ch, fg, bg, transparent);
        cur_x += 8;
    }
}

void draw_blit(uint32_t *dst, int dst_pitch, int dst_w, int dst_h, int dst_x, int dst_y,
               const uint32_t *src, int src_pitch, int src_w, int src_h, int src_x, int src_y, int w, int h) {
    if (!dst || !src || w <= 0 || h <= 0) return;

    int dst_stride = dst_pitch / 4;
    int src_stride = src_pitch / 4;

    for (int r = 0; r < h; r++) {
        int sy = src_y + r;
        int dy = dst_y + r;
        if (sy < 0 || sy >= src_h || dy < 0 || dy >= dst_h) continue;

        for (int c = 0; c < w; c++) {
            int sx = src_x + c;
            int dx = dst_x + c;
            if (sx < 0 || sx >= src_w || dx < 0 || dx >= dst_w) continue;

            dst[dy * dst_stride + dx] = src[sy * src_stride + sx];
        }
    }
}

/* 24x24 Modern High-DPI Arrow Cursor with Drop Shadow */
static const uint32_t g_cursor_bitmap_24[24] = {
    0b100000000000000000000000,
    0b110000000000000000000000,
    0b111000000000000000000000,
    0b111100000000000000000000,
    0b111110000000000000000000,
    0b111111000000000000000000,
    0b111111100000000000000000,
    0b111111110000000000000000,
    0b111111111000000000000000,
    0b111111111100000000000000,
    0b111111111110000000000000,
    0b111111111111000000000000,
    0b111111111111100000000000,
    0b111111111111110000000000,
    0b111111111100000000000000,
    0b111101111110000000000000,
    0b111000111111000000000000,
    0b110000011111100000000000,
    0b100000001111110000000000,
    0b000000000111111000000000,
    0b000000000011111100000000,
    0b000000000001111100000000,
    0b000000000000111000000000,
    0b000000000000000000000000,
};

static const uint32_t g_cursor_outline_24[24] = {
    0b110000000000000000000000,
    0b111000000000000000000000,
    0b111100000000000000000000,
    0b111110000000000000000000,
    0b111111000000000000000000,
    0b111111100000000000000000,
    0b111111110000000000000000,
    0b111111111000000000000000,
    0b111111111100000000000000,
    0b111111111110000000000000,
    0b111111111111000000000000,
    0b111111111111100000000000,
    0b111111111111110000000000,
    0b111111111111111000000000,
    0b111111111111111100000000,
    0b111111111111111100000000,
    0b111101111111111100000000,
    0b111001111111111100000000,
    0b110000111111111000000000,
    0b100000011111110000000000,
    0b000000001111110000000000,
    0b000000000111110000000000,
    0b000000000011100000000000,
    0b000000000000000000000000,
};

void draw_cursor(uint32_t *dst, int pitch, int dst_w, int dst_h, int cx, int cy) {
    if (!dst) return;
    int stride = pitch / 4;

    /* 1. Draw subtle dark drop shadow (+2, +2 offset) */
    for (int y = 0; y < 24; y++) {
        int py = cy + y + 2;
        if (py < 0 || py >= dst_h) continue;

        uint32_t outline = g_cursor_outline_24[y];
        for (int x = 0; x < 24; x++) {
            int px = cx + x + 2;
            if (px < 0 || px >= dst_w) continue;

            if ((outline >> (23 - x)) & 1) {
                uint32_t cur = dst[py * stride + px];
                uint8_t r = (uint8_t)((cur >> 16) & 0xFF) / 2;
                uint8_t g = (uint8_t)((cur >> 8) & 0xFF) / 2;
                uint8_t b = (uint8_t)(cur & 0xFF) / 2;
                dst[py * stride + px] = (0xFF << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }

    /* 2. Draw black border outline and crisp white body */
    for (int y = 0; y < 24; y++) {
        int py = cy + y;
        if (py < 0 || py >= dst_h) continue;

        uint32_t mask = g_cursor_bitmap_24[y];
        uint32_t outline = g_cursor_outline_24[y];

        for (int x = 0; x < 24; x++) {
            int px = cx + x;
            if (px < 0 || px >= dst_w) continue;

            if ((mask >> (23 - x)) & 1) {
                dst[py * stride + px] = 0xFFFFFFFF; /* Crisp snow white inside */
            } else if ((outline >> (23 - x)) & 1) {
                dst[py * stride + px] = 0xFF000000; /* Deep black outline */
            }
        }
    }
}

static void composite_window_recursive(window_t *win, uint32_t *dst, int dst_pitch, int dst_w, int dst_h) {
    if (!win) return;

    if (win->mapped) {
        int abs_x, abs_y;
        window_get_absolute_coords(win, &abs_x, &abs_y);

        /* Draw decorative titlebar and border for top-level application windows */
        if (win->parent == &g_server.root_window && win->id >= 0x500000 && win->width > 100 && win->height > 60) {
            uint32_t border_col = (g_server.focus_window == win || (g_server.focus_window && g_server.focus_window->parent == win)) ? 0xFF3B82F6 : 0xFF475569;

            /* Titlebar background (28px height above window) */
            draw_fill_rect(dst, dst_pitch, dst_w, dst_h, abs_x - 2, abs_y - 28, win->width + 4, 28, 0xFF1E293B, GXcopy);
            draw_fill_rect(dst, dst_pitch, dst_w, dst_h, abs_x - 2, abs_y - 28, win->width + 4, 2, border_col, GXcopy);

            /* Title text */
            draw_text(dst, dst_pitch, dst_w, dst_h, abs_x + 12, abs_y - 22, "XTerm — /bin/sh", 15, 0xFFE2E8F0, 0, true);

            /* Window action controls (close, minimize, maximize) */
            draw_fill_rect(dst, dst_pitch, dst_w, dst_h, abs_x + win->width - 18, abs_y - 20, 10, 10, 0xFFEF4444, GXcopy);
            draw_fill_rect(dst, dst_pitch, dst_w, dst_h, abs_x + win->width - 34, abs_y - 20, 10, 10, 0xFFF59E0B, GXcopy);
            draw_fill_rect(dst, dst_pitch, dst_w, dst_h, abs_x + win->width - 50, abs_y - 20, 10, 10, 0xFF10B981, GXcopy);

            /* Outer side and bottom borders */
            draw_fill_rect(dst, dst_pitch, dst_w, dst_h, abs_x - 2, abs_y, 2, win->height, border_col, GXcopy);
            draw_fill_rect(dst, dst_pitch, dst_w, dst_h, abs_x + win->width, abs_y, 2, win->height, border_col, GXcopy);
            draw_fill_rect(dst, dst_pitch, dst_w, dst_h, abs_x - 2, abs_y + win->height, win->width + 4, 2, border_col, GXcopy);
        } else if (win->border_width > 0) {
            draw_rect(dst, dst_pitch, dst_w, dst_h,
                      abs_x - win->border_width, abs_y - win->border_width,
                      win->width + win->border_width * 2, win->height + win->border_width * 2,
                      win->border_pixel, win->border_width);
        }

        /* Draw window backing pixmap */
        if (win->backing_pixmap && win->backing_pixmap->data) {
            draw_blit(dst, dst_pitch, dst_w, dst_h, abs_x, abs_y,
                      win->backing_pixmap->data, win->backing_pixmap->pitch,
                      win->width, win->height, 0, 0, win->width, win->height);
        }
    }

    /* Draw children in forward order */
    for (window_t *child = win->first_child; child != NULL; child = child->next_sibling) {
        composite_window_recursive(child, dst, dst_pitch, dst_w, dst_h);
    }
}

static uint32_t s_cursor_bg[32 * 32];
static int s_prev_cx = -1;
static int s_prev_cy = -1;
static bool s_has_saved_bg = false;

void draw_update_cursor(void) {
    if (!g_server.shadow_fb || !g_server.fb_mapped) return;

    int new_cx = g_server.mouse_x;
    int new_cy = g_server.mouse_y;
    int stride = g_server.pitch / 4;
    int dw = g_server.width;
    int dh = g_server.height;

    /* 1. Restore previous cursor background if we had one */
    if (s_has_saved_bg && s_prev_cx >= 0 && s_prev_cy >= 0) {
        for (int y = 0; y < 28; y++) {
            int py = s_prev_cy + y;
            if (py < 0 || py >= dh) continue;
            for (int x = 0; x < 28; x++) {
                int px = s_prev_cx + x;
                if (px < 0 || px >= dw) continue;
                g_server.shadow_fb[py * stride + px] = s_cursor_bg[y * 32 + x];
            }
        }
        drm_flush_rect(s_prev_cx, s_prev_cy, 28, 28);
    }

    /* 2. Save new background */
    for (int y = 0; y < 28; y++) {
        int py = new_cy + y;
        for (int x = 0; x < 28; x++) {
            int px = new_cx + x;
            if (py >= 0 && py < dh && px >= 0 && px < dw) {
                s_cursor_bg[y * 32 + x] = g_server.shadow_fb[py * stride + px];
            } else {
                s_cursor_bg[y * 32 + x] = 0;
            }
        }
    }
    s_has_saved_bg = true;
    s_prev_cx = new_cx;
    s_prev_cy = new_cy;

    /* 3. Draw cursor on shadow_fb */
    draw_cursor(g_server.shadow_fb, g_server.pitch, dw, dh, new_cx, new_cy);

    /* 4. Flush only the 28x28 cursor rect to DRM */
    drm_flush_rect(new_cx, new_cy, 28, 28);
}

void draw_composite_scene(void) {
    if (!g_server.shadow_fb) return;

    /* 1. Composite Root Window and all mapped children */
    composite_window_recursive(&g_server.root_window, g_server.shadow_fb,
                               g_server.pitch, g_server.width, g_server.height);

    /* 2. Save background under current cursor */
    int cx = g_server.mouse_x;
    int cy = g_server.mouse_y;
    int stride = g_server.pitch / 4;
    int dw = g_server.width;
    int dh = g_server.height;

    for (int y = 0; y < 28; y++) {
        int py = cy + y;
        for (int x = 0; x < 28; x++) {
            int px = cx + x;
            if (py >= 0 && py < dh && px >= 0 && px < dw) {
                s_cursor_bg[y * 32 + x] = g_server.shadow_fb[py * stride + px];
            } else {
                s_cursor_bg[y * 32 + x] = 0;
            }
        }
    }
    s_has_saved_bg = true;
    s_prev_cx = cx;
    s_prev_cy = cy;

    /* 3. Draw software cursor at live mouse position */
    draw_cursor(g_server.shadow_fb, g_server.pitch, g_server.width, g_server.height,
                g_server.mouse_x, g_server.mouse_y);

    /* 4. Flush Shadow Buffer to DRM Framebuffer */
    drm_flush_screen();
}
