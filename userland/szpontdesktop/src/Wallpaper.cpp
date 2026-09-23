#include "Wallpaper.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#include "stb_image.h"

namespace SzpontDesktop {

static std::unique_ptr<SzpontUI::BitmapSurface> s_wallpaper_surface;

#define SZPONT_WALLPAPER_TOP 0x141d33
#define SZPONT_WALLPAPER_BTM 0x3c5a86

static unsigned char *load_image_file(const char *path, int *w, int *h, int *channels, int req_comp) {
    if (!path || !path[0]) return nullptr;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return nullptr;

    off_t size = lseek(fd, 0, SEEK_END);
    if (size <= 0) {
        close(fd);
        return nullptr;
    }
    lseek(fd, 0, SEEK_SET);

    unsigned char *buf = (unsigned char *)malloc((size_t)size);
    if (!buf) {
        close(fd);
        return nullptr;
    }

    size_t total_read = 0;
    while (total_read < (size_t)size) {
        ssize_t n = read(fd, buf + total_read, (size_t)size - total_read);
        if (n <= 0) break;
        total_read += (size_t)n;
    }
    close(fd);

    if (total_read != (size_t)size) {
        free(buf);
        return nullptr;
    }

    unsigned char *img = stbi_load_from_memory(buf, (int)size, w, h, channels, req_comp);
    if (!img) {
        const char *reason = stbi_failure_reason();
        printf("[szpontdesktop] stbi_load_from_memory failed for '%s' (%zu bytes): %s\n",
               path, (size_t)size, reason ? reason : "unknown");
    }
    free(buf);
    return img;
}

static Pixmap create_gradient_wallpaper(Display *dpy, Window root, int screen, int h) {
    int depth = DefaultDepth(dpy, screen);
    int strip_w = 32;
    Pixmap pix = XCreatePixmap(dpy, root, (unsigned int)strip_w, (unsigned int)h, (unsigned int)depth);
    if (pix == None) return None;

    GC gc = XCreateGC(dpy, pix, 0, nullptr);
    uint32_t top = SZPONT_WALLPAPER_TOP;
    uint32_t btm = SZPONT_WALLPAPER_BTM;
    int r1 = (top >> 16) & 0xFF, g1 = (top >> 8) & 0xFF, b1 = top & 0xFF;
    int r2 = (btm >> 16) & 0xFF, g2 = (btm >> 8) & 0xFF, b2 = btm & 0xFF;

    int band_h = 4;
    for (int y = 0; y < h; y += band_h) {
        float t = (float)y / (float)(h > 1 ? (h - 1) : 1);
        int r = r1 + (int)((r2 - r1) * t);
        int g = g1 + (int)((g2 - g1) * t);
        int b = b1 + (int)((b2 - b1) * t);
        if (r < 0) r = 0;
        if (r > 255) r = 255;
        if (g < 0) g = 0;
        if (g > 255) g = 255;
        if (b < 0) b = 0;
        if (b > 255) b = 255;

        unsigned long pixel = (((unsigned long)r & 0xFF) << 16) |
                              (((unsigned long)g & 0xFF) << 8) |
                              (((unsigned long)b & 0xFF));
        XSetForeground(dpy, gc, pixel);
        XFillRectangle(dpy, pix, gc, 0, y, (unsigned int)strip_w, (unsigned int)band_h);
    }

    XFreeGC(dpy, gc);
    return pix;
}

Pixmap WallpaperManager::apply_wallpaper(Display *dpy, Window root, int screen, int screen_w, int screen_h,
                                        const std::string &image_path) {
    Pixmap wallpaper_pix = None;

    int img_w = 0, img_h = 0, channels = 0;
    unsigned char *raw = nullptr;

    if (!image_path.empty()) {
        raw = load_image_file(image_path.c_str(), &img_w, &img_h, &channels, 4);
        if (!raw) {
            // Check fallback in /usr/share/artwork/
            size_t slash = image_path.find_last_of('/');
            std::string basename = (slash != std::string::npos) ? image_path.substr(slash + 1) : image_path;
            std::string fallback = "/usr/share/artwork/" + basename;
            if (fallback != image_path) {
                raw = load_image_file(fallback.c_str(), &img_w, &img_h, &channels, 4);
                if (raw) {
                    printf("[szpontdesktop] Loaded fallback wallpaper '%s' (%dx%d)\n",
                           fallback.c_str(), img_w, img_h);
                }
            }
        } else {
            printf("[szpontdesktop] Loaded wallpaper '%s' (%dx%d, %d channels)\n",
                   image_path.c_str(), img_w, img_h, channels);
        }
    }

    if (raw && img_w > 0 && img_h > 0) {
        int depth = DefaultDepth(dpy, screen);
        Visual *visual = DefaultVisual(dpy, screen);

        wallpaper_pix = XCreatePixmap(dpy, root, (unsigned int)screen_w, (unsigned int)screen_h, (unsigned int)depth);
        if (wallpaper_pix != None) {
            GC gc = XCreateGC(dpy, wallpaper_pix, 0, nullptr);

            // Allocate 32-bit image buffer scaled to screen_w x screen_h (aspect-fill / crop)
            uint32_t *scaled_pixels = (uint32_t*)malloc(screen_w * screen_h * sizeof(uint32_t));
            if (scaled_pixels) {
                float scale_x = (float)img_w / (float)screen_w;
                float scale_y = (float)img_h / (float)screen_h;
                float scale = (scale_x < scale_y) ? scale_x : scale_y; // aspect cover

                int crop_src_w = (int)(screen_w * scale);
                int crop_src_h = (int)(screen_h * scale);
                int offset_x = (img_w - crop_src_w) / 2;
                int offset_y = (img_h - crop_src_h) / 2;
                if (offset_x < 0) offset_x = 0;
                if (offset_y < 0) offset_y = 0;

                const uint32_t *src32 = (const uint32_t*)raw;

                for (int dst_y = 0; dst_y < screen_h; ++dst_y) {
                    int src_y = offset_y + (int)(dst_y * scale);
                    if (src_y >= img_h) src_y = img_h - 1;
                    const uint32_t *src_row = src32 + src_y * img_w;
                    uint32_t *dst_row = scaled_pixels + dst_y * screen_w;

                    for (int dst_x = 0; dst_x < screen_w; ++dst_x) {
                        int src_x = offset_x + (int)(dst_x * scale);
                        if (src_x >= img_w) src_x = img_w - 1;
                        uint32_t rgba = src_row[src_x];
                        // Convert RGBA to 0x00RRGGBB / ARGB format
                        uint32_t r = rgba & 0xFF;
                        uint32_t g = (rgba >> 8) & 0xFF;
                        uint32_t b = (rgba >> 16) & 0xFF;
                        dst_row[dst_x] = (r << 16) | (g << 8) | b;
                    }
                }

                s_wallpaper_surface = std::make_unique<SzpontUI::BitmapSurface>(screen_w, screen_h);
                for (int y = 0; y < screen_h; ++y) {
                    uint32_t *dst_line = s_wallpaper_surface->scanline(y);
                    const uint32_t *src_line = scaled_pixels + y * screen_w;
                    for (int x = 0; x < screen_w; ++x) {
                        dst_line[x] = 0xFF000000 | (src_line[x] & 0x00FFFFFF);
                    }
                }

                XImage *ximg = XCreateImage(dpy, visual, depth, ZPixmap, 0,
                                            (char*)scaled_pixels, screen_w, screen_h, 32, screen_w * 4);
                if (ximg) {
                    XPutImage(dpy, wallpaper_pix, gc, ximg, 0, 0, 0, 0, screen_w, screen_h);
                    ximg->data = nullptr; // prevent double free by XDestroyImage
                    XDestroyImage(ximg);
                }
                free(scaled_pixels);
            }

            XFreeGC(dpy, gc);
        }
        stbi_image_free(raw);
    }

    if (wallpaper_pix == None) {
        printf("[szpontdesktop] Using native gradient wallpaper fallback\n");
        wallpaper_pix = create_gradient_wallpaper(dpy, root, screen, screen_h);
        s_wallpaper_surface = std::make_unique<SzpontUI::BitmapSurface>(screen_w, screen_h);
        for (int y = 0; y < screen_h; ++y) {
            float t = (float)y / (float)(screen_h > 1 ? (screen_h - 1) : 1);
            int r = 0x14 + (int)((0x3c - 0x14) * t);
            int g = 0x1d + (int)((0x5a - 0x1d) * t);
            int b = 0x33 + (int)((0x86 - 0x33) * t);
            uint32_t col = 0xFF000000 | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
            uint32_t *dst_line = s_wallpaper_surface->scanline(y);
            std::fill_n(dst_line, screen_w, col);
        }
    }

    if (wallpaper_pix != None) {
        XSetWindowBackgroundPixmap(dpy, root, wallpaper_pix);
        Atom prop_root = XInternAtom(dpy, "_XROOTPMAP_ID", False);
        Atom prop_eset = XInternAtom(dpy, "ESETROOT_PMAP_ID", False);
        XChangeProperty(dpy, root, prop_root, XA_PIXMAP, 32, PropModeReplace,
                        (unsigned char *)&wallpaper_pix, 1);
        XChangeProperty(dpy, root, prop_eset, XA_PIXMAP, 32, PropModeReplace,
                        (unsigned char *)&wallpaper_pix, 1);
        XClearWindow(dpy, root);
        XFlush(dpy);
    }

    return wallpaper_pix;
}

const SzpontUI::BitmapSurface *WallpaperManager::wallpaper_surface() {
    return s_wallpaper_surface.get();
}

} // namespace SzpontDesktop
