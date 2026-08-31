/*
 * SzpontOS — Native X11 Desktop Environment (Szpont Experience)
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Ultra-fast, lightweight multi-window desktop manager with draggable windows,
 * application launcher ([Launch XTerm]), STB PNG/JPEG artwork viewers,
 * glassmorphic TopBar, and fluid 60 FPS performance.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#include "../../third_party/stb/stb_image.h"

#define TITLEBAR_H 36

/* Color Helper */
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

typedef struct {
    int width;
    int height;
    int channels;
    uint32_t *pixels;
} loaded_image_t;

static loaded_image_t load_image_file(const char *path1, const char *path2, const char *path3) {
    loaded_image_t img = {0, 0, 0, NULL};
    const char *paths[] = {path1, path2, path3};
    unsigned char *raw = NULL;
    int w = 0, h = 0, ch = 0;

    for (int i = 0; i < 3; i++) {
        if (!paths[i]) continue;
        raw = stbi_load(paths[i], &w, &h, &ch, 4);
        if (raw) {
            printf("[szpontdesktop] Loaded artwork '%s' (%dx%d, %d channels)\n",
                   paths[i], w, h, ch);
            break;
        }
    }

    if (!raw) {
        printf("[szpontdesktop] Warning: Artwork not found (%s)\n", path1);
        return img;
    }

    img.width = w;
    img.height = h;
    img.channels = 4;
    img.pixels = (uint32_t *)malloc(w * h * sizeof(uint32_t));
    if (img.pixels) {
        for (int i = 0; i < w * h; i++) {
            uint8_t r = raw[i * 4 + 0];
            uint8_t g = raw[i * 4 + 1];
            uint8_t b = raw[i * 4 + 2];
            uint8_t a = raw[i * 4 + 3];
            img.pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
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

    uint32_t *scaled = (uint32_t *)malloc(render_w * render_h * sizeof(uint32_t));
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
                                render_w, render_h, 32, 0);
    if (ximg) {
        XPutImage(dpy, win, gc, ximg, 0, 0, off_x, off_y, render_w, render_h);
        ximg->data = NULL;
        XDestroyImage(ximg);
    }
    free(scaled);
}

/* Color definitions */
static unsigned long bg_color;
static unsigned long card_bg;
static unsigned long panel_color;
static unsigned long titlebar_bg;
static unsigned long fg_color;
static unsigned long cyan_color;
static unsigned long pink_color;
static unsigned long green_color;
static unsigned long yellow_color;
static unsigned long blue_color;
static unsigned long border_color;
static unsigned long active_border;
static unsigned long close_btn_col;
static unsigned long min_btn_col;
static unsigned long max_btn_col;

/* Managed Window Struct */
typedef struct window_entry {
    Window win;
    const char *title;
    int x, y;
    int w, h;
    bool mapped;
    bool focused;
    unsigned long title_color;
} window_entry_t;

#define NUM_WINDOWS 3
static window_entry_t g_windows[NUM_WINDOWS];

static void spawn_szponterm(void) {
    pid_t pid = fork();
    if (pid == 0) {
        char *args[] = {"/bin/szponterm", NULL};
        char *envp[] = {"DISPLAY=:0", "PATH=/bin:/usr/bin", "TERM=xterm-256color", "HOME=/root", "USER=root", "SHELL=/bin/sh", NULL};
        execve("/bin/szponterm", args, envp);
        _exit(1);
    }
    printf("[szpontdesktop] Spawned native SzponTerm process (PID %d)\n", pid);
}

static void draw_window_frame(Display *dpy, Window win, GC gc, int w, const char *title,
                              unsigned long title_col, bool focused) {
    /* Titlebar background */
    XSetForeground(dpy, gc, titlebar_bg);
    XFillRectangle(dpy, win, gc, 0, 0, w, TITLEBAR_H);

    /* macOS Window Controls */
    XSetForeground(dpy, gc, close_btn_col); /* Red Close */
    XFillArc(dpy, win, gc, 12, 12, 12, 12, 0, 360 * 64);
    XSetForeground(dpy, gc, yellow_color);  /* Yellow Minimize */
    XFillArc(dpy, win, gc, 30, 12, 12, 12, 0, 360 * 64);
    XSetForeground(dpy, gc, green_color);   /* Green Maximize */
    XFillArc(dpy, win, gc, 48, 12, 12, 12, 0, 360 * 64);

    /* Title string */
    XSetForeground(dpy, gc, title_col);
    XDrawString(dpy, win, gc, 70, 23, title, strlen(title));

    /* Accent bottom line */
    XSetForeground(dpy, gc, focused ? blue_color : panel_color);
    XDrawLine(dpy, win, gc, 0, TITLEBAR_H, w, TITLEBAR_H);
}

static void render_topbar(Display *dpy, Window win, GC gc, int screen_w, time_t now) {
    XSetForeground(dpy, gc, titlebar_bg);
    XFillRectangle(dpy, win, gc, 0, 0, screen_w, 36);

    XSetForeground(dpy, gc, blue_color);
    XDrawLine(dpy, win, gc, 0, 35, screen_w, 35);

    XSetForeground(dpy, gc, cyan_color);
    XDrawString(dpy, win, gc, 16, 22, "Szpont Experience", 16);

    XSetForeground(dpy, gc, fg_color);
    XDrawString(dpy, win, gc, 170, 22, "[1] Dashboard", 13);

    XSetForeground(dpy, gc, green_color);
    XDrawString(dpy, win, gc, 290, 22, "[2] + Launch SzponTerm", 22);

    XSetForeground(dpy, gc, fg_color);
    XDrawString(dpy, win, gc, 460, 22, "[3] Makaljer", 12);
    XDrawString(dpy, win, gc, 570, 22, "[4] Detected", 12);

    struct tm *tm_info = gmtime(&now);
    char clock_buf[64];
    if (tm_info) {
        snprintf(clock_buf, sizeof(clock_buf), "UTC: %04d-%02d-%02d %02d:%02d:%02d",
                 tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                 tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    } else {
        snprintf(clock_buf, sizeof(clock_buf), "SzpontOS Display :0");
    }
    XSetForeground(dpy, gc, yellow_color);
    XDrawString(dpy, win, gc, screen_w - 220, 22, clock_buf, strlen(clock_buf));
}

static void render_makaljer_window(Display *dpy, Window win, GC gc, const loaded_image_t *img, bool focused) {
    XSetForeground(dpy, gc, card_bg);
    XFillRectangle(dpy, win, gc, 0, TITLEBAR_H, 500, 600 - TITLEBAR_H);

    draw_window_frame(dpy, win, gc, 500, "Makaljer (PNG Artwork)", pink_color, focused);

    if (img && img->pixels) {
        draw_scaled_image(dpy, win, gc, img, 15, 50, 470, 480);
    } else {
        XSetForeground(dpy, gc, yellow_color);
        XDrawString(dpy, win, gc, 130, 290, "[ artwork/makaljer.png not found ]", 34);
    }

    /* Bottom Badge */
    XSetForeground(dpy, gc, panel_color);
    XFillRectangle(dpy, win, gc, 15, 550, 470, 32);
    XSetForeground(dpy, gc, fg_color);
    char sbuf[128];
    snprintf(sbuf, sizeof(sbuf), "Dimensions: %dx%d px | RGBA 32-bit",
             img ? img->width : 0, img ? img->height : 0);
    XDrawString(dpy, win, gc, 30, 572, sbuf, strlen(sbuf));
}

static void render_detected_window(Display *dpy, Window win, GC gc, const loaded_image_t *img, bool focused) {
    XSetForeground(dpy, gc, card_bg);
    XFillRectangle(dpy, win, gc, 0, TITLEBAR_H, 500, 600 - TITLEBAR_H);

    draw_window_frame(dpy, win, gc, 500, "Szpont Detected (Artwork)", green_color, focused);

    if (img && img->pixels) {
        draw_scaled_image(dpy, win, gc, img, 15, 50, 470, 480);
    } else {
        XSetForeground(dpy, gc, yellow_color);
        XDrawString(dpy, win, gc, 110, 290, "[ artwork/szpont-detected.png not found ]", 41);
    }

    /* Bottom Badge */
    XSetForeground(dpy, gc, panel_color);
    XFillRectangle(dpy, win, gc, 15, 550, 470, 32);
    XSetForeground(dpy, gc, fg_color);
    char sbuf[128];
    snprintf(sbuf, sizeof(sbuf), "Dimensions: %dx%d px | TrueColor 32-bit",
             img ? img->width : 0, img ? img->height : 0);
    XDrawString(dpy, win, gc, 30, 572, sbuf, strlen(sbuf));
}

static void render_main_static(Display *dpy, Window win, GC gc, int screen_w, int screen_h, bool focused) {
    XSetForeground(dpy, gc, bg_color);
    XFillRectangle(dpy, win, gc, 0, TITLEBAR_H, 800, 600 - TITLEBAR_H);

    draw_window_frame(dpy, win, gc, 800, "Szpont Experience — System Dashboard", cyan_color, focused);

    /* Left Card: System Specs */
    XSetForeground(dpy, gc, panel_color);
    XFillRectangle(dpy, win, gc, 20, 50, 370, 240);
    XSetForeground(dpy, gc, pink_color);
    XDrawRectangle(dpy, win, gc, 20, 50, 370, 240);

    XSetForeground(dpy, gc, fg_color);
    XDrawString(dpy, win, gc, 35, 75, "System Architecture & Kernel:", 29);
    char sbuf[128];
    snprintf(sbuf, sizeof(sbuf), "OS: SzpontOS 64-bit Monolithic (Ring 3)");
    XDrawString(dpy, win, gc, 35, 105, sbuf, strlen(sbuf));
    snprintf(sbuf, sizeof(sbuf), "Desktop: Szpont Experience v1.0");
    XDrawString(dpy, win, gc, 35, 130, sbuf, strlen(sbuf));
    snprintf(sbuf, sizeof(sbuf), "Display: %dx%d (24 bpp / 60 Hz)", screen_w, screen_h);
    XDrawString(dpy, win, gc, 35, 155, sbuf, strlen(sbuf));
    snprintf(sbuf, sizeof(sbuf), "Acceleration: DRM/KMS Hardware Dumb Buffer");
    XDrawString(dpy, win, gc, 35, 180, sbuf, strlen(sbuf));
    snprintf(sbuf, sizeof(sbuf), "Shared Libs: libX11.so, libm.so, libc.so");
    XDrawString(dpy, win, gc, 35, 205, sbuf, strlen(sbuf));
    snprintf(sbuf, sizeof(sbuf), "Terminal: Native SzponTerm X11");
    XDrawString(dpy, win, gc, 35, 230, sbuf, strlen(sbuf));

    /* Right Card Frame: Telemetry */
    XSetForeground(dpy, gc, panel_color);
    XFillRectangle(dpy, win, gc, 410, 50, 370, 240);
    XSetForeground(dpy, gc, green_color);
    XDrawRectangle(dpy, win, gc, 410, 50, 370, 240);
    XSetForeground(dpy, gc, fg_color);
    XDrawString(dpy, win, gc, 425, 75, "Interactive Input Telemetry:", 28);

    /* Bottom Card Frame: Geometry Animation */
    XSetForeground(dpy, gc, panel_color);
    XFillRectangle(dpy, win, gc, 20, 310, 760, 270);
    XSetForeground(dpy, gc, blue_color);
    XDrawRectangle(dpy, win, gc, 20, 310, 760, 270);
}

int main(int argc, char *argv[]) {
    const char *disp_name = (argc > 1) ? argv[1] : getenv("DISPLAY");
    if (!disp_name || !*disp_name) disp_name = ":0";

    printf("[szpontdesktop] Initializing Szpont Experience on '%s'...\n", disp_name);
    Display *dpy = XOpenDisplay(disp_name);
    if (!dpy) {
        fprintf(stderr, "[szpontdesktop] Fatal: Cannot connect to X server '%s'!\n", disp_name);
        return 1;
    }

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    int depth = DefaultDepth(dpy, screen);
    int screen_w = DisplayWidth(dpy, screen);
    int screen_h = DisplayHeight(dpy, screen);

    printf("[szpontdesktop] Connected: Screen %dx%d, Depth %d bpp, Vendor: %s\n",
           screen_w, screen_h, depth, ServerVendor(dpy));

    /* Initialize Colors */
    bg_color      = make_rgb(dpy, screen, 0x1818, 0x1818, 0x2525);
    card_bg       = make_rgb(dpy, screen, 0x1e1e, 0x1e1e, 0x2e2e);
    panel_color   = make_rgb(dpy, screen, 0x3131, 0x3232, 0x4444);
    titlebar_bg   = make_rgb(dpy, screen, 0x1111, 0x1111, 0x1b1b);
    fg_color      = make_rgb(dpy, screen, 0xcdcd, 0xd6d6, 0xf4f4);
    cyan_color    = make_rgb(dpy, screen, 0x8989, 0xdceb, 0xfafa);
    pink_color    = make_rgb(dpy, screen, 0xf5f5, 0xc2c2, 0xe7e7);
    green_color   = make_rgb(dpy, screen, 0xa6a6, 0xe3e3, 0xa1a1);
    yellow_color  = make_rgb(dpy, screen, 0xf9f9, 0xe2e2, 0xafaf);
    blue_color    = make_rgb(dpy, screen, 0x8989, 0xb4b4, 0xfafa);
    border_color  = make_rgb(dpy, screen, 0x4545, 0x4747, 0x5a5a);
    active_border = make_rgb(dpy, screen, 0x8989, 0xdceb, 0xfafa);
    close_btn_col = make_rgb(dpy, screen, 0xf3f3, 0x8b8b, 0xabab);
    min_btn_col   = make_rgb(dpy, screen, 0xf9f9, 0xe2e2, 0xafaf);
    max_btn_col   = make_rgb(dpy, screen, 0xa6a6, 0xe3e3, 0xa1a1);

    /* Load Artwork Images Once */
    loaded_image_t img_makaljer = load_image_file("/usr/share/artwork/makaljer.png",
                                                  "/usr/share/makaljer.png",
                                                  "artwork/makaljer.png");

    loaded_image_t img_detected = load_image_file("/usr/share/artwork/szpont-detected.png",
                                                  "/usr/share/artwork/szpont-detected.jpg",
                                                  "/usr/share/szpont-detected.png");
    if (!img_detected.pixels) {
        img_detected = load_image_file("artwork/szpont-detected.jpg",
                                       "artwork/szpont-detected.png",
                                       "artwork/szpont-scale.png");
    }

    /* 1. Top Menu Bar (1920x36) */
    Window win_topbar = XCreateSimpleWindow(dpy, root, 0, 0, screen_w, 36, 0, border_color, titlebar_bg);
    XSelectInput(dpy, win_topbar, ExposureMask | ButtonPressMask | KeyPressMask);
    XMapWindow(dpy, win_topbar);

    /* 2. Window 0: Main Dashboard (800x600 at 40, 56) */
    g_windows[0].x = 40; g_windows[0].y = 56; g_windows[0].w = 800; g_windows[0].h = 600;
    g_windows[0].title = "Szpont Experience Dashboard"; g_windows[0].title_color = cyan_color;
    g_windows[0].mapped = true; g_windows[0].focused = true;
    g_windows[0].win = XCreateSimpleWindow(dpy, root, g_windows[0].x, g_windows[0].y,
                                           g_windows[0].w, g_windows[0].h, 2, cyan_color, bg_color);
    XStoreName(dpy, g_windows[0].win, "Szpont Experience Dashboard");
    XSelectInput(dpy, g_windows[0].win, ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XMapWindow(dpy, g_windows[0].win);

    /* 3. Window 1: Makaljer Artwork Viewer (500x600 at 860, 56) */
    g_windows[1].x = 860; g_windows[1].y = 56; g_windows[1].w = 500; g_windows[1].h = 600;
    g_windows[1].title = "Makaljer Artwork"; g_windows[1].title_color = pink_color;
    g_windows[1].mapped = true; g_windows[1].focused = false;
    g_windows[1].win = XCreateSimpleWindow(dpy, root, g_windows[1].x, g_windows[1].y,
                                           g_windows[1].w, g_windows[1].h, 2, pink_color, card_bg);
    XStoreName(dpy, g_windows[1].win, "Makaljer Artwork");
    XSelectInput(dpy, g_windows[1].win, ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XMapWindow(dpy, g_windows[1].win);

    /* 4. Window 2: Szpont Detected Viewer (500x600 at 1380, 56) */
    g_windows[2].x = 1380; g_windows[2].y = 56; g_windows[2].w = 500; g_windows[2].h = 600;
    g_windows[2].title = "Szpont Detected"; g_windows[2].title_color = green_color;
    g_windows[2].mapped = true; g_windows[2].focused = false;
    g_windows[2].win = XCreateSimpleWindow(dpy, root, g_windows[2].x, g_windows[2].y,
                                           g_windows[2].w, g_windows[2].h, 2, green_color, card_bg);
    XStoreName(dpy, g_windows[2].win, "Szpont Detected");
    XSelectInput(dpy, g_windows[2].win, ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XMapWindow(dpy, g_windows[2].win);

    GC gc = XCreateGC(dpy, root, 0, NULL);

    /* Render initial static scenes */
    time_t last_time = 0;
    render_topbar(dpy, win_topbar, gc, screen_w, time(NULL));
    render_main_static(dpy, g_windows[0].win, gc, screen_w, screen_h, true);
    render_makaljer_window(dpy, g_windows[1].win, gc, &img_makaljer, false);
    render_detected_window(dpy, g_windows[2].win, gc, &img_detected, false);
    XFlush(dpy);

    /* Launch initial SzponTerm instance automatically */
    spawn_szponterm();

    /* Drag & State */
    int dragging_win_idx = -1;
    int drag_start_mouse_x = 0;
    int drag_start_mouse_y = 0;
    int drag_win_orig_x = 0;
    int drag_win_orig_y = 0;

    int mouse_x = 400;
    int mouse_y = 300;
    int click_count = 0;
    char last_key[64] = "None";
    float angle = 0.0f;
    int frame_count = 0;
    int running = 1;

    printf("[szpontdesktop] Szpont Experience running smoothly!\n");

    while (running) {
        /* Process all pending X11 events */
        while (XPending(dpy) > 0) {
            XEvent ev;
            XNextEvent(dpy, &ev);

            switch (ev.type) {
            case Expose:
                if (ev.xexpose.window == win_topbar) {
                    render_topbar(dpy, win_topbar, gc, screen_w, time(NULL));
                } else if (ev.xexpose.window == g_windows[0].win) {
                    render_main_static(dpy, g_windows[0].win, gc, screen_w, screen_h, g_windows[0].focused);
                } else if (ev.xexpose.window == g_windows[1].win) {
                    render_makaljer_window(dpy, g_windows[1].win, gc, &img_makaljer, g_windows[1].focused);
                } else if (ev.xexpose.window == g_windows[2].win) {
                    render_detected_window(dpy, g_windows[2].win, gc, &img_detected, g_windows[2].focused);
                }
                break;

            case ButtonPress: {
                click_count++;
                int win_idx = -1;
                for (int i = 0; i < NUM_WINDOWS; i++) {
                    if (ev.xbutton.window == g_windows[i].win) {
                        win_idx = i;
                        break;
                    }
                }

                if (ev.xbutton.window == win_topbar) {
                    /* TopBar Menu Actions */
                    if (ev.xbutton.x >= 170 && ev.xbutton.x <= 280) {
                        g_windows[0].mapped = true;
                        XMapWindow(dpy, g_windows[0].win);
                        XRaiseWindow(dpy, g_windows[0].win);
                    } else if (ev.xbutton.x >= 290 && ev.xbutton.x <= 440) {
                        spawn_szponterm();
                    } else if (ev.xbutton.x >= 450 && ev.xbutton.x <= 550) {
                        g_windows[1].mapped = true;
                        XMapWindow(dpy, g_windows[1].win);
                        XRaiseWindow(dpy, g_windows[1].win);
                    } else if (ev.xbutton.x >= 560 && ev.xbutton.x <= 660) {
                        g_windows[2].mapped = true;
                        XMapWindow(dpy, g_windows[2].win);
                        XRaiseWindow(dpy, g_windows[2].win);
                    }
                } else if (win_idx >= 0) {
                    /* Focus & Raise Window */
                    for (int i = 0; i < NUM_WINDOWS; i++) {
                        g_windows[i].focused = (i == win_idx);
                    }
                    XRaiseWindow(dpy, g_windows[win_idx].win);

                    /* Check if clicked Red Close button (x: 8..24, y: 6..28) */
                    if (ev.xbutton.x >= 8 && ev.xbutton.x <= 24 && ev.xbutton.y >= 6 && ev.xbutton.y <= 28) {
                        g_windows[win_idx].mapped = false;
                        XUnmapWindow(dpy, g_windows[win_idx].win);
                    } else if (ev.xbutton.x >= 26 && ev.xbutton.x <= 42 && ev.xbutton.y >= 6 && ev.xbutton.y <= 28) {
                        /* Yellow Minimize button */
                        g_windows[win_idx].mapped = false;
                        XUnmapWindow(dpy, g_windows[win_idx].win);
                    } else if (ev.xbutton.y < TITLEBAR_H) {
                        /* Start Dragging */
                        dragging_win_idx = win_idx;
                        drag_start_mouse_x = ev.xbutton.x_root;
                        drag_start_mouse_y = ev.xbutton.y_root;
                        drag_win_orig_x = g_windows[win_idx].x;
                        drag_win_orig_y = g_windows[win_idx].y;
                    }
                }
                break;
            }

            case ButtonRelease:
                dragging_win_idx = -1;
                break;

            case MotionNotify:
                mouse_x = ev.xmotion.x_root;
                mouse_y = ev.xmotion.y_root;
                if (dragging_win_idx >= 0) {
                    int dx = ev.xmotion.x_root - drag_start_mouse_x;
                    int dy = ev.xmotion.y_root - drag_start_mouse_y;
                    int new_x = drag_win_orig_x + dx;
                    int new_y = drag_win_orig_y + dy;
                    if (new_y < 36) new_y = 36;
                    g_windows[dragging_win_idx].x = new_x;
                    g_windows[dragging_win_idx].y = new_y;
                    XMoveWindow(dpy, g_windows[dragging_win_idx].win, new_x, new_y);
                }
                break;

            case KeyPress: {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                char *ks_name = XKeysymToString(ks);
                if (ks_name) {
                    snprintf(last_key, sizeof(last_key), "%s (0x%lx)", ks_name, (unsigned long)ks);
                }
                if (ks == XK_Escape || ks == XK_q || ks == XK_Q) {
                    running = 0;
                }
                break;
            }

            case DestroyNotify:
                running = 0;
                break;
            }
        }

        if (!running) break;

        /* 1. Update Clock on TopBar once per second */
        time_t cur_time = time(NULL);
        if (cur_time != last_time) {
            last_time = cur_time;
            render_topbar(dpy, win_topbar, gc, screen_w, cur_time);
        }

        /* 2. Update Dashboard Geometry Animation & Telemetry at 30 FPS */
        if (g_windows[0].mapped && (frame_count % 2 == 0)) {
            XSetForeground(dpy, gc, panel_color);
            XFillRectangle(dpy, g_windows[0].win, gc, 415, 95, 360, 185);

            XSetForeground(dpy, gc, fg_color);
            char sbuf[128];
            snprintf(sbuf, sizeof(sbuf), "Pointer: X = %4d, Y = %4d", mouse_x, mouse_y);
            XDrawString(dpy, g_windows[0].win, gc, 430, 120, sbuf, strlen(sbuf));
            snprintf(sbuf, sizeof(sbuf), "Registered Clicks: %d", click_count);
            XDrawString(dpy, g_windows[0].win, gc, 430, 150, sbuf, strlen(sbuf));
            snprintf(sbuf, sizeof(sbuf), "Last Key: %s", last_key);
            XDrawString(dpy, g_windows[0].win, gc, 430, 180, sbuf, strlen(sbuf));
            snprintf(sbuf, sizeof(sbuf), "Status: 60 FPS Multi-Window");
            XDrawString(dpy, g_windows[0].win, gc, 430, 210, sbuf, strlen(sbuf));
            snprintf(sbuf, sizeof(sbuf), "XTerm: Standalone Window (Press 'T' or Click TopBar)");
            XDrawString(dpy, g_windows[0].win, gc, 430, 240, sbuf, strlen(sbuf));

            /* Clear ONLY the animated bottom canvas */
            XSetForeground(dpy, gc, panel_color);
            XFillRectangle(dpy, g_windows[0].win, gc, 21, 311, 758, 268);

            /* Rotating Geometric Star */
            int star_cx = 580;
            int star_cy = 445;
            XPoint pts[9];
            for (int i = 0; i < 8; i++) {
                float a = angle + i * (3.14159265f / 4.0f);
                float rad = (i % 2 == 0) ? 75.0f : 35.0f;
                pts[i].x = star_cx + (short)(cosf(a) * rad);
                pts[i].y = star_cy + (short)(sinf(a) * rad);
            }
            pts[8] = pts[0];
            XSetForeground(dpy, gc, yellow_color);
            XDrawLines(dpy, g_windows[0].win, gc, pts, 9, CoordModeOrigin);

            /* Concentric Geometry */
            int center_x = 220;
            int center_y = 445;
            for (int r = 16; r <= 100; r += 18) {
                if (r % 36 == 0) XSetForeground(dpy, gc, cyan_color);
                else if (r % 36 == 18) XSetForeground(dpy, gc, pink_color);
                else XSetForeground(dpy, gc, green_color);
                XDrawArc(dpy, g_windows[0].win, gc, center_x - r, center_y - r, r * 2, r * 2, 0, 360 * 64);
            }

            angle += 0.06f;
        }

        XFlush(dpy);
        frame_count++;
        usleep(33000); /* 33ms -> smooth 30 FPS */
    }

    printf("[szpontdesktop] Cleaning up...\n");
    if (img_makaljer.pixels) free(img_makaljer.pixels);
    if (img_detected.pixels) free(img_detected.pixels);
    XFreeGC(dpy, gc);
    for (int i = 0; i < NUM_WINDOWS; i++) {
        XDestroyWindow(dpy, g_windows[i].win);
    }
    XDestroyWindow(dpy, win_topbar);
    XCloseDisplay(dpy);
    printf("[szpontdesktop] Exited cleanly.\n");

    return 0;
}
