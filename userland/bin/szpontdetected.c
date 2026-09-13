/*
 * SzpontOS — Native X11 Artwork Viewer: Szpont Detected
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Standalone process displaying "Wykryto Szpont!" artwork in an independent X11 window.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#include "stb_image.h"

#define WIN_WIDTH   520
#define WIN_HEIGHT  560

typedef struct {
    int width;
    int height;
    int channels;
    uint32_t *pixels;
} loaded_image_t;

static unsigned long make_rgb(Display *dpy, int screen, unsigned short r, unsigned short g, unsigned short b) {
    Colormap cmap = DefaultColormap(dpy, screen);
    XColor col;
    col.red = r;
    col.green = g;
    col.blue = b;
    col.flags = DoRed | DoGreen | DoBlue;
    if (XAllocColor(dpy, cmap, &col)) {
        return col.pixel;
    }
    return WhitePixel(dpy, screen);
}

static loaded_image_t load_image_file(const char *p1, const char *p2, const char *p3, const char *p4) {
    loaded_image_t img = {0, 0, 0, NULL};
    const char *paths[] = {p1, p2, p3, p4};
    unsigned char *raw = NULL;
    int w = 0, h = 0, ch = 0;

    for (int i = 0; i < 4; i++) {
        if (!paths[i]) continue;
        raw = stbi_load(paths[i], &w, &h, &ch, 4);
        if (raw) {
            printf("[szpontdetected] Loaded artwork '%s' (%dx%d, %d channels)\n",
                   paths[i], w, h, ch);
            break;
        }
    }

    if (!raw) {
        printf("[szpontdetected] Warning: Artwork not found (%s)\n", p1);
        return img;
    }

    img.width = w;
    img.height = h;
    img.channels = 4;
    img.pixels = (uint32_t *)malloc((size_t)(w * h) * sizeof(uint32_t));
    if (img.pixels) {
        for (int i = 0; i < w * h; i++) {
            uint8_t r = raw[i * 4 + 0];
            uint8_t g = raw[i * 4 + 1];
            uint8_t b = raw[i * 4 + 2];
            uint8_t a = raw[i * 4 + 3];
            img.pixels[i] = (uint32_t)((a << 24) | (r << 16) | (g << 8) | b);
        }
    }
    stbi_image_free(raw);
    return img;
}

static void draw_scaled_image(Display *dpy, Window win, GC gc, const loaded_image_t *img,
                              int dst_x, int dst_y, int max_w, int max_h) {
    if (!img || !img->pixels || img->width <= 0 || img->height <= 0 || max_w <= 0 || max_h <= 0)
        return;

    float aspect = (float)img->width / (float)img->height;
    int render_w = max_w;
    int render_h = (int)(max_w / aspect);
    if (render_h > max_h) {
        render_h = max_h;
        render_w = (int)(max_h * aspect);
    }
    int off_x = dst_x + (max_w - render_w) / 2;
    int off_y = dst_y + (max_h - render_h) / 2;

    uint32_t *scaled = (uint32_t *)malloc((size_t)(render_w * render_h) * sizeof(uint32_t));
    if (!scaled) return;

    for (int y = 0; y < render_h; y++) {
        int src_y = (y * img->height) / render_h;
        if (src_y >= img->height) src_y = img->height - 1;
        for (int x = 0; x < render_w; x++) {
            int src_x = (x * img->width) / render_w;
            if (src_x >= img->width) src_x = img->width - 1;
            scaled[y * render_w + x] = img->pixels[src_y * img->width + src_x];
        }
    }

    XImage *ximg = XCreateImage(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
                                24, ZPixmap, 0, (char *)scaled,
                                (unsigned int)render_w, (unsigned int)render_h, 32, 0);
    if (ximg) {
        XPutImage(dpy, win, gc, ximg, 0, 0, off_x, off_y, (unsigned int)render_w, (unsigned int)render_h);
        ximg->data = NULL;
        XDestroyImage(ximg);
    }
    free(scaled);
}

int main(int argc, char *argv[]) {
    const char *disp_name = (argc > 1) ? argv[1] : getenv("DISPLAY");
    if (!disp_name || !*disp_name) disp_name = ":0";

    Display *dpy = XOpenDisplay(disp_name);
    if (!dpy) {
        fprintf(stderr, "[szpontdetected] Cannot open display '%s'\n", disp_name);
        return 1;
    }

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    unsigned long card_bg     = make_rgb(dpy, screen, 0x1e1e, 0x1e1e, 0x2e2e);
    unsigned long panel_color = make_rgb(dpy, screen, 0x3131, 0x3232, 0x4444);
    unsigned long fg_color    = make_rgb(dpy, screen, 0xcdcd, 0xd6d6, 0xf4f4);
    unsigned long green_col   = make_rgb(dpy, screen, 0xa6a6, 0xe3e3, 0xa1a1);
    unsigned long yellow_col  = make_rgb(dpy, screen, 0xf9f9, 0xe2e2, 0xafaf);

    loaded_image_t img = load_image_file("/usr/share/artwork/szpont-detected.png",
                                         "/usr/share/artwork/szpont-detected.jpg",
                                         "/usr/share/szpont-detected.png",
                                         "artwork/szpont-detected.jpg");

    int win_x = 580;
    int win_y = 70;
    Window win = XCreateSimpleWindow(dpy, root, win_x, win_y,
                                    WIN_WIDTH, WIN_HEIGHT, 0, green_col, card_bg);

    XStoreName(dpy, win, "Szpont Detected (Artwork)");

    /* Support WM_DELETE_WINDOW for clean closure via close button */
    Atom wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
    Atom wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete_window, 1);

    XSelectInput(dpy, win, ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(dpy, win);

    GC gc = XCreateGC(dpy, win, 0, NULL);

    int running = 1;
    while (running) {
        XEvent ev;
        XNextEvent(dpy, &ev);

        switch (ev.type) {
        case Expose:
            if (ev.xexpose.count == 0) {
                /* Clear and redraw card */
                XSetForeground(dpy, gc, card_bg);
                XFillRectangle(dpy, win, gc, 0, 0, WIN_WIDTH, WIN_HEIGHT);

                if (img.pixels) {
                    draw_scaled_image(dpy, win, gc, &img, 15, 15, 490, 480);
                } else {
                    XSetForeground(dpy, gc, yellow_col);
                    XDrawString(dpy, win, gc, 110, 270, "[ artwork/szpont-detected.png not found ]", 41);
                }

                /* Bottom Badge */
                XSetForeground(dpy, gc, panel_color);
                XFillRectangle(dpy, win, gc, 15, 510, 490, 36);
                XSetForeground(dpy, gc, fg_color);
                char sbuf[128];
                snprintf(sbuf, sizeof(sbuf), "Dimensions: %dx%d px | TrueColor 32-bit",
                         img.width, img.height);
                XDrawString(dpy, win, gc, 30, 532, sbuf, (int)strlen(sbuf));
                XFlush(dpy);
            }
            break;

        case KeyPress: {
            KeySym sym = XLookupKeysym(&ev.xkey, 0);
            if (sym == XK_q || sym == XK_Q || sym == XK_Escape) {
                running = 0;
            }
            break;
        }

        case ClientMessage:
            if (ev.xclient.message_type == wm_protocols &&
                (Atom)ev.xclient.data.l[0] == wm_delete_window) {
                running = 0;
            }
            break;

        case DestroyNotify:
            running = 0;
            break;
        }
    }

    if (img.pixels) free(img.pixels);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
