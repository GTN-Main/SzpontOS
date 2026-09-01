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

    /* Clip source bounds */
    if (src_x < 0) { w += src_x; dst_x -= src_x; src_x = 0; }
    if (src_y < 0) { h += src_y; dst_y -= src_y; src_y = 0; }
    if (src_x + w > src_w) { w = src_w - src_x; }
    if (src_y + h > src_h) { h = src_h - src_y; }

    /* Clip destination bounds */
    if (dst_x < 0) { w += dst_x; src_x -= dst_x; dst_x = 0; }
    if (dst_y < 0) { h += dst_y; src_y -= dst_y; dst_y = 0; }
    if (dst_x + w > dst_w) { w = dst_w - dst_x; }
    if (dst_y + h > dst_h) { h = dst_h - dst_y; }

    if (w <= 0 || h <= 0) return;

    int dst_stride = dst_pitch / 4;
    int src_stride = src_pitch / 4;
    size_t copy_bytes = (size_t)w * sizeof(uint32_t);

    for (int r = 0; r < h; r++) {
        uint32_t *d_row = &dst[(dst_y + r) * dst_stride + dst_x];
        const uint32_t *s_row = &src[(src_y + r) * src_stride + src_x];
        memcpy(d_row, s_row, copy_bytes);
    }
}

/* =========================================================================
 * 24x24 Cursor Bitmaps & Outlines for Multiple Cursor Types
 * ========================================================================= */

typedef struct {
    int      hot_x;
    int      hot_y;
    uint32_t bitmap[24];
    uint32_t outline[24];
} cursor_def_t;

static const cursor_def_t g_cursor_defs[MAX_CURSOR_TYPES] = {
    /* [0] CURSOR_ARROW */
    {
        .hot_x = 0, .hot_y = 0,
        .bitmap = {
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
        },
        .outline = {
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
        }
    },
    /* [1] CURSOR_IBEAM */
    {
        .hot_x = 8, .hot_y = 11,
        .bitmap = {
            0b000000000000000000000000,
            0b000111111111000000000000,
            0b000000011000000000000000,
            0b000000011000000000000000,
            0b000000011000000000000000,
            0b000000011000000000000000,
            0b000000011000000000000000,
            0b000000011000000000000000,
            0b000000011000000000000000,
            0b000000011000000000000000,
            0b000000011000000000000000,
            0b000000011000000000000000,
            0b000000011000000000000000,
            0b000000011000000000000000,
            0b000000011000000000000000,
            0b000000011000000000000000,
            0b000000011000000000000000,
            0b000000011000000000000000,
            0b000000011000000000000000,
            0b000111111111000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
        },
        .outline = {
            0b001111111111100000000000,
            0b001111111111100000000000,
            0b000000111100000000000000,
            0b000000111100000000000000,
            0b000000111100000000000000,
            0b000000111100000000000000,
            0b000000111100000000000000,
            0b000000111100000000000000,
            0b000000111100000000000000,
            0b000000111100000000000000,
            0b000000111100000000000000,
            0b000000111100000000000000,
            0b000000111100000000000000,
            0b000000111100000000000000,
            0b000000111100000000000000,
            0b000000111100000000000000,
            0b000000111100000000000000,
            0b000000111100000000000000,
            0b001111111111100000000000,
            0b001111111111100000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
        }
    },
    /* [2] CURSOR_HAND */
    {
        .hot_x = 5, .hot_y = 1,
        .bitmap = {
            0b000011000000000000000000,
            0b000011000000000000000000,
            0b000011000000000000000000,
            0b000011001100000000000000,
            0b000011011110110000000000,
            0b000011011111111000000000,
            0b001111011111111000000000,
            0b011111111111111000000000,
            0b011111111111111000000000,
            0b011111111111111000000000,
            0b011111111111111000000000,
            0b001111111111110000000000,
            0b000111111111110000000000,
            0b000011111111100000000000,
            0b000011111111000000000000,
            0b000011111111000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
        },
        .outline = {
            0b000111100000000000000000,
            0b000111100000000000000000,
            0b000111100110000000000000,
            0b000111111111011000000000,
            0b000111111111111100000000,
            0b001111111111111110000000,
            0b011111111111111110000000,
            0b111111111111111110000000,
            0b111111111111111110000000,
            0b111111111111111110000000,
            0b111111111111111110000000,
            0b011111111111111100000000,
            0b001111111111111100000000,
            0b000111111111111000000000,
            0b000111111111110000000000,
            0b000111111111110000000000,
            0b000011111111000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
        }
    },
    /* [3] CURSOR_RESIZE_NWSE */
    {
        .hot_x = 11, .hot_y = 11,
        .bitmap = {
            0b111111100000000000000000,
            0b111111000000000000000000,
            0b111110000000000000000000,
            0b111110000000000000000000,
            0b111011000000000000000000,
            0b110001100000000000000000,
            0b100000110000000000000000,
            0b000000011000000000000000,
            0b000000001100000000000000,
            0b000000000110000000000000,
            0b000000000011000000000000,
            0b000000000001100000000000,
            0b000000000000110000000000,
            0b000000000000011000000000,
            0b000000000000001100000000,
            0b000000000000000110000001,
            0b000000000000000011000011,
            0b000000000000000001101111,
            0b000000000000000000111111,
            0b000000000000000000111111,
            0b000000000000000000011111,
            0b000000000000000000001111,
            0b000000000000000000000000,
            0b000000000000000000000000,
        },
        .outline = {
            0b111111110000000000000000,
            0b111111110000000000000000,
            0b111111100000000000000000,
            0b111111100000000000000000,
            0b111111110000000000000000,
            0b111011110000000000000000,
            0b110001111000000000000000,
            0b100000111100000000000000,
            0b000000011110000000000000,
            0b000000001111000000000000,
            0b000000000111100000000000,
            0b000000000011110000000000,
            0b000000000001111000000000,
            0b000000000000111100000000,
            0b000000000000011110000000,
            0b000000000000001111000011,
            0b000000000000000111100111,
            0b000000000000000011111111,
            0b000000000000000001111111,
            0b000000000000000001111111,
            0b000000000000000000111111,
            0b000000000000000000111111,
            0b000000000000000000000000,
            0b000000000000000000000000,
        }
    },
    /* [4] CURSOR_RESIZE_NESW */
    {
        .hot_x = 11, .hot_y = 11,
        .bitmap = {
            0b000000000000000011111110,
            0b000000000000000000111111,
            0b000000000000000000011111,
            0b000000000000000000011111,
            0b000000000000000000110111,
            0b000000000000000001100011,
            0b000000000000000011000001,
            0b000000000000000110000000,
            0b000000000000001100000000,
            0b000000000000011000000000,
            0b000000000000110000000000,
            0b000000000001100000000000,
            0b000000000011000000000000,
            0b000000000110000000000000,
            0b000000001100000000000000,
            0b100000011000000000000000,
            0b110000110000000000000000,
            0b111011000000000000000000,
            0b111111000000000000000000,
            0b111111000000000000000000,
            0b111110000000000000000000,
            0b111100000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
        },
        .outline = {
            0b000000000000000011111111,
            0b000000000000000011111111,
            0b000000000000000001111111,
            0b000000000000000001111111,
            0b000000000000000011111111,
            0b000000000000000011110111,
            0b000000000000000111100011,
            0b000000000000001111000001,
            0b000000000000011110000000,
            0b000000000000111100000000,
            0b000000000001111000000000,
            0b000000000011110000000000,
            0b000000000111100000000000,
            0b000000001111000000000000,
            0b000000011110000000000000,
            0b110000111100000000000000,
            0b111001111000000000000000,
            0b111111110000000000000000,
            0b111111100000000000000000,
            0b111111100000000000000000,
            0b111111000000000000000000,
            0b111111000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
        }
    },
    /* [5] CURSOR_RESIZE_WE */
    {
        .hot_x = 11, .hot_y = 11,
        .bitmap = {
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000010000000000001000000,
            0b000110000000000011000000,
            0b001110000000000111000000,
            0b011110000000001111000000,
            0b111111111111111111100000,
            0b111111111111111111100000,
            0b011110000000001111000000,
            0b001110000000000111000000,
            0b000110000000000011000000,
            0b000010000000000001000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
        },
        .outline = {
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000011000000000011000000,
            0b000111000000000111000000,
            0b001111000000001111000000,
            0b011111000000011111000000,
            0b111111111111111111110000,
            0b111111111111111111110000,
            0b111111111111111111110000,
            0b111111111111111111110000,
            0b011111000000011111000000,
            0b001111000000001111000000,
            0b000111000000000111000000,
            0b000011000000000011000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
        }
    },
    /* [6] CURSOR_RESIZE_NS */
    {
        .hot_x = 11, .hot_y = 11,
        .bitmap = {
            0b000000000100000000000000,
            0b000000001110000000000000,
            0b000000011111000000000000,
            0b000000111111100000000000,
            0b000000001110000000000000,
            0b000000001110000000000000,
            0b000000001110000000000000,
            0b000000001110000000000000,
            0b000000001110000000000000,
            0b000000001110000000000000,
            0b000000001110000000000000,
            0b000000001110000000000000,
            0b000000001110000000000000,
            0b000000001110000000000000,
            0b000000001110000000000000,
            0b000000001110000000000000,
            0b000000111111100000000000,
            0b000000011111000000000000,
            0b000000001110000000000000,
            0b000000000100000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
        },
        .outline = {
            0b000000001110000000000000,
            0b000000011111000000000000,
            0b000000111111100000000000,
            0b000001111111110000000000,
            0b000000011111000000000000,
            0b000000011111000000000000,
            0b000000011111000000000000,
            0b000000011111000000000000,
            0b000000011111000000000000,
            0b000000011111000000000000,
            0b000000011111000000000000,
            0b000000011111000000000000,
            0b000000011111000000000000,
            0b000000011111000000000000,
            0b000000011111000000000000,
            0b000001111111110000000000,
            0b000000111111100000000000,
            0b000000011111000000000000,
            0b000000001110000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
        }
    },
    /* [7] CURSOR_MOVE (4-way crosshair) */
    {
        .hot_x = 11, .hot_y = 11,
        .bitmap = {
            0b000000000100000000000000,
            0b000000001110000000000000,
            0b000000011111000000000000,
            0b000000001110000000000000,
            0b000000001110000000000000,
            0b000010001110001000000000,
            0b000110001110011000000000,
            0b001111111111111000000000,
            0b011111111111111100000000,
            0b001111111111111000000000,
            0b000110001110011000000000,
            0b000010001110001000000000,
            0b000000001110000000000000,
            0b000000001110000000000000,
            0b000000011111000000000000,
            0b000000001110000000000000,
            0b000000000100000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
        },
        .outline = {
            0b000000001110000000000000,
            0b000000011111000000000000,
            0b000000111111100000000000,
            0b000000011111000000000000,
            0b000011011111011000000000,
            0b000111011111011100000000,
            0b001111111111111100000000,
            0b011111111111111110000000,
            0b111111111111111111000000,
            0b011111111111111110000000,
            0b001111111111111100000000,
            0b000111011111011100000000,
            0b000011011111011000000000,
            0b000000011111000000000000,
            0b000000111111100000000000,
            0b000000011111000000000000,
            0b000000001110000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
        }
    },
    /* [8] CURSOR_WAIT (Hourglass) */
    {
        .hot_x = 11, .hot_y = 11,
        .bitmap = {
            0b000111111111111000000000,
            0b000111111111111000000000,
            0b000011111111110000000000,
            0b000001111111100000000000,
            0b000000111111000000000000,
            0b000000011110000000000000,
            0b000000001100000000000000,
            0b000000011110000000000000,
            0b000000111111000000000000,
            0b000001111111100000000000,
            0b000011111111110000000000,
            0b000111111111111000000000,
            0b000111111111111000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
        },
        .outline = {
            0b001111111111111100000000,
            0b001111111111111100000000,
            0b000111111111111000000000,
            0b000011111111110000000000,
            0b000001111111100000000000,
            0b000000111111000000000000,
            0b000000011110000000000000,
            0b000000111111000000000000,
            0b000001111111100000000000,
            0b000011111111110000000000,
            0b000111111111111000000000,
            0b001111111111111100000000,
            0b001111111111111100000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
        }
    },
    /* [9] CURSOR_CROSSHAIR */
    {
        .hot_x = 11, .hot_y = 11,
        .bitmap = {
            0b000000000001100000000000,
            0b000000000001100000000000,
            0b000000000001100000000000,
            0b000000000001100000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b111100000000000000001111,
            0b111100000000000000001111,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000001100000000000,
            0b000000000001100000000000,
            0b000000000001100000000000,
            0b000000000001100000000000,
        },
        .outline = {
            0b000000000011110000000000,
            0b000000000011110000000000,
            0b000000000011110000000000,
            0b000000000011110000000000,
            0b000000000011110000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b111110000000000000011111,
            0b111110000000000000011111,
            0b111110000000000000011111,
            0b111110000000000000011111,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000000000000000000,
            0b000000000011110000000000,
            0b000000000011110000000000,
            0b000000000011110000000000,
            0b000000000011110000000000,
            0b000000000011110000000000,
        }
    }
};

void draw_cursor(uint32_t *dst, int pitch, int dst_w, int dst_h, int cx, int cy) {
    if (!dst) return;
    int stride = pitch / 4;

    int ctype = g_server.active_cursor_type;
    if (ctype < 0 || ctype >= MAX_CURSOR_TYPES) ctype = CURSOR_ARROW;

    const cursor_def_t *cdef = &g_cursor_defs[ctype];
    int ox = cx - cdef->hot_x;
    int oy = cy - cdef->hot_y;

    /* 1. Draw subtle dark drop shadow (+2, +2 offset) */
    for (int y = 0; y < 24; y++) {
        int py = oy + y + 2;
        if (py < 0 || py >= dst_h) continue;

        uint32_t outline = cdef->outline[y];
        for (int x = 0; x < 24; x++) {
            int px = ox + x + 2;
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
        int py = oy + y;
        if (py < 0 || py >= dst_h) continue;

        uint32_t mask = cdef->bitmap[y];
        uint32_t outline = cdef->outline[y];

        for (int x = 0; x < 24; x++) {
            int px = ox + x;
            if (px < 0 || px >= dst_w) continue;

            if ((mask >> (23 - x)) & 1) {
                dst[py * stride + px] = 0xFFFFFFFF; /* Crisp snow white inside */
            } else if ((outline >> (23 - x)) & 1) {
                dst[py * stride + px] = 0xFF000000; /* Deep black outline */
            }
        }
    }
}

static void draw_traffic_light(uint32_t *dst, int pitch, int dst_w, int dst_h, int cx, int cy, int r, uint32_t color) {
    if (!dst) return;
    int stride = pitch / 4;
    for (int dy = -r; dy <= r; dy++) {
        int py = cy + dy;
        if (py < 0 || py >= dst_h) continue;
        for (int dx = -r; dx <= r; dx++) {
            int px = cx + dx;
            if (px < 0 || px >= dst_w) continue;
            if (dx * dx + dy * dy <= r * r) {
                dst[py * stride + px] = color;
            }
        }
    }
}

static void draw_window_shadow(uint32_t *dst, int pitch, int dst_w, int dst_h, int x, int y, int w, int h) {
    if (!dst) return;
    int stride = pitch / 4;
    int shadow_size = 6;

    int sx0 = x - shadow_size;
    int sy0 = y - shadow_size;
    int sx1 = x + w + shadow_size;
    int sy1 = y + h + shadow_size;

    if (sx0 < 0) sx0 = 0;
    if (sy0 < 0) sy0 = 0;
    if (sx1 > dst_w) sx1 = dst_w;
    if (sy1 > dst_h) sy1 = dst_h;

    for (int r = sy0; r < sy1; r++) {
        uint32_t *row = &dst[r * stride];
        for (int c = sx0; c < sx1; c++) {
            if (c >= x && c < x + w && r >= y && r < y + h)
                continue;

            uint32_t cur = row[c];
            uint8_t cr = (uint8_t)((cur >> 16) & 0xFF) / 2;
            uint8_t cg = (uint8_t)((cur >> 8) & 0xFF) / 2;
            uint8_t cb = (uint8_t)(cur & 0xFF) / 2;
            row[c] = (0xFF << 24) | (cr << 16) | (cg << 8) | cb;
        }
    }
}

static void composite_window_recursive(window_t *win, uint32_t *dst, int dst_pitch, int dst_w, int dst_h) {
    if (!win) return;

    if (win->mapped) {
        int abs_x, abs_y;
        window_get_absolute_coords(win, &abs_x, &abs_y);

        /* Draw decorative titlebar, drop shadow, and glowing border for top-level decorated windows */
        if (win->is_decorated && win->width > 60 && win->height > 40) {
            bool is_focused = (g_server.focus_window == win || (g_server.focus_window && g_server.focus_window->parent == win));
            uint32_t border_col = is_focused ? 0xFF38BDF8 : 0xFF334155;
            uint32_t titlebar_bg = is_focused ? 0xFF181825 : 0xFF11111B;
            uint32_t title_text_col = is_focused ? 0xFFF8FAFC : 0xFF94A3B8;

            /* 1. Ambient Drop Shadow */
            draw_window_shadow(dst, dst_pitch, dst_w, dst_h,
                               abs_x - WINDOW_BORDER_W, abs_y - TITLEBAR_HEIGHT,
                               win->width + WINDOW_BORDER_W * 2, win->height + TITLEBAR_HEIGHT + WINDOW_BORDER_W);

            /* 2. Titlebar background (28px height above window) */
            draw_fill_rect(dst, dst_pitch, dst_w, dst_h,
                           abs_x - WINDOW_BORDER_W, abs_y - TITLEBAR_HEIGHT,
                           win->width + WINDOW_BORDER_W * 2, TITLEBAR_HEIGHT, titlebar_bg, GXcopy);

            /* Top Accent line */
            draw_fill_rect(dst, dst_pitch, dst_w, dst_h,
                           abs_x - WINDOW_BORDER_W, abs_y - TITLEBAR_HEIGHT,
                           win->width + WINDOW_BORDER_W * 2, 2, border_col, GXcopy);

            /* 3. Traffic Light Controls (Close: Red, Minimize: Yellow, Maximize: Green) */
            draw_traffic_light(dst, dst_pitch, dst_w, dst_h, abs_x + 14, abs_y - TITLEBAR_HEIGHT + 14, 5, 0xFFEF4444);
            draw_traffic_light(dst, dst_pitch, dst_w, dst_h, abs_x + 30, abs_y - TITLEBAR_HEIGHT + 14, 5, 0xFFF59E0B);
            draw_traffic_light(dst, dst_pitch, dst_w, dst_h, abs_x + 46, abs_y - TITLEBAR_HEIGHT + 14, 5, 0xFF10B981);

            /* 4. Window Title Text */
            const char *title = win->title[0] ? win->title : "SzpontOS Application";
            draw_text(dst, dst_pitch, dst_w, dst_h, abs_x + 64, abs_y - TITLEBAR_HEIGHT + 6, title, strlen(title), title_text_col, 0, true);

            /* 5. Outer side and bottom borders */
            draw_fill_rect(dst, dst_pitch, dst_w, dst_h, abs_x - WINDOW_BORDER_W, abs_y, WINDOW_BORDER_W, win->height, border_col, GXcopy);
            draw_fill_rect(dst, dst_pitch, dst_w, dst_h, abs_x + win->width, abs_y, WINDOW_BORDER_W, win->height, border_col, GXcopy);
            draw_fill_rect(dst, dst_pitch, dst_w, dst_h, abs_x - WINDOW_BORDER_W, abs_y + win->height, win->width + WINDOW_BORDER_W * 2, WINDOW_BORDER_W, border_col, GXcopy);
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
static int s_prev_ox = -1;
static int s_prev_oy = -1;
static bool s_has_saved_bg = false;

static void update_active_cursor_type(void) {
    int ctype = CURSOR_ARROW;
    if (g_server.dragging_window) {
        ctype = CURSOR_MOVE;
    } else if (g_server.grab_window) {
        ctype = CURSOR_RESIZE_NWSE;
    } else {
        window_t *win = g_server.pointer_window;
        if (win && win != &g_server.root_window) {
            int abs_x, abs_y;
            window_get_absolute_coords(win, &abs_x, &abs_y);
            int rel_x = g_server.mouse_x - abs_x;
            int rel_y = g_server.mouse_y - abs_y;

            if (rel_y < 0 && rel_y >= -TITLEBAR_HEIGHT) {
                if (rel_x >= 8 && rel_x <= 56) {
                    ctype = CURSOR_HAND;
                } else {
                    ctype = CURSOR_MOVE;
                }
            } else if (win->id == 0x400001 || (win->y == 0 && win->height <= 36)) {
                if (rel_x >= 160 && rel_x <= 800) {
                    ctype = CURSOR_HAND;
                }
            } else if (rel_x >= (int)win->width - 24 && rel_y >= (int)win->height - 24) {
                ctype = CURSOR_RESIZE_NWSE;
            } else if (rel_x >= (int)win->width - 6) {
                ctype = CURSOR_RESIZE_WE;
            } else if (rel_y >= (int)win->height - 6) {
                ctype = CURSOR_RESIZE_NS;
            } else if (win->backing_pixmap && win->id != 0x400001) {
                ctype = CURSOR_IBEAM;
            }
        }
    }
    g_server.active_cursor_type = (uint8_t)ctype;
}

void draw_update_cursor(void) {
    if (!g_server.shadow_fb || !g_server.fb_mapped) return;

    update_active_cursor_type();

    int new_cx = g_server.mouse_x;
    int new_cy = g_server.mouse_y;
    int new_ox = new_cx - 16;
    int new_oy = new_cy - 16;
    int stride = g_server.pitch / 4;
    int dw = g_server.width;
    int dh = g_server.height;

    /* 1. Restore previous cursor background if we had one */
    if (s_has_saved_bg && s_prev_ox != -1 && s_prev_oy != -1) {
        for (int y = 0; y < 32; y++) {
            int py = s_prev_oy + y;
            if (py < 0 || py >= dh) continue;
            for (int x = 0; x < 32; x++) {
                int px = s_prev_ox + x;
                if (px < 0 || px >= dw) continue;
                g_server.shadow_fb[py * stride + px] = s_cursor_bg[y * 32 + x];
            }
        }
        drm_flush_rect(s_prev_ox, s_prev_oy, 32, 32);
    }

    /* 2. Save new background */
    for (int y = 0; y < 32; y++) {
        int py = new_oy + y;
        for (int x = 0; x < 32; x++) {
            int px = new_ox + x;
            if (py >= 0 && py < dh && px >= 0 && px < dw) {
                s_cursor_bg[y * 32 + x] = g_server.shadow_fb[py * stride + px];
            } else {
                s_cursor_bg[y * 32 + x] = 0;
            }
        }
    }
    s_has_saved_bg = true;
    s_prev_ox = new_ox;
    s_prev_oy = new_oy;

    /* 3. Draw cursor on shadow_fb */
    draw_cursor(g_server.shadow_fb, g_server.pitch, dw, dh, new_cx, new_cy);

    /* 4. Flush only the 32x32 cursor rect to DRM */
    drm_flush_rect(new_ox, new_oy, 32, 32);
}

void draw_composite_scene(void) {
    if (!g_server.shadow_fb) return;

    update_active_cursor_type();

    /* 1. Composite Root Window and all mapped children */
    composite_window_recursive(&g_server.root_window, g_server.shadow_fb,
                               g_server.pitch, g_server.width, g_server.height);

    /* 2. Save background under current cursor */
    int cx = g_server.mouse_x;
    int cy = g_server.mouse_y;
    int ox = cx - 16;
    int oy = cy - 16;
    int stride = g_server.pitch / 4;
    int dw = g_server.width;
    int dh = g_server.height;

    for (int y = 0; y < 32; y++) {
        int py = oy + y;
        for (int x = 0; x < 32; x++) {
            int px = ox + x;
            if (py >= 0 && py < dh && px >= 0 && px < dw) {
                s_cursor_bg[y * 32 + x] = g_server.shadow_fb[py * stride + px];
            } else {
                s_cursor_bg[y * 32 + x] = 0;
            }
        }
    }
    s_has_saved_bg = true;
    s_prev_ox = ox;
    s_prev_oy = oy;

    /* 3. Draw software cursor at live mouse position */
    draw_cursor(g_server.shadow_fb, g_server.pitch, g_server.width, g_server.height,
                g_server.mouse_x, g_server.mouse_y);

    /* 4. Flush Shadow Buffer to DRM Framebuffer */
    drm_flush_screen();
}
