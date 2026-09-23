#!/usr/bin/env python3
"""
SzpontOS High-Definition Modern App Icon Generator
Generates clean 48x48 RGBA PNG icons for XDG application themes
using only Python standard library (struct, zlib, math).
"""

import os
import math
import struct
import zlib

def write_png(filepath, width, height, rgba_bytes):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)

    header = b"\x89PNG\r\n\x1a\n"
    ihdr = chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))

    raw_data = bytearray()
    for y in range(height):
        raw_data.append(0)  # filter type 0 (None)
        raw_data.extend(rgba_bytes[y * width * 4 : (y + 1) * width * 4])

    idat = chunk(b"IDAT", zlib.compress(bytes(raw_data), level=9))
    iend = chunk(b"IEND", b"")

    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    with open(filepath, "wb") as f:
        f.write(header + ihdr + idat + iend)

class Canvas:
    def __init__(self, size=48):
        self.size = size
        self.pixels = [[(0, 0, 0, 0) for _ in range(size)] for _ in range(size)]

    def set_pixel(self, x, y, color):
        if 0 <= x < self.size and 0 <= y < self.size:
            r, g, b, a = color
            if a <= 0:
                return
            if a >= 255:
                self.pixels[y][x] = (r, g, b, 255)
            else:
                br, bg, bb, ba = self.pixels[y][x]
                alpha = a / 255.0
                inv_a = (1.0 - alpha) * (ba / 255.0)
                out_a = alpha + inv_a
                if out_a > 0:
                    out_r = int((r * alpha + br * inv_a) / out_a)
                    out_g = int((g * alpha + bg * inv_a) / out_a)
                    out_b = int((b * alpha + bb * inv_a) / out_a)
                    self.pixels[y][x] = (out_r, out_g, out_b, int(out_a * 255))

    def fill_squircle(self, cx, cy, radius, start_col, end_col, border_col=None):
        for y in range(self.size):
            for x in range(self.size):
                dx = abs(x - cx)
                dy = abs(y - cy)
                # Superellipse equation: (x/r)^4 + (y/r)^4 <= 1
                dist = (dx / radius) ** 4.0 + (dy / radius) ** 4.0
                if dist <= 1.0:
                    # Anti-aliasing on edge
                    alpha = 1.0
                    if dist > 0.85:
                        alpha = max(0.0, min(1.0, (1.0 - dist) / 0.15))
                    t = y / float(self.size)
                    r = int(start_col[0] + (end_col[0] - start_col[0]) * t)
                    g = int(start_col[1] + (end_col[1] - start_col[1]) * t)
                    b = int(start_col[2] + (end_col[2] - start_col[2]) * t)
                    self.set_pixel(x, y, (r, g, b, int(alpha * 255)))

        if border_col:
            for y in range(self.size):
                for x in range(self.size):
                    dx = abs(x - cx)
                    dy = abs(y - cy)
                    dist = (dx / radius) ** 4.0 + (dy / radius) ** 4.0
                    if 0.82 <= dist <= 1.0:
                        alpha = border_col[3] / 255.0
                        self.set_pixel(x, y, (border_col[0], border_col[1], border_col[2], int(alpha * 180)))

    def fill_rect(self, x1, y1, x2, y2, color):
        for y in range(max(0, y1), min(self.size, y2)):
            for x in range(max(0, x1), min(self.size, x2)):
                self.set_pixel(x, y, color)

    def draw_line(self, x1, y1, x2, y2, color, thickness=1):
        dx = x2 - x1
        dy = y2 - y1
        steps = int(max(abs(dx), abs(dy)) * 2) or 1
        for i in range(steps + 1):
            t = i / steps
            x = int(x1 + dx * t)
            y = int(y1 + dy * t)
            for ox in range(-thickness // 2, thickness // 2 + 1):
                for oy in range(-thickness // 2, thickness // 2 + 1):
                    self.set_pixel(x + ox, y + oy, color)

    def fill_circle(self, cx, cy, r, color):
        for y in range(int(cy - r - 1), int(cy + r + 2)):
            for x in range(int(cx - r - 1), int(cx + r + 2)):
                d = math.hypot(x - cx, y - cy)
                if d <= r:
                    self.set_pixel(x, y, color)
                elif d <= r + 0.8:
                    alpha = (r + 0.8 - d) / 0.8
                    self.set_pixel(x, y, (color[0], color[1], color[2], int(color[3] * alpha)))

    def to_bytes(self):
        buf = bytearray()
        for y in range(self.size):
            for x in range(self.size):
                r, g, b, a = self.pixels[y][x]
                buf.extend([r, g, b, a])
        return bytes(buf)

def generate_terminal_icon():
    c = Canvas(48)
    # Dark carbon glass squircle
    c.fill_squircle(23.5, 23.5, 20.0, (30, 36, 48), (14, 18, 26), (255, 255, 255, 45))
    # Window header bar
    c.fill_rect(8, 8, 40, 16, (20, 24, 34, 230))
    c.fill_circle(12, 12, 2, (255, 95, 87, 255))
    c.fill_circle(17, 12, 2, (254, 188, 46, 255))
    c.fill_circle(22, 12, 2, (40, 200, 64, 255))
    # Prompt >
    c.draw_line(13, 21, 19, 26, (56, 189, 248, 255), 2)
    c.draw_line(19, 26, 13, 31, (56, 189, 248, 255), 2)
    # Cursor _
    c.fill_rect(22, 29, 32, 32, (241, 245, 249, 255))
    return c.to_bytes()

def generate_info_icon():
    c = Canvas(48)
    # Sapphire / Cobalt squircle
    c.fill_squircle(23.5, 23.5, 20.0, (37, 99, 235), (29, 78, 216), (255, 255, 255, 60))
    # Inner chip badge
    c.fill_rect(13, 13, 35, 35, (15, 23, 42, 180))
    c.fill_rect(15, 15, 33, 33, (30, 58, 138, 255))
    # Centered 'i'
    c.fill_circle(24, 20, 2.5, (255, 255, 255, 255))
    c.fill_rect(22, 25, 26, 31, (255, 255, 255, 255))
    c.fill_rect(20, 25, 24, 27, (255, 255, 255, 255))
    c.fill_rect(20, 30, 28, 32, (255, 255, 255, 255))
    # Microchip pins
    for p in [18, 24, 30]:
        c.fill_rect(p, 10, p + 2, 13, (147, 197, 253, 255))
        c.fill_rect(p, 35, p + 2, 38, (147, 197, 253, 255))
        c.fill_rect(10, p, 13, p + 2, (147, 197, 253, 255))
        c.fill_rect(35, p, 38, p + 2, (147, 197, 253, 255))
    return c.to_bytes()

def generate_image_viewer_icon():
    c = Canvas(48)
    # Royal violet / sunset squircle
    c.fill_squircle(23.5, 23.5, 20.0, (124, 58, 237), (67, 56, 202), (255, 255, 255, 60))
    # Sun / Moon
    c.fill_circle(17, 18, 4.5, (253, 224, 71, 255))
    # Mountains
    c.draw_line(10, 34, 22, 22, (248, 250, 252, 255), 2)
    c.draw_line(22, 22, 32, 34, (248, 250, 252, 255), 2)
    for y in range(23, 35):
        w = (y - 22) * 1.0
        c.fill_rect(int(22 - w), y, int(22 + w), y + 1, (248, 250, 252, 240))
    c.draw_line(26, 34, 34, 25, (226, 232, 240, 255), 2)
    c.draw_line(34, 25, 40, 34, (226, 232, 240, 255), 2)
    for y in range(26, 35):
        w = (y - 25) * 0.9
        c.fill_rect(int(34 - w), y, int(34 + w), y + 1, (203, 213, 225, 240))
    return c.to_bytes()

def generate_text_editor_icon():
    c = Canvas(48)
    # Amber / Bronze squircle
    c.fill_squircle(23.5, 23.5, 20.0, (217, 119, 6), (180, 83, 9), (255, 255, 255, 60))
    # Document paper
    c.fill_rect(13, 10, 35, 38, (255, 255, 255, 255))
    # Dog-ear fold
    c.fill_rect(28, 10, 35, 17, (226, 232, 240, 255))
    c.draw_line(28, 10, 35, 17, (203, 213, 225, 255), 1)
    # Text lines
    c.fill_rect(17, 18, 26, 20, (100, 116, 139, 255))
    c.fill_rect(17, 23, 31, 25, (100, 116, 139, 255))
    c.fill_rect(17, 28, 29, 30, (100, 116, 139, 255))
    c.fill_rect(17, 33, 24, 35, (217, 119, 6, 255))
    return c.to_bytes()

def generate_games_icon():
    c = Canvas(48)
    # Emerald / Arcade squircle
    c.fill_squircle(23.5, 23.5, 20.0, (5, 150, 105), (4, 120, 87), (255, 255, 255, 60))
    # Gamepad body
    c.fill_rect(11, 18, 37, 32, (15, 23, 42, 240))
    c.fill_circle(14, 25, 6, (15, 23, 42, 240))
    c.fill_circle(34, 25, 6, (15, 23, 42, 240))
    # D-pad cross
    c.fill_rect(13, 23, 19, 27, (241, 245, 249, 255))
    c.fill_rect(15, 21, 17, 29, (241, 245, 249, 255))
    # Action buttons
    c.fill_circle(33, 22, 2, (239, 68, 68, 255))
    c.fill_circle(36, 25, 2, (59, 130, 246, 255))
    c.fill_circle(30, 25, 2, (234, 179, 8, 255))
    c.fill_circle(33, 28, 2, (16, 185, 129, 255))
    return c.to_bytes()

def generate_settings_icon():
    c = Canvas(48)
    # Neutral slate squircle
    c.fill_squircle(23.5, 23.5, 20.0, (71, 85, 105), (51, 65, 85), (255, 255, 255, 50))
    # Outer gear teeth
    for deg in range(0, 360, 45):
        rad = math.radians(deg)
        gx = 23.5 + math.cos(rad) * 11
        gy = 23.5 + math.sin(rad) * 11
        c.fill_circle(gx, gy, 4.0, (241, 245, 249, 255))
    # Gear body
    c.fill_circle(23.5, 23.5, 11, (241, 245, 249, 255))
    # Center hole
    c.fill_circle(23.5, 23.5, 5, (51, 65, 85, 255))
    return c.to_bytes()

def generate_monitor_icon():
    c = Canvas(48)
    # Dark carbon slate squircle
    c.fill_squircle(23.5, 23.5, 20.0, (30, 41, 59), (15, 23, 42), (255, 255, 255, 45))
    # Grid lines
    for gy in [16, 24, 32]:
        c.draw_line(8, gy, 40, gy, (51, 65, 85, 150), 1)
    for gx in [14, 24, 34]:
        c.draw_line(gx, 10, gx, 38, (51, 65, 85, 150), 1)
    # ECG Pulse Wave
    points = [
        (8, 24), (16, 24), (20, 14), (23, 34), (26, 21), (29, 27), (32, 24), (40, 24)
    ]
    for i in range(len(points) - 1):
        c.draw_line(points[i][0], points[i][1], points[i+1][0], points[i+1][1], (16, 185, 129, 255), 2)
    return c.to_bytes()

def generate_szpont_icon():
    c = Canvas(48)
    # Deep obsidian glass squircle
    c.fill_squircle(23.5, 23.5, 20.0, (15, 23, 42), (2, 6, 23), (56, 189, 248, 90))
    # Minimalist S logo
    # Top bar
    c.fill_rect(14, 13, 34, 17, (248, 250, 252, 255))
    # Top left drop
    c.fill_rect(14, 17, 19, 23, (248, 250, 252, 255))
    # Middle bar
    c.fill_rect(14, 22, 34, 26, (56, 189, 248, 255))
    # Bottom right drop
    c.fill_rect(29, 25, 34, 31, (56, 189, 248, 255))
    # Bottom bar
    c.fill_rect(14, 31, 34, 35, (56, 189, 248, 255))
    return c.to_bytes()

def generate_file_manager_icon():
    c = Canvas(48)
    # macOS Finder two-tone blue face squircle
    c.fill_squircle(23.5, 23.5, 20.0, (37, 99, 235), (29, 78, 216), (255, 255, 255, 70))
    # Left half lighter blue
    for y in range(4, 44):
        for x in range(4, 24):
            dx = abs(x - 23.5)
            dy = abs(y - 23.5)
            if (dx / 20.0)**4 + (dy / 20.0)**4 <= 1.0:
                c.pixels[y][x] = (56, 189, 248, 255)
    # Center dividing nose line
    c.draw_line(23, 14, 23, 29, (15, 23, 42, 230), 2)
    c.draw_line(23, 29, 27, 29, (15, 23, 42, 230), 2)
    # Left eye
    c.fill_circle(16, 20, 2.5, (15, 23, 42, 255))
    # Right eye
    c.fill_circle(31, 20, 2.5, (15, 23, 42, 255))
    # Smiling mouth arc
    mouth = [(14, 34), (18, 37), (23, 38), (28, 37), (32, 34)]
    for i in range(len(mouth) - 1):
        c.draw_line(mouth[i][0], mouth[i][1], mouth[i+1][0], mouth[i+1][1], (15, 23, 42, 240), 2)
    return c.to_bytes()

def main():
    base_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    hicolor_dir = os.path.join(base_dir, "userland", "skeleton", "usr", "share", "icons", "hicolor", "48x48", "apps")
    pixmaps_dir = os.path.join(base_dir, "userland", "skeleton", "usr", "share", "pixmaps")
    icons_root = os.path.join(base_dir, "userland", "skeleton", "usr", "share", "icons")

    for d in [hicolor_dir, pixmaps_dir, icons_root]:
        os.makedirs(d, exist_ok=True)

    icons = {
        "system-file-manager.png": generate_file_manager_icon(),
        "utilities-terminal.png": generate_terminal_icon(),
        "dialog-information.png": generate_info_icon(),
        "image-viewer.png": generate_image_viewer_icon(),
        "text-editor.png": generate_text_editor_icon(),
        "applications-games.png": generate_games_icon(),
        "preferences-system.png": generate_settings_icon(),
        "utilities-system-monitor.png": generate_monitor_icon(),
        "szpont.png": generate_szpont_icon(),
    }

    aliases = {
        "szponter.png": "system-file-manager.png",
        "szponterm.png": "utilities-terminal.png",
        "szpontview.png": "image-viewer.png",
        "szpontmon.png": "utilities-system-monitor.png",
        "fastfetch.png": "dialog-information.png",
        "makaljer.png": "image-viewer.png",
        "nano.png": "text-editor.png",
        "donut.png": "applications-games.png",
        "szpontdetected.png": "preferences-system.png",
        "top.png": "utilities-system-monitor.png",
        "sh.png": "utilities-terminal.png",
    }

    print("[*] Generating high-definition 48x48 PNG icons...")
    for filename, raw_bytes in icons.items():
        dst = os.path.join(hicolor_dir, filename)
        write_png(dst, 48, 48, raw_bytes)
        print(f"  + {filename} (48x48 PNG)")

        # Copy to icons_root and pixmaps
        dst_pixmap = os.path.join(pixmaps_dir, filename)
        write_png(dst_pixmap, 48, 48, raw_bytes)
        dst_root = os.path.join(icons_root, filename)
        write_png(dst_root, 48, 48, raw_bytes)

    for alias, target in aliases.items():
        data = icons[target]
        write_png(os.path.join(hicolor_dir, alias), 48, 48, data)
        write_png(os.path.join(pixmaps_dir, alias), 48, 48, data)
        write_png(os.path.join(icons_root, alias), 48, 48, data)
        print(f"  + {alias} -> {target}")

    # Also copy to build/rootfs if it exists
    build_rootfs = os.path.join(base_dir, "build", "rootfs")
    if os.path.isdir(build_rootfs):
        b_hicolor = os.path.join(build_rootfs, "usr", "share", "icons", "hicolor", "48x48", "apps")
        b_pixmaps = os.path.join(build_rootfs, "usr", "share", "pixmaps")
        b_icons = os.path.join(build_rootfs, "usr", "share", "icons")
        for d in [b_hicolor, b_pixmaps, b_icons]:
            os.makedirs(d, exist_ok=True)
        for filename, raw_bytes in icons.items():
            write_png(os.path.join(b_hicolor, filename), 48, 48, raw_bytes)
            write_png(os.path.join(b_pixmaps, filename), 48, 48, raw_bytes)
            write_png(os.path.join(b_icons, filename), 48, 48, raw_bytes)
        for alias, target in aliases.items():
            data = icons[target]
            write_png(os.path.join(b_hicolor, alias), 48, 48, data)
            write_png(os.path.join(b_pixmaps, alias), 48, 48, data)
            write_png(os.path.join(b_icons, alias), 48, 48, data)

    print("[OK] All icons generated successfully.")

if __name__ == "__main__":
    main()
