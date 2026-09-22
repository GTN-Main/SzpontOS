/*
 * SzpontOS — Native X11 Desktop Environment & Window Manager (Szpont Experience)
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Lightweight, high-performance desktop panel and reparenting window manager.
 * Features:
 *   - Midnight vertical gradient wallpaper (tiled on X11 root)
 *   - Sleek 1px rounded window borders (XShape) with vibrant accent on focus
 *   - 32px titlebars with idle-dimmed traffic lights (#334155)
 *   - Dark-slate taskbar with Start launcher, quick app pills,
 *     dynamic open-window tabs with active indicators, and status tray.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

#define TOPBAR_HEIGHT 38
#define TITLEBAR_HEIGHT 32
#define BORDER_WIDTH 1
#define WINDOW_CORNER_RADIUS 8
#define MAX_MANAGED_WIN 128

/* ── SzpontOS Color Palette Tokens ───────────────────────────────────────── */
#define SZPONT_WALLPAPER_TOP 0x141d33 /* Deep Midnight Navy */
#define SZPONT_WALLPAPER_BTM 0x3c5a86 /* Atmospheric Slate Azure */

#define SZPONT_TASKBAR_BG 0x101726   /* Dark Taskbar Base */
#define SZPONT_TASKBAR_EDGE 0x28344e /* 1px Taskbar Boundary Line */
#define SZPONT_ITEM_BG 0x1b2436      /* Inactive pill / task item */
#define SZPONT_ITEM_HOVER 0x27334b   /* Hovered pill */
#define SZPONT_ITEM_ACTIVE 0x2c3d5e  /* Active focused window pill */
#define SZPONT_ACCENT 0x5b9cf8       /* Vibrant Szpont Accent Blue */
#define SZPONT_ACCENT_DIM 0x27436e   /* Dimmed accent */

#define SZPONT_WIN_EDGE_ACTIVE 0x5b9cf8 /* Active window 1px border */
#define SZPONT_WIN_EDGE_INACT 0x28344e  /* Inactive window 1px border */
#define SZPONT_TITLE_ACTIVE 0x172033    /* Active titlebar bg */
#define SZPONT_TITLE_INACT 0x0f1523     /* Inactive titlebar bg */
#define SZPONT_TITLE_DIVIDER 0x1e2a40   /* 1px separator line under titlebar */
#define SZPONT_TITLE_HIGHLIGHT 0x2c3d5e /* 1px top highlight line */

#define SZPONT_TRAFFIC_CLOSE 0xff5f57  /* Close Red */
#define SZPONT_TRAFFIC_MIN 0xfebc2e    /* Minimize Amber */
#define SZPONT_TRAFFIC_MAX 0x28c840    /* Maximize Emerald */
#define SZPONT_TRAFFIC_IDLE 0x334155   /* Unfocused Grey */
#define SZPONT_TRAFFIC_BORDER 0x1e293b /* Disc outline */

#define SZPONT_TEXT_WHITE 0xffffff  /* High-contrast white */
#define SZPONT_TEXT_MUTED 0x76839a  /* Muted slate */
#define SZPONT_TEXT_DIM 0x94a3b8    /* Subtle grey */
#define SZPONT_EXIT_BG 0x25181d     /* Exit pill background */
#define SZPONT_EXIT_BORDER 0x4a242c /* Exit pill border */
#define SZPONT_EXIT_RED 0xef4444    /* Exit soft red */

typedef struct {
    Window client;
    Window frame;
    int x, y;
    int width, height;
    bool is_maximized;
    bool is_shaded;
    int saved_x, saved_y;
    int saved_width, saved_height;
    char title[128];
    int close_attempts;
} client_window_t;

typedef struct {
    unsigned long wallpaper_top;
    unsigned long wallpaper_btm;
    unsigned long taskbar_bg;
    unsigned long taskbar_edge;
    unsigned long item_bg;
    unsigned long item_hover;
    unsigned long item_active;
    unsigned long accent;
    unsigned long accent_dim;
    unsigned long win_edge_active;
    unsigned long win_edge_inact;
    unsigned long title_active;
    unsigned long title_inact;
    unsigned long title_highlight;
    unsigned long title_divider;
    unsigned long traffic_close;
    unsigned long traffic_min;
    unsigned long traffic_max;
    unsigned long traffic_idle;
    unsigned long traffic_border;
    unsigned long text_white;
    unsigned long text_muted;
    unsigned long text_dim;
    unsigned long exit_bg;
    unsigned long exit_border;
    unsigned long exit_red;
} theme_colors_t;

static theme_colors_t g_theme;
static client_window_t g_clients[MAX_MANAGED_WIN];
static int g_client_count = 0;
static client_window_t *g_focused_client = NULL;
static client_window_t *g_drag_client = NULL;
static int g_drag_start_x = 0;
static int g_drag_start_y = 0;
static int g_drag_win_x = 0;
static int g_drag_win_y = 0;
static Window g_win_topbar = None;
static int g_screen_w = 1024;
static int g_screen_h = 768;
static Pixmap g_wallpaper_pix = None;
static Cursor g_default_cursor = None;
static bool g_has_shape = false;

static unsigned long make_rgb(Display *dpy, int screen, unsigned short r, unsigned short g, unsigned short b) {
    (void)dpy;
    (void)screen;
    return (((unsigned long)(r >> 8) & 0xFF) << 16) |
           (((unsigned long)(g >> 8) & 0xFF) << 8) |
           (((unsigned long)(b >> 8) & 0xFF));
}

static unsigned long make_hex_color(Display *dpy, int screen, uint32_t hex) {
    (void)dpy;
    (void)screen;
    return (unsigned long)(hex & 0x00FFFFFF);
}

static void init_theme(Display *dpy, int screen) {
    g_theme.wallpaper_top = make_hex_color(dpy, screen, SZPONT_WALLPAPER_TOP);
    g_theme.wallpaper_btm = make_hex_color(dpy, screen, SZPONT_WALLPAPER_BTM);
    g_theme.taskbar_bg = make_hex_color(dpy, screen, SZPONT_TASKBAR_BG);
    g_theme.taskbar_edge = make_hex_color(dpy, screen, SZPONT_TASKBAR_EDGE);
    g_theme.item_bg = make_hex_color(dpy, screen, SZPONT_ITEM_BG);
    g_theme.item_hover = make_hex_color(dpy, screen, SZPONT_ITEM_HOVER);
    g_theme.item_active = make_hex_color(dpy, screen, SZPONT_ITEM_ACTIVE);
    g_theme.accent = make_hex_color(dpy, screen, SZPONT_ACCENT);
    g_theme.accent_dim = make_hex_color(dpy, screen, SZPONT_ACCENT_DIM);
    g_theme.win_edge_active = make_hex_color(dpy, screen, SZPONT_WIN_EDGE_ACTIVE);
    g_theme.win_edge_inact = make_hex_color(dpy, screen, SZPONT_WIN_EDGE_INACT);
    g_theme.title_active = make_hex_color(dpy, screen, SZPONT_TITLE_ACTIVE);
    g_theme.title_inact = make_hex_color(dpy, screen, SZPONT_TITLE_INACT);
    g_theme.title_highlight = make_hex_color(dpy, screen, SZPONT_TITLE_HIGHLIGHT);
    g_theme.title_divider = make_hex_color(dpy, screen, SZPONT_TITLE_DIVIDER);
    g_theme.traffic_close = make_hex_color(dpy, screen, SZPONT_TRAFFIC_CLOSE);
    g_theme.traffic_min = make_hex_color(dpy, screen, SZPONT_TRAFFIC_MIN);
    g_theme.traffic_max = make_hex_color(dpy, screen, SZPONT_TRAFFIC_MAX);
    g_theme.traffic_idle = make_hex_color(dpy, screen, SZPONT_TRAFFIC_IDLE);
    g_theme.traffic_border = make_hex_color(dpy, screen, SZPONT_TRAFFIC_BORDER);
    g_theme.text_white = make_hex_color(dpy, screen, SZPONT_TEXT_WHITE);
    g_theme.text_muted = make_hex_color(dpy, screen, SZPONT_TEXT_MUTED);
    g_theme.text_dim = make_hex_color(dpy, screen, SZPONT_TEXT_DIM);
    g_theme.exit_bg = make_hex_color(dpy, screen, SZPONT_EXIT_BG);
    g_theme.exit_border = make_hex_color(dpy, screen, SZPONT_EXIT_BORDER);
    g_theme.exit_red = make_hex_color(dpy, screen, SZPONT_EXIT_RED);
}

static Pixmap create_gradient_wallpaper(Display *dpy, Window root, int screen, int h) {
    int depth = DefaultDepth(dpy, screen);
    int strip_w = 32;
    Pixmap pix = XCreatePixmap(dpy, root, (unsigned int)strip_w, (unsigned int)h, (unsigned int)depth);
    if (pix == None)
        return None;

    GC gc = XCreateGC(dpy, pix, 0, NULL);
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
        if (r < 0)
            r = 0;
        if (r > 255)
            r = 255;
        if (g < 0)
            g = 0;
        if (g > 255)
            g = 255;
        if (b < 0)
            b = 0;
        if (b > 255)
            b = 255;

        unsigned long pixel = make_rgb(dpy, screen, (unsigned short)(r * 0x101), (unsigned short)(g * 0x101),
                                       (unsigned short)(b * 0x101));
        XSetForeground(dpy, gc, pixel);
        XFillRectangle(dpy, pix, gc, 0, y, (unsigned int)strip_w, (unsigned int)band_h);
    }

    XFreeGC(dpy, gc);
    return pix;
}

static void apply_window_shape(Display *dpy, client_window_t *cw) {
    if (!g_has_shape || !cw || cw->frame == None)
        return;

    if (cw->is_maximized) {
        /* Remove shape constraints when maximized so window fills screen rectangularly */
        XShapeCombineMask(dpy, cw->frame, ShapeBounding, 0, 0, None, ShapeSet);
        XShapeCombineMask(dpy, cw->frame, ShapeClip, 0, 0, None, ShapeSet);
        return;
    }

    int w = cw->width;
    int h = cw->is_shaded ? TITLEBAR_HEIGHT : (cw->height + TITLEBAR_HEIGHT);
    int r_out = WINDOW_CORNER_RADIUS;
    int r_in = (r_out > 1) ? (r_out - 1) : 1;

    if (w <= 2 * r_out || h <= 2 * r_out)
        return;

    int bw = BORDER_WIDTH;
    int total_w = w + 2 * bw;
    int total_h = h + 2 * bw;

    /* 1. ShapeBounding mask: curves the window frame and outer 1px border */
    Pixmap mask_b = XCreatePixmap(dpy, cw->frame, (unsigned int)total_w, (unsigned int)total_h, 1);
    if (mask_b != None) {
        GC mgc = XCreateGC(dpy, mask_b, 0, NULL);
        XSetForeground(dpy, mgc, 0);
        XFillRectangle(dpy, mask_b, mgc, 0, 0, (unsigned int)total_w, (unsigned int)total_h);

        XSetForeground(dpy, mgc, 1);
        int d_out = r_out * 2;
        /* Center cross slabs */
        XFillRectangle(dpy, mask_b, mgc, 0, r_out, (unsigned int)total_w, (unsigned int)(total_h - d_out));
        XFillRectangle(dpy, mask_b, mgc, r_out, 0, (unsigned int)(total_w - d_out), (unsigned int)total_h);

        /* 4 corner arcs */
        XFillArc(dpy, mask_b, mgc, 0, 0, (unsigned int)d_out, (unsigned int)d_out, 90 * 64, 90 * 64);
        XFillArc(dpy, mask_b, mgc, total_w - d_out, 0, (unsigned int)d_out, (unsigned int)d_out, 0 * 64, 90 * 64);
        XFillArc(dpy, mask_b, mgc, 0, total_h - d_out, (unsigned int)d_out, (unsigned int)d_out, 180 * 64, 90 * 64);
        XFillArc(dpy, mask_b, mgc, total_w - d_out, total_h - d_out, (unsigned int)d_out, (unsigned int)d_out, 270 * 64,
                 90 * 64);

        XShapeCombineMask(dpy, cw->frame, ShapeBounding, -bw, -bw, mask_b, ShapeSet);
        XFreeGC(dpy, mgc);
        XFreePixmap(dpy, mask_b);
    }

    /* 2. ShapeClip mask: curves inner content area and clips subwindow corners */
    Pixmap mask_c = XCreatePixmap(dpy, cw->frame, (unsigned int)w, (unsigned int)h, 1);
    if (mask_c != None) {
        GC mgc = XCreateGC(dpy, mask_c, 0, NULL);
        XSetForeground(dpy, mgc, 0);
        XFillRectangle(dpy, mask_c, mgc, 0, 0, (unsigned int)w, (unsigned int)h);

        XSetForeground(dpy, mgc, 1);
        int d_in = r_in * 2;
        /* Center cross slabs */
        XFillRectangle(dpy, mask_c, mgc, 0, r_in, (unsigned int)w, (unsigned int)(h - d_in));
        XFillRectangle(dpy, mask_c, mgc, r_in, 0, (unsigned int)(w - d_in), (unsigned int)h);

        /* 4 corner arcs */
        XFillArc(dpy, mask_c, mgc, 0, 0, (unsigned int)d_in, (unsigned int)d_in, 90 * 64, 90 * 64);
        XFillArc(dpy, mask_c, mgc, w - d_in, 0, (unsigned int)d_in, (unsigned int)d_in, 0 * 64, 90 * 64);
        XFillArc(dpy, mask_c, mgc, 0, h - d_in, (unsigned int)d_in, (unsigned int)d_in, 180 * 64, 90 * 64);
        XFillArc(dpy, mask_c, mgc, w - d_in, h - d_in, (unsigned int)d_in, (unsigned int)d_in, 270 * 64, 90 * 64);

        XShapeCombineMask(dpy, cw->frame, ShapeClip, 0, 0, mask_c, ShapeSet);
        XFreeGC(dpy, mgc);
        XFreePixmap(dpy, mask_c);
    }
}

static pid_t spawn_app_args(const char *binary, char *const argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        extern char **environ;
        execve(binary, argv, environ);
        fprintf(stderr, "[szpontdesktop] Failed to exec '%s'\n", binary);
        _exit(1);
    }
    if (pid > 0) {
        setpgid(pid, pid);
        printf("[szpontdesktop] Spawned process '%s' (PID %d)\n", binary, pid);
        fflush(stdout);
    }
    return pid;
}

static pid_t spawn_app(const char *binary) {
    char *args[] = {(char *)binary, NULL};
    return spawn_app_args(binary, args);
}

static pid_t spawn_named_app(const char *name) {
    char path[128];
    snprintf(path, sizeof(path), "/usr/bin/%s", name);
    if (access(path, X_OK) == 0) {
        return spawn_app(path);
    }
    snprintf(path, sizeof(path), "/bin/%s", name);
    return spawn_app(path);
}

static pid_t spawn_terminal(void) {
    if (access("/usr/bin/szponterm", X_OK) == 0) {
        return spawn_app("/usr/bin/szponterm");
    }
    if (access("/bin/szponterm", X_OK) == 0) {
        return spawn_app("/bin/szponterm");
    }
    const char *xterm_bin = (access("/usr/bin/xterm", X_OK) == 0) ? "/usr/bin/xterm" : "/bin/xterm";
    char *args[] = {
        (char *)xterm_bin,
        (char *)"-bg", (char *)"#000000",
        (char *)"-fg", (char *)"#f8fafc",
        (char *)"-geometry", (char *)"80x24",
        (char *)"+wf",
        (char *)"-e", (char *)"/bin/sh",
        NULL
    };
    return spawn_app_args(xterm_bin, args);
}

static void draw_pill_button(Display *dpy, Window win, GC gc, int x, int y, int w, int h, unsigned long bg_col,
                             unsigned long border_col) {
    XSetForeground(dpy, gc, bg_col);
    XFillRectangle(dpy, win, gc, x, y, (unsigned int)w, (unsigned int)h);
    XSetForeground(dpy, gc, border_col);
    XDrawRectangle(dpy, win, gc, x, y, (unsigned int)(w - 1), (unsigned int)(h - 1));
}

static void render_topbar(Display *dpy, Window win, GC gc, int screen_w, time_t now) {
    /* 1. TopBar background & bottom 1px edge */
    XSetForeground(dpy, gc, g_theme.taskbar_bg);
    XFillRectangle(dpy, win, gc, 0, 0, (unsigned int)screen_w, TOPBAR_HEIGHT);

    XSetForeground(dpy, gc, g_theme.taskbar_edge);
    XDrawLine(dpy, win, gc, 0, TOPBAR_HEIGHT - 1, screen_w, TOPBAR_HEIGHT - 1);

    int pill_y = 6;
    int pill_h = 26;
    int text_y = 23;

    /* 2. Start Menu Pill: x=8, w=92 */
    draw_pill_button(dpy, win, gc, 8, pill_y, 92, pill_h, g_theme.item_bg, g_theme.taskbar_edge);
    XSetForeground(dpy, gc, g_theme.accent);
    XDrawString(dpy, win, gc, 16, text_y, "[S]", 3);
    XSetForeground(dpy, gc, g_theme.text_white);
    XDrawString(dpy, win, gc, 38, text_y, "SzpontOS", 8);

    /* 3. Quick Launchers */
    /* [>_] Term */
    draw_pill_button(dpy, win, gc, 106, pill_y, 70, pill_h, g_theme.item_bg, g_theme.taskbar_edge);
    XSetForeground(dpy, gc, g_theme.accent);
    XDrawString(dpy, win, gc, 114, text_y, ">_", 2);
    XSetForeground(dpy, gc, g_theme.text_dim);
    XDrawString(dpy, win, gc, 134, text_y, "Term", 4);

    /* Makaljer */
    draw_pill_button(dpy, win, gc, 182, pill_y, 74, pill_h, g_theme.item_bg, g_theme.taskbar_edge);
    XSetForeground(dpy, gc, g_theme.text_dim);
    XDrawString(dpy, win, gc, 192, text_y, "Makaljer", 8);

    /* Detected */
    draw_pill_button(dpy, win, gc, 262, pill_y, 74, pill_h, g_theme.item_bg, g_theme.taskbar_edge);
    XSetForeground(dpy, gc, g_theme.text_dim);
    XDrawString(dpy, win, gc, 272, text_y, "Detected", 8);

    /* 4. System Tray & Status Area (Right Side) */
    /* Logout button pill */
    int exit_x = screen_w - 48;
    draw_pill_button(dpy, win, gc, exit_x, pill_y, 40, pill_h, g_theme.exit_bg, g_theme.exit_border);
    XSetForeground(dpy, gc, g_theme.exit_red);
    XDrawString(dpy, win, gc, exit_x + 8, text_y, "Exit", 4);

    /* Clock pill */
    int clock_w = 146;
    int clock_x = exit_x - 8 - clock_w;
    draw_pill_button(dpy, win, gc, clock_x, pill_y, clock_w, pill_h, g_theme.title_active, g_theme.taskbar_edge);
    struct tm *tm_info = gmtime(&now);
    char clock_buf[32];
    if (tm_info) {
        snprintf(clock_buf, sizeof(clock_buf), "%02d:%02d:%02d UTC", tm_info->tm_hour, tm_info->tm_min,
                 tm_info->tm_sec);
    } else {
        snprintf(clock_buf, sizeof(clock_buf), ":0 Display");
    }
    XSetForeground(dpy, gc, g_theme.text_white);
    XDrawString(dpy, win, gc, clock_x + 12, text_y, clock_buf, (int)strlen(clock_buf));

    /* User badge pill */
    const char *curr_user = getenv("USER");
    if (!curr_user || !*curr_user)
        curr_user = "szpont";
    char user_buf[32];
    snprintf(user_buf, sizeof(user_buf), "u: %s", curr_user);
    int user_w = 78;
    int user_x = clock_x - 8 - user_w;
    draw_pill_button(dpy, win, gc, user_x, pill_y, user_w, pill_h, g_theme.item_bg, g_theme.taskbar_edge);
    XSetForeground(dpy, gc, g_theme.accent);
    XDrawString(dpy, win, gc, user_x + 10, text_y, user_buf, (int)strlen(user_buf));

    /* 5. Dynamic Window Taskbar Tabs */
    int task_start_x = 344;
    int task_end_x = user_x - 8;
    int avail_w = task_end_x - task_start_x;

    if (g_client_count > 0 && avail_w > 80) {
        int max_item_w = 140;
        int gap = 6;
        int item_w = max_item_w;
        if (g_client_count * (max_item_w + gap) > avail_w) {
            item_w = (avail_w / g_client_count) - gap;
            if (item_w < 64)
                item_w = 64;
        }

        for (int i = 0; i < g_client_count; i++) {
            client_window_t *cw = &g_clients[i];
            int tx = task_start_x + i * (item_w + gap);
            if (tx + item_w > task_end_x)
                break;

            bool is_act = (cw == g_focused_client);
            unsigned long bg = is_act ? g_theme.item_active : g_theme.item_bg;
            unsigned long border = is_act ? g_theme.accent : g_theme.taskbar_edge;
            draw_pill_button(dpy, win, gc, tx, pill_y, item_w, pill_h, bg, border);

            if (is_act) {
                /* 2px bottom accent strip in Szpont Blue */
                XSetForeground(dpy, gc, g_theme.accent);
                XFillRectangle(dpy, win, gc, tx + 2, pill_y + pill_h - 2, (unsigned int)(item_w - 4), 2);
            }

            /* Window Title formatting */
            char disp_title[64];
            if (cw->is_shaded) {
                snprintf(disp_title, sizeof(disp_title), "[-] %s", cw->title);
            } else {
                snprintf(disp_title, sizeof(disp_title), "%s", cw->title);
            }

            int max_chars = (item_w - 16) / 7;
            if (max_chars < 3)
                max_chars = 3;
            if ((int)strlen(disp_title) > max_chars) {
                disp_title[max_chars - 2] = '.';
                disp_title[max_chars - 1] = '.';
                disp_title[max_chars] = '\0';
            }

            XSetForeground(dpy, gc,
                           is_act ? g_theme.text_white : (cw->is_shaded ? g_theme.text_muted : g_theme.text_dim));
            XDrawString(dpy, win, gc, tx + 8, text_y, disp_title, (int)strlen(disp_title));
        }
    }
}

/* Standard 16x16 crisp arrow cursor */
static const unsigned char cursor_bits[] = {0x01, 0x00, 0x03, 0x00, 0x07, 0x00, 0x0f, 0x00, 0x1f, 0x00, 0x3f,
                                            0x00, 0x7f, 0x00, 0xff, 0x00, 0x7f, 0x00, 0x1f, 0x00, 0x3b, 0x00,
                                            0x71, 0x00, 0xe0, 0x00, 0xc0, 0x01, 0x80, 0x01, 0x00, 0x00};
static const unsigned char cursor_mask[] = {0x03, 0x00, 0x07, 0x00, 0x0f, 0x00, 0x1f, 0x00, 0x3f, 0x00, 0x7f,
                                            0x00, 0xff, 0x00, 0xff, 0x01, 0xff, 0x01, 0xff, 0x00, 0x7f, 0x00,
                                            0xfb, 0x00, 0xf1, 0x01, 0xe0, 0x03, 0xc0, 0x03, 0x80, 0x01};

static Cursor create_default_cursor(Display *dpy, Window root) {
    Pixmap src = XCreateBitmapFromData(dpy, root, (const char *)cursor_bits, 16, 16);
    Pixmap msk = XCreateBitmapFromData(dpy, root, (const char *)cursor_mask, 16, 16);
    XColor fg, bg;
    fg.red = 0xffff;
    fg.green = 0xffff;
    fg.blue = 0xffff;
    fg.flags = DoRed | DoGreen | DoBlue;
    bg.red = 0x0000;
    bg.green = 0x0000;
    bg.blue = 0x0000;
    bg.flags = DoRed | DoGreen | DoBlue;
    Cursor c = XCreatePixmapCursor(dpy, src, msk, &fg, &bg, 0, 0);
    XFreePixmap(dpy, src);
    XFreePixmap(dpy, msk);
    return c;
}

static client_window_t *find_client_by_frame(Window frame) {
    for (int i = 0; i < g_client_count; i++) {
        if (g_clients[i].frame == frame)
            return &g_clients[i];
    }
    return NULL;
}

static client_window_t *find_client_by_window(Window client) {
    for (int i = 0; i < g_client_count; i++) {
        if (g_clients[i].client == client)
            return &g_clients[i];
    }
    return NULL;
}

static void remove_client(Window client, Display *dpy, GC gc) {
    for (int i = 0; i < g_client_count; i++) {
        if (g_clients[i].client == client || g_clients[i].frame == client) {
            for (int j = i; j < g_client_count - 1; j++) {
                g_clients[j] = g_clients[j + 1];
            }
            g_client_count--;
            break;
        }
    }
    if (g_win_topbar != None && dpy != NULL && gc != None) {
        render_topbar(dpy, g_win_topbar, gc, g_screen_w, time(NULL));
    }
}

static void render_titlebar(Display *dpy, client_window_t *cw, GC gc, bool is_focused) {
    if (!cw || cw->frame == None)
        return;

    /* 1. Titlebar background */
    XSetForeground(dpy, gc, is_focused ? g_theme.title_active : g_theme.title_inact);
    XFillRectangle(dpy, cw->frame, gc, 0, 0, (unsigned int)cw->width, TITLEBAR_HEIGHT);

    /* 2. Top accent highlight line (runs between rounded corners) */
    XSetForeground(dpy, gc, is_focused ? g_theme.title_highlight : g_theme.win_edge_inact);
    int top_line_start = g_has_shape ? WINDOW_CORNER_RADIUS : 0;
    int top_line_end = g_has_shape ? (cw->width - WINDOW_CORNER_RADIUS) : cw->width;
    if (top_line_end > top_line_start) {
        XDrawLine(dpy, cw->frame, gc, top_line_start, 0, top_line_end, 0);
    }

    /* 3. Bottom divider line */
    XSetForeground(dpy, gc, is_focused ? g_theme.title_divider : g_theme.win_edge_inact);
    XDrawLine(dpy, cw->frame, gc, 0, TITLEBAR_HEIGHT - 1, cw->width, TITLEBAR_HEIGHT - 1);

    /* 4. Traffic light control dots (12px diameter) */
    int dot_y = (TITLEBAR_HEIGHT - 12) / 2; /* 10 */
    int dot_x1 = 14;
    int dot_x2 = 32;
    int dot_x3 = 50;

    if (is_focused) {
        /* Close (Red) */
        XSetForeground(dpy, gc, g_theme.traffic_close);
        XFillArc(dpy, cw->frame, gc, dot_x1, dot_y, 12, 12, 0, 360 * 64);
        XSetForeground(dpy, gc, g_theme.traffic_border);
        XDrawArc(dpy, cw->frame, gc, dot_x1, dot_y, 12, 12, 0, 360 * 64);

        /* Minimize (Yellow) */
        XSetForeground(dpy, gc, g_theme.traffic_min);
        XFillArc(dpy, cw->frame, gc, dot_x2, dot_y, 12, 12, 0, 360 * 64);
        XSetForeground(dpy, gc, g_theme.traffic_border);
        XDrawArc(dpy, cw->frame, gc, dot_x2, dot_y, 12, 12, 0, 360 * 64);

        /* Maximize (Green) */
        XSetForeground(dpy, gc, g_theme.traffic_max);
        XFillArc(dpy, cw->frame, gc, dot_x3, dot_y, 12, 12, 0, 360 * 64);
        XSetForeground(dpy, gc, g_theme.traffic_border);
        XDrawArc(dpy, cw->frame, gc, dot_x3, dot_y, 12, 12, 0, 360 * 64);
    } else {
        /* Unfocused / Inactive: Dimmed to neutral slate grey (#334155) */
        XSetForeground(dpy, gc, g_theme.traffic_idle);
        XFillArc(dpy, cw->frame, gc, dot_x1, dot_y, 12, 12, 0, 360 * 64);
        XFillArc(dpy, cw->frame, gc, dot_x2, dot_y, 12, 12, 0, 360 * 64);
        XFillArc(dpy, cw->frame, gc, dot_x3, dot_y, 12, 12, 0, 360 * 64);

        XSetForeground(dpy, gc, g_theme.traffic_border);
        XDrawArc(dpy, cw->frame, gc, dot_x1, dot_y, 12, 12, 0, 360 * 64);
        XDrawArc(dpy, cw->frame, gc, dot_x2, dot_y, 12, 12, 0, 360 * 64);
        XDrawArc(dpy, cw->frame, gc, dot_x3, dot_y, 12, 12, 0, 360 * 64);
    }

    /* 5. Window Title Text (left-aligned at x=72) */
    XSetForeground(dpy, gc, is_focused ? g_theme.text_white : g_theme.text_muted);
    int text_y = dot_y + 10;
    XDrawString(dpy, cw->frame, gc, 72, text_y, cw->title, (int)strlen(cw->title));

    /* 6. Border width & color on the frame */
    XSetWindowBorderWidth(dpy, cw->frame, BORDER_WIDTH);
    XSetWindowBorder(dpy, cw->frame, is_focused ? g_theme.win_edge_active : g_theme.win_edge_inact);
}

static void set_focus(Display *dpy, client_window_t *cw, GC gc) {
    if (!cw)
        return;
    printf("[szpontdesktop] set_focus: start for '%s'\n", cw->title); fflush(stdout);
    if (g_focused_client && g_focused_client != cw) {
        /* Grab any button on previously focused client so clicking anywhere refocuses it */
        XGrabButton(dpy, AnyButton, AnyModifier, g_focused_client->client, False, ButtonPressMask, GrabModeAsync,
                    GrabModeAsync, None, None);
        render_titlebar(dpy, g_focused_client, gc, false);
    }
    g_focused_client = cw;
    /* Ungrab buttons on active client so application handles clicks directly */
    XUngrabButton(dpy, AnyButton, AnyModifier, cw->client);
    XRaiseWindow(dpy, cw->frame);
    render_titlebar(dpy, cw, gc, true);
    printf("[szpontdesktop] set_focus: calling XSetInputFocus\n"); fflush(stdout);
    XSetInputFocus(dpy, cw->client, RevertToParent, CurrentTime);

    /* Update dynamic taskbar */
    if (g_win_topbar != None) {
        render_topbar(dpy, g_win_topbar, gc, g_screen_w, time(NULL));
    }
    XFlush(dpy);
    printf("[szpontdesktop] set_focus: finished for '%s'\n", cw->title); fflush(stdout);
}

static void close_client(Display *dpy, client_window_t *cw) {
    if (!cw || !cw->client)
        return;

    printf("[szpontdesktop] Close requested for window '%s' (client 0x%lx, frame 0x%lx, attempt %d)\n",
           cw->title, (unsigned long)cw->client, (unsigned long)cw->frame, cw->close_attempts + 1);
    fflush(stdout);

    cw->close_attempts++;
    if (cw->close_attempts > 1) {
        printf("[szpontdesktop] Force killing unresponsive window '%s' (0x%lx)\n",
               cw->title, (unsigned long)cw->client);
        XKillClient(dpy, cw->client);
        XFlush(dpy);
        return;
    }

    Atom wm_del = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    Atom wm_proto = XInternAtom(dpy, "WM_PROTOCOLS", False);

    Atom *protocols = NULL;
    int count = 0;
    bool has_wm_delete = false;
    if (XGetWMProtocols(dpy, cw->client, &protocols, &count) && protocols) {
        for (int i = 0; i < count; i++) {
            if (protocols[i] == wm_del) {
                has_wm_delete = true;
                break;
            }
        }
        XFree(protocols);
    }

    if (has_wm_delete) {
        XEvent msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = ClientMessage;
        msg.xclient.window = cw->client;
        msg.xclient.message_type = wm_proto;
        msg.xclient.format = 32;
        msg.xclient.data.l[0] = (long)wm_del;
        msg.xclient.data.l[1] = CurrentTime;
        XSendEvent(dpy, cw->client, False, NoEventMask, &msg);
    } else {
        XKillClient(dpy, cw->client);
    }
    XFlush(dpy);
}

static client_window_t *decorate_window(Display *dpy, Window root, Window client, GC gc) {
    printf("[szpontdesktop] decorate_window: client=0x%lx\n", (unsigned long)client); fflush(stdout);
    if (client == root || client == g_win_topbar)
        return NULL;
    client_window_t *existing = find_client_by_window(client);
    if (existing)
        return existing;
    if (find_client_by_frame(client))
        return NULL;

    Window root_ret = None;
    int cx = 20, cy = TOPBAR_HEIGHT + 10;
    unsigned int cw = 640, ch = 480, border_w = 0, depth = 0;
    printf("[szpontdesktop] decorate_window: calling XGetGeometry for client 0x%lx\n", (unsigned long)client); fflush(stdout);
    if (!XGetGeometry(dpy, client, &root_ret, &cx, &cy, &cw, &ch, &border_w, &depth)) {
        printf("[szpontdesktop] decorate_window: XGetGeometry failed on 0x%lx (window invalid or destroyed)\n", (unsigned long)client); fflush(stdout);
        return NULL;
    }
    printf("[szpontdesktop] decorate_window: XGetGeometry succeeded: %ux%u at (%d,%d)\n", cw, ch, cx, cy); fflush(stdout);
    if (cy < TOPBAR_HEIGHT)
        cy = TOPBAR_HEIGHT + 10;
    if (cx < 10)
        cx = 10;

    char title[128] = "SzpontOS Application";
    char *name = NULL;
    printf("[szpontdesktop] decorate_window: calling XFetchName\n"); fflush(stdout);
    if (XFetchName(dpy, client, &name) && name) {
        strncpy(title, name, sizeof(title) - 1);
        XFree(name);
    }
    printf("[szpontdesktop] decorate_window: title='%s', geom=%ux%u at (%d,%d)\n",
           title, cw, ch, cx, cy); fflush(stdout);

    printf("[szpontdesktop] decorate_window: creating frame window\n"); fflush(stdout);
    Window frame = XCreateSimpleWindow(dpy, root, cx, cy, (unsigned int)cw, (unsigned int)(ch + TITLEBAR_HEIGHT),
                                       BORDER_WIDTH, g_theme.win_edge_inact, g_theme.title_inact);

    XSelectInput(dpy, frame,
                 SubstructureNotifyMask | ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);

    if (g_default_cursor != None) {
        XDefineCursor(dpy, frame, g_default_cursor);
    }

    XSetWindowBorderWidth(dpy, client, 0);
    printf("[szpontdesktop] decorate_window: reparenting to frame 0x%lx\n", (unsigned long)frame); fflush(stdout);
    XReparentWindow(dpy, client, frame, 0, TITLEBAR_HEIGHT);

    if (g_client_count < MAX_MANAGED_WIN) {
        client_window_t *entry = &g_clients[g_client_count++];
        memset(entry, 0, sizeof(client_window_t));
        entry->client = client;
        entry->frame = frame;
        entry->x = cx;
        entry->y = cy;
        entry->width = cw;
        entry->height = ch;
        strncpy(entry->title, title, sizeof(entry->title) - 1);

        XSelectInput(dpy, client, StructureNotifyMask | PropertyChangeMask | FocusChangeMask);

        XGrabButton(dpy, AnyButton, AnyModifier, client, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None,
                    None);

        /* Apply rounded corner shape on the window frame */
        printf("[szpontdesktop] decorate_window: applying shape\n"); fflush(stdout);
        apply_window_shape(dpy, entry);

        printf("[szpontdesktop] decorate_window: mapping client and frame\n"); fflush(stdout);
        XMapWindow(dpy, client);
        XMapWindow(dpy, frame);

        /* Refresh taskbar when new window appears */
        if (g_win_topbar != None && gc != None) {
            render_topbar(dpy, g_win_topbar, gc, g_screen_w, time(NULL));
        }
        XFlush(dpy);
        printf("[szpontdesktop] decorate_window: complete, returning entry\n"); fflush(stdout);

        return entry;
    }

    return NULL;
}

static int xerror_handler(Display *d, XErrorEvent *e) {
    (void)d;
    (void)e;
    return 0;
}

int main(int argc, char *argv[]) {
    setpgid(0, 0);
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    const char *disp_name = (argc > 1) ? argv[1] : getenv("DISPLAY");
    if (!disp_name || !*disp_name)
        disp_name = ":0";

    printf("[szpontdesktop] Initializing Szpont Experience on '%s'...\n", disp_name);
    Display *dpy = XOpenDisplay(disp_name);
    if (!dpy) {
        fprintf(stderr, "[szpontdesktop] Fatal: Cannot connect to X server '%s'!\n", disp_name);
        return 1;
    }
    setenv("DISPLAY", disp_name, 1);
    setenv("XDG_CURRENT_DESKTOP", "SzpontOS", 0);
    setenv("XDG_SESSION_TYPE", "x11", 0);
    setenv("XDG_RUNTIME_DIR", "/tmp", 0);
    setenv("XFILESEARCHPATH", "/etc/X11/app-defaults/%N:/usr/share/X11/app-defaults/%N:/usr/lib/X11/app-defaults/%N", 1);
    setenv("XAPPLRESDIR", "/etc/X11/app-defaults", 1);

    XSetErrorHandler(xerror_handler);

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    g_screen_w = DisplayWidth(dpy, screen);
    g_screen_h = DisplayHeight(dpy, screen);

    /* Initialize XShape extension for rounded window borders */
    int shape_event_base, shape_error_base;
    g_has_shape = XShapeQueryExtension(dpy, &shape_event_base, &shape_error_base);
    printf("[szpontdesktop] XShape extension: %s\n",
           g_has_shape ? "available (rounded borders enabled)" : "unavailable");

    printf("[szpontdesktop] 1. Initializing theme...\n"); fflush(stdout);
    init_theme(dpy, screen);

    printf("[szpontdesktop] 2. Creating wallpaper...\n"); fflush(stdout);
    g_wallpaper_pix = create_gradient_wallpaper(dpy, root, screen, g_screen_h);
    if (g_wallpaper_pix != None) {
        printf("[szpontdesktop] 3. Applying wallpaper pixmap to root...\n"); fflush(stdout);
        XSetWindowBackgroundPixmap(dpy, root, g_wallpaper_pix);
        Atom prop_root = XInternAtom(dpy, "_XROOTPMAP_ID", False);
        Atom prop_eset = XInternAtom(dpy, "ESETROOT_PMAP_ID", False);
        XChangeProperty(dpy, root, prop_root, XA_PIXMAP, 32, PropModeReplace, (unsigned char *)&g_wallpaper_pix, 1);
        XChangeProperty(dpy, root, prop_eset, XA_PIXMAP, 32, PropModeReplace, (unsigned char *)&g_wallpaper_pix, 1);
        XClearWindow(dpy, root);
    }

    printf("[szpontdesktop] 4. Creating TopBar window...\n"); fflush(stdout);
    g_win_topbar = XCreateSimpleWindow(dpy, root, 0, 0, (unsigned int)g_screen_w, TOPBAR_HEIGHT, 0,
                                       g_theme.taskbar_edge, g_theme.taskbar_bg);
    XSetWindowAttributes top_attr;
    top_attr.override_redirect = True;
    XChangeWindowAttributes(dpy, g_win_topbar, CWOverrideRedirect, &top_attr);
    XSelectInput(dpy, g_win_topbar, ExposureMask | ButtonPressMask | KeyPressMask);
    XMapWindow(dpy, g_win_topbar);
    XRaiseWindow(dpy, g_win_topbar);

    GC gc = XCreateGC(dpy, root, 0, NULL);

    printf("[szpontdesktop] 5. Rendering initial TopBar...\n"); fflush(stdout);
    time_t last_time = 0;
    render_topbar(dpy, g_win_topbar, gc, g_screen_w, time(NULL));
    XFlush(dpy);

    printf("[szpontdesktop] 6. Creating cursor...\n"); fflush(stdout);
    g_default_cursor = create_default_cursor(dpy, root);
    if (g_default_cursor != None) {
        XDefineCursor(dpy, root, g_default_cursor);
        XDefineCursor(dpy, g_win_topbar, g_default_cursor);
    }

    printf("[szpontdesktop] 7. Selecting root window redirection and events...\n"); fflush(stdout);
    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);
    XGrabButton(dpy, 1, Mod1Mask, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);

    printf("[szpontdesktop] 8. Grabbing key shortcuts...\n"); fflush(stdout);
    KeyCode kc_1 = XKeysymToKeycode(dpy, XK_1);
    KeyCode kc_2 = XKeysymToKeycode(dpy, XK_2);
    KeyCode kc_3 = XKeysymToKeycode(dpy, XK_3);
    KeyCode kc_4 = XKeysymToKeycode(dpy, XK_4);
    KeyCode kc_q = XKeysymToKeycode(dpy, XK_q);
    KeyCode kc_w = XKeysymToKeycode(dpy, XK_w);
    KeyCode kc_f4 = XKeysymToKeycode(dpy, XK_F4);
    if (kc_1)
        XGrabKey(dpy, kc_1, Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);
    if (kc_2)
        XGrabKey(dpy, kc_2, Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);
    if (kc_3)
        XGrabKey(dpy, kc_3, Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);
    if (kc_4)
        XGrabKey(dpy, kc_4, Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);
    if (kc_q)
        XGrabKey(dpy, kc_q, Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);
    if (kc_w)
        XGrabKey(dpy, kc_w, Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);
    if (kc_f4)
        XGrabKey(dpy, kc_f4, Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);

    printf("[szpontdesktop] Desktop environment and Window Manager initialized. Entering event loop.\n");
    fflush(stdout);

    int x11_fd = ConnectionNumber(dpy);
    int running = 1;

    while (running) {
        /* Process all pending X11 events */
        while (XPending(dpy) > 0) {
            XEvent ev;
            XNextEvent(dpy, &ev);

            switch (ev.type) {
            case MapRequest: {
                Window w = ev.xmaprequest.window;
                printf("[szpontdesktop] MapRequest for window 0x%lx\n", (unsigned long)w); fflush(stdout);
                client_window_t *cw = decorate_window(dpy, root, w, gc);
                printf("[szpontdesktop] MapRequest: decorate_window returned cw=%p\n", cw); fflush(stdout);
                if (cw) {
                    if (!g_focused_client || strstr(cw->title, "szponterm") || strstr(cw->title, "SzponTerm") ||
                        strstr(cw->title, "xterm") || strstr(cw->title, "XTerm")) {
                        printf("[szpontdesktop] MapRequest: setting focus to new client '%s'\n", cw->title); fflush(stdout);
                        set_focus(dpy, cw, gc);
                    }
                } else {
                    printf("[szpontdesktop] MapRequest: mapping undecorated window 0x%lx\n", (unsigned long)w); fflush(stdout);
                    XMapWindow(dpy, w);
                }
                printf("[szpontdesktop] MapRequest complete for window 0x%lx\n", (unsigned long)w); fflush(stdout);
                break;
            }

            case ConfigureRequest: {
                client_window_t *cw = find_client_by_window(ev.xconfigurerequest.window);
                if (cw) {
                    int nw = ev.xconfigurerequest.width;
                    int nh = ev.xconfigurerequest.height;
                    if (nw > 10 && nh > 10) {
                        cw->width = nw;
                        cw->height = nh;
                        XResizeWindow(dpy, cw->client, (unsigned int)nw, (unsigned int)nh);
                        XResizeWindow(dpy, cw->frame, (unsigned int)nw, (unsigned int)(nh + TITLEBAR_HEIGHT));
                        apply_window_shape(dpy, cw);
                        render_titlebar(dpy, cw, gc, (cw == g_focused_client));
                    }
                } else {
                    XWindowChanges wc;
                    wc.x = ev.xconfigurerequest.x;
                    wc.y = ev.xconfigurerequest.y;
                    if (wc.y < TOPBAR_HEIGHT)
                        wc.y = TOPBAR_HEIGHT;
                    wc.width = ev.xconfigurerequest.width;
                    wc.height = ev.xconfigurerequest.height;
                    wc.border_width = 0;
                    wc.sibling = ev.xconfigurerequest.above;
                    wc.stack_mode = ev.xconfigurerequest.detail;
                    XConfigureWindow(dpy, ev.xconfigurerequest.window, ev.xconfigurerequest.value_mask, &wc);
                }
                break;
            }

            case ConfigureNotify: {
                client_window_t *cw = find_client_by_window(ev.xconfigure.window);
                if (cw && ev.xconfigure.window == cw->client) {
                    if (!cw->is_shaded && (cw->width != ev.xconfigure.width || cw->height != ev.xconfigure.height)) {
                        cw->width = ev.xconfigure.width;
                        cw->height = ev.xconfigure.height;
                        XResizeWindow(dpy, cw->frame, (unsigned int)cw->width,
                                      (unsigned int)(cw->height + TITLEBAR_HEIGHT));
                        apply_window_shape(dpy, cw);
                        render_titlebar(dpy, cw, gc, (cw == g_focused_client));
                    }
                }
                break;
            }

            case EnterNotify:
                /* Focus does not follow mouse; focus changes strictly on click */
                break;

            case DestroyNotify:
            case UnmapNotify: {
                Window w = (ev.type == DestroyNotify) ? ev.xdestroywindow.window : ev.xunmap.window;
                client_window_t *cw = find_client_by_window(w);
                if (!cw)
                    cw = find_client_by_frame(w);
                if (cw) {
                    if (ev.type == UnmapNotify && cw->is_shaded)
                        break;
                    Window frame = cw->frame;
                    if (g_focused_client == cw)
                        g_focused_client = NULL;
                    if (g_drag_client == cw)
                        g_drag_client = NULL;
                    remove_client(w, dpy, gc);
                    XDestroyWindow(dpy, frame);
                    if (!g_focused_client && g_client_count > 0) {
                        set_focus(dpy, &g_clients[g_client_count - 1], gc);
                    }
                }
                break;
            }

            case PropertyNotify: {
                if (ev.xproperty.atom == XA_WM_NAME) {
                    client_window_t *cw = find_client_by_window(ev.xproperty.window);
                    if (cw) {
                        char *name = NULL;
                        if (XFetchName(dpy, cw->client, &name) && name) {
                            strncpy(cw->title, name, sizeof(cw->title) - 1);
                            XFree(name);
                            render_titlebar(dpy, cw, gc, (cw == g_focused_client));
                            render_topbar(dpy, g_win_topbar, gc, g_screen_w, time(NULL));
                        }
                    }
                }
                break;
            }

            case Expose:
                if (ev.xexpose.window == g_win_topbar && ev.xexpose.count == 0) {
                    render_topbar(dpy, g_win_topbar, gc, g_screen_w, time(NULL));
                } else {
                    client_window_t *cw = find_client_by_frame(ev.xexpose.window);
                    if (cw && ev.xexpose.count == 0) {
                        render_titlebar(dpy, cw, gc, (cw == g_focused_client));
                    }
                }
                break;

            case ButtonPress: {
                Window clicked_win = ev.xbutton.window;
                if (clicked_win == root && ev.xbutton.subwindow != None) {
                    clicked_win = ev.xbutton.subwindow;
                }

                if (clicked_win == g_win_topbar && ev.xbutton.button == 1) {
                    int bx = ev.xbutton.x;
                    int by = ev.xbutton.y;
                    if (by >= 0 && by <= TOPBAR_HEIGHT) {
                        if (bx >= 8 && bx <= 100) {
                            /* Start Pill */
                            spawn_terminal();
                        } else if (bx >= 106 && bx <= 176) {
                            /* Term Launcher */
                            spawn_terminal();
                        } else if (bx >= 182 && bx <= 256) {
                            /* Makaljer */
                            spawn_named_app("makaljer");
                        } else if (bx >= 262 && bx <= 336) {
                            /* Detected */
                            spawn_named_app("szpontdetected");
                        } else if (bx >= g_screen_w - 60) {
                            /* Exit / Logout */
                            printf("[szpontdesktop] Logout clicked. Exiting session...\n");
                            fflush(stdout);
                            running = 0;
                        } else {
                            /* Check dynamic window taskbar tabs */
                            int task_start_x = 344;
                            const char *curr_user = getenv("USER");
                            if (!curr_user || !*curr_user)
                                curr_user = "szpont";
                            int user_w = 78;
                            int clock_w = 146;
                            int exit_x = g_screen_w - 48;
                            int clock_x = exit_x - 8 - clock_w;
                            int user_x = clock_x - 8 - user_w;
                            int task_end_x = user_x - 8;
                            int avail_w = task_end_x - task_start_x;

                            if (g_client_count > 0 && avail_w > 80 && bx >= task_start_x && bx < task_end_x) {
                                int max_item_w = 140;
                                int gap = 6;
                                int item_w = max_item_w;
                                if (g_client_count * (max_item_w + gap) > avail_w) {
                                    item_w = (avail_w / g_client_count) - gap;
                                    if (item_w < 64)
                                        item_w = 64;
                                }

                                int idx = (bx - task_start_x) / (item_w + gap);
                                if (idx >= 0 && idx < g_client_count) {
                                    client_window_t *cw = &g_clients[idx];
                                    if (cw == g_focused_client) {
                                        if (!cw->is_shaded) {
                                            cw->is_shaded = true;
                                            XUnmapWindow(dpy, cw->client);
                                            XResizeWindow(dpy, cw->frame, (unsigned int)cw->width, TITLEBAR_HEIGHT);
                                            apply_window_shape(dpy, cw);
                                            render_titlebar(dpy, cw, gc, false);
                                        } else {
                                            cw->is_shaded = false;
                                            XMapWindow(dpy, cw->client);
                                            XResizeWindow(dpy, cw->frame, (unsigned int)cw->width,
                                                          (unsigned int)(cw->height + TITLEBAR_HEIGHT));
                                            apply_window_shape(dpy, cw);
                                            set_focus(dpy, cw, gc);
                                        }
                                    } else {
                                        if (cw->is_shaded) {
                                            cw->is_shaded = false;
                                            XMapWindow(dpy, cw->client);
                                            XResizeWindow(dpy, cw->frame, (unsigned int)cw->width,
                                                          (unsigned int)(cw->height + TITLEBAR_HEIGHT));
                                            apply_window_shape(dpy, cw);
                                        }
                                        set_focus(dpy, cw, gc);
                                    }
                                    render_topbar(dpy, g_win_topbar, gc, g_screen_w, time(NULL));
                                }
                            }
                        }
                    }
                } else {
                    client_window_t *cw = find_client_by_frame(clicked_win);
                    if (!cw)
                        cw = find_client_by_window(clicked_win);
                    if (!cw && ev.xbutton.subwindow != None) {
                        cw = find_client_by_window(ev.xbutton.subwindow);
                        if (!cw)
                            cw = find_client_by_frame(ev.xbutton.subwindow);
                    }

                    if (cw) {
                        set_focus(dpy, cw, gc);

                        if (ev.xbutton.button == 1) {
                            int click_x = (ev.xbutton.window == cw->frame) ? ev.xbutton.x : (ev.xbutton.x_root - cw->x);
                            int click_y = (ev.xbutton.window == cw->frame) ? ev.xbutton.y : (ev.xbutton.y_root - cw->y);

                            if ((clicked_win == cw->frame || ev.xbutton.window == cw->frame || click_y < TITLEBAR_HEIGHT) &&
                                click_y >= 0 && click_y < TITLEBAR_HEIGHT) {
                                int dot_y = (TITLEBAR_HEIGHT - 12) / 2;
                                if (click_x >= 8 && click_x <= 26 && click_y >= dot_y - 3 && click_y <= dot_y + 15) {
                                    /* Red close dot */
                                    close_client(dpy, cw);
                                } else if (click_x >= 28 && click_x <= 44 && click_y >= dot_y - 3 && click_y <= dot_y + 15) {
                                    /* Yellow shade/minimize dot */
                                    if (!cw->is_shaded) {
                                        cw->is_shaded = true;
                                        XUnmapWindow(dpy, cw->client);
                                        XResizeWindow(dpy, cw->frame, (unsigned int)cw->width, TITLEBAR_HEIGHT);
                                        apply_window_shape(dpy, cw);
                                    } else {
                                        cw->is_shaded = false;
                                        XMapWindow(dpy, cw->client);
                                        XResizeWindow(dpy, cw->frame, (unsigned int)cw->width,
                                                      (unsigned int)(cw->height + TITLEBAR_HEIGHT));
                                        apply_window_shape(dpy, cw);
                                    }
                                    render_titlebar(dpy, cw, gc, (cw == g_focused_client));
                                    render_topbar(dpy, g_win_topbar, gc, g_screen_w, time(NULL));
                                } else if (click_x >= 46 && click_x <= 62 && click_y >= dot_y - 3 && click_y <= dot_y + 15) {
                                    /* Green maximize/restore dot */
                                    if (!cw->is_maximized) {
                                        cw->saved_x = cw->x;
                                        cw->saved_y = cw->y;
                                        cw->saved_width = cw->width;
                                        cw->saved_height = cw->height;
                                        cw->is_maximized = true;

                                        int max_w = g_screen_w - (BORDER_WIDTH * 2);
                                        int max_h = g_screen_h - TOPBAR_HEIGHT - (BORDER_WIDTH * 2);
                                        cw->x = 0;
                                        cw->y = TOPBAR_HEIGHT;
                                        cw->width = max_w;
                                        cw->height = max_h - TITLEBAR_HEIGHT;

                                        XMoveResizeWindow(dpy, cw->frame, 0, TOPBAR_HEIGHT, (unsigned int)max_w,
                                                          (unsigned int)max_h);
                                        XMoveResizeWindow(dpy, cw->client, 0, TITLEBAR_HEIGHT, (unsigned int)cw->width,
                                                          (unsigned int)cw->height);
                                        apply_window_shape(dpy, cw);
                                    } else {
                                        cw->is_maximized = false;
                                        cw->x = cw->saved_x;
                                        cw->y = cw->saved_y;
                                        cw->width = cw->saved_width;
                                        cw->height = cw->saved_height;

                                        XMoveResizeWindow(dpy, cw->frame, cw->x, cw->y, (unsigned int)cw->width,
                                                          (unsigned int)(cw->height + TITLEBAR_HEIGHT));
                                        XMoveResizeWindow(dpy, cw->client, 0, TITLEBAR_HEIGHT, (unsigned int)cw->width,
                                                          (unsigned int)cw->height);
                                        apply_window_shape(dpy, cw);
                                    }
                                    render_titlebar(dpy, cw, gc, (cw == g_focused_client));
                                } else {
                                    /* Drag titlebar */
                                    g_drag_client = cw;
                                    g_drag_start_x = ev.xbutton.x_root;
                                    g_drag_start_y = ev.xbutton.y_root;
                                    g_drag_win_x = cw->x;
                                    g_drag_win_y = cw->y;
                                    XGrabPointer(dpy, root, False, ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
                                                 GrabModeAsync, None, None, CurrentTime);
                                }
                            } else if (ev.xbutton.state & Mod1Mask) {
                                /* Alt + Left Click drag anywhere */
                                g_drag_client = cw;
                                g_drag_start_x = ev.xbutton.x_root;
                                g_drag_start_y = ev.xbutton.y_root;
                                g_drag_win_x = cw->x;
                                g_drag_win_y = cw->y;
                                XGrabPointer(dpy, root, False, ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
                                             GrabModeAsync, None, None, CurrentTime);
                            }
                        }
                    }
                }
                break;
            }

            case MotionNotify:
                if (g_drag_client) {
                    int dx = ev.xmotion.x_root - g_drag_start_x;
                    int dy = ev.xmotion.y_root - g_drag_start_y;
                    int new_x = g_drag_win_x + dx;
                    int new_y = g_drag_win_y + dy;
                    if (new_y < TOPBAR_HEIGHT)
                        new_y = TOPBAR_HEIGHT;
                    XMoveWindow(dpy, g_drag_client->frame, new_x, new_y);
                    g_drag_client->x = new_x;
                    g_drag_client->y = new_y;
                }
                break;

            case ButtonRelease:
                if (ev.xbutton.button == 1 && g_drag_client) {
                    XUngrabPointer(dpy, CurrentTime);
                    g_drag_client = NULL;
                }
                break;

            case KeyPress: {
                KeySym sym = XLookupKeysym(&ev.xkey, 0);
                if (ev.xkey.state & (Mod1Mask | Mod4Mask)) {
                    if (sym == XK_1 || sym == XK_t || sym == XK_T) {
                        spawn_terminal();
                    } else if (sym == XK_2 || sym == XK_m || sym == XK_M) {
                        spawn_named_app("makaljer");
                    } else if (sym == XK_3 || sym == XK_d || sym == XK_D) {
                        spawn_named_app("szpontdetected");
                    } else if (sym == XK_w || sym == XK_W || sym == XK_F4) {
                        if (g_focused_client) {
                            close_client(dpy, g_focused_client);
                        }
                    } else if (sym == XK_4 || sym == XK_q || sym == XK_Q || sym == XK_Escape) {
                        printf("[szpontdesktop] Logout requested via key. Exiting session...\n");
                        running = 0;
                    }
                }
                break;
            }
            }
        }

        /* Update clock once per second */
        time_t cur_time = time(NULL);
        if (cur_time != last_time) {
            last_time = cur_time;
            render_topbar(dpy, g_win_topbar, gc, g_screen_w, cur_time);
            XFlush(dpy);
        }

        /* Reap any terminated child processes */
        while (waitpid(-1, NULL, WNOHANG) > 0) {
        }

        /* Efficient sleep waiting for X11 events or next clock tick */
        struct pollfd pfd;
        pfd.fd = x11_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        poll(&pfd, 1, 50); /* 50ms responsive timeout */
    }

    printf("[szpontdesktop] Cleaning up and shutting down desktop session...\n");
    kill(0, SIGTERM);
    usleep(25000);
    kill(0, SIGKILL);
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }

    if (g_wallpaper_pix != None) {
        XFreePixmap(dpy, g_wallpaper_pix);
        g_wallpaper_pix = None;
    }

    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, g_win_topbar);
    XCloseDisplay(dpy);
    return 0;
}
