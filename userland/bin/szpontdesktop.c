/*
 * SzpontOS — Native X11 Desktop Environment & Window Manager (Szpont Experience)
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Lightweight, high-performance desktop panel and reparenting window manager.
 * Renders glassmorphic TopBar, manages window decorations (cyber titlebars,
 * traffic light controls, vibrant glowing borders), dragging, and session lifecycle.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

#define TOPBAR_HEIGHT    36
#define TITLEBAR_HEIGHT  26
#define BORDER_WIDTH     4
#define MAX_MANAGED_WIN  128

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
} client_window_t;

static client_window_t g_clients[MAX_MANAGED_WIN];
static int g_client_count = 0;
static client_window_t *g_focused_client = NULL;
static client_window_t *g_drag_client = NULL;
static int g_drag_start_x = 0;
static int g_drag_start_y = 0;
static int g_drag_win_x = 0;
static int g_drag_win_y = 0;

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

static pid_t spawn_app(const char *binary) {
    pid_t pid = fork();
    if (pid == 0) {
        char *args[] = {(char *)binary, NULL};
        extern char **environ;
        execve(binary, args, environ);
        fprintf(stderr, "[szpontdesktop] Failed to exec '%s'\n", binary);
        _exit(1);
    }
    if (pid > 0) {
        printf("[szpontdesktop] Spawned process '%s' (PID %d)\n", binary, pid);
    }
    return pid;
}

static void render_topbar(Display *dpy, Window win, GC gc, int screen_w, time_t now,
                          unsigned long titlebar_bg, unsigned long blue_color,
                          unsigned long cyan_color, unsigned long green_color,
                          unsigned long pink_color, unsigned long yellow_color,
                          unsigned long close_btn_col) {
    /* TopBar background */
    XSetForeground(dpy, gc, titlebar_bg);
    XFillRectangle(dpy, win, gc, 0, 0, (unsigned int)screen_w, TOPBAR_HEIGHT);

    /* Bottom accent line */
    XSetForeground(dpy, gc, blue_color);
    XDrawLine(dpy, win, gc, 0, TOPBAR_HEIGHT - 1, screen_w, TOPBAR_HEIGHT - 1);

    /* Brand Logo */
    XSetForeground(dpy, gc, cyan_color);
    XDrawString(dpy, win, gc, 16, 22, "Szpont Experience", 16);

    /* [1] + SzponTerm */
    XSetForeground(dpy, gc, green_color);
    XDrawString(dpy, win, gc, 180, 22, "[1] + SzponTerm", 15);

    /* [2] Makaljer */
    XSetForeground(dpy, gc, pink_color);
    XDrawString(dpy, win, gc, 320, 22, "[2] Makaljer", 12);

    /* [3] Detected */
    XSetForeground(dpy, gc, yellow_color);
    XDrawString(dpy, win, gc, 450, 22, "[3] Szpont Detected", 19);

    /* [User: ...] badge */
    const char *curr_user = getenv("USER");
    if (!curr_user || !*curr_user) curr_user = "szpont";
    char user_badge[64];
    snprintf(user_badge, sizeof(user_badge), "[User: %s]", curr_user);
    XSetForeground(dpy, gc, cyan_color);
    XDrawString(dpy, win, gc, 640, 22, user_badge, (int)strlen(user_badge));

    /* [4] Logout */
    XSetForeground(dpy, gc, close_btn_col);
    XDrawString(dpy, win, gc, 770, 22, "[4] Logout", 10);

    /* Right-aligned Clock */
    struct tm *tm_info = gmtime(&now);
    char clock_buf[64];
    if (tm_info) {
        snprintf(clock_buf, sizeof(clock_buf), "UTC: %04d-%02d-%02d %02d:%02d:%02d",
                 tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                 tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    } else {
        snprintf(clock_buf, sizeof(clock_buf), "SzpontOS Display :0");
    }
    XSetForeground(dpy, gc, cyan_color);
    XDrawString(dpy, win, gc, screen_w - 240, 22, clock_buf, (int)strlen(clock_buf));
}

/* Standard 16x16 crisp arrow cursor */
static const unsigned char cursor_bits[] = {
    0x01, 0x00, 0x03, 0x00, 0x07, 0x00, 0x0f, 0x00,
    0x1f, 0x00, 0x3f, 0x00, 0x7f, 0x00, 0xff, 0x00,
    0x7f, 0x00, 0x1f, 0x00, 0x3b, 0x00, 0x71, 0x00,
    0xe0, 0x00, 0xc0, 0x01, 0x80, 0x01, 0x00, 0x00
};
static const unsigned char cursor_mask[] = {
    0x03, 0x00, 0x07, 0x00, 0x0f, 0x00, 0x1f, 0x00,
    0x3f, 0x00, 0x7f, 0x00, 0xff, 0x00, 0xff, 0x01,
    0xff, 0x01, 0xff, 0x00, 0x7f, 0x00, 0xfb, 0x00,
    0xf1, 0x01, 0xe0, 0x03, 0xc0, 0x03, 0x80, 0x01
};

static Cursor g_default_cursor = None;

static Cursor create_default_cursor(Display *dpy, Window root) {
    Pixmap src = XCreateBitmapFromData(dpy, root, (const char *)cursor_bits, 16, 16);
    Pixmap msk = XCreateBitmapFromData(dpy, root, (const char *)cursor_mask, 16, 16);
    XColor fg, bg;
    fg.red = 0xffff; fg.green = 0xffff; fg.blue = 0xffff; fg.flags = DoRed|DoGreen|DoBlue;
    bg.red = 0x0000; bg.green = 0x0000; bg.blue = 0x0000; bg.flags = DoRed|DoGreen|DoBlue;
    Cursor c = XCreatePixmapCursor(dpy, src, msk, &fg, &bg, 0, 0);
    XFreePixmap(dpy, src);
    XFreePixmap(dpy, msk);
    return c;
}

static client_window_t *find_client_by_frame(Window frame) {
    for (int i = 0; i < g_client_count; i++) {
        if (g_clients[i].frame == frame) return &g_clients[i];
    }
    return NULL;
}

static client_window_t *find_client_by_window(Window client) {
    for (int i = 0; i < g_client_count; i++) {
        if (g_clients[i].client == client) return &g_clients[i];
    }
    return NULL;
}

static void remove_client(Window client) {
    for (int i = 0; i < g_client_count; i++) {
        if (g_clients[i].client == client || g_clients[i].frame == client) {
            for (int j = i; j < g_client_count - 1; j++) {
                g_clients[j] = g_clients[j + 1];
            }
            g_client_count--;
            break;
        }
    }
}

static void render_titlebar(Display *dpy, client_window_t *cw, GC gc, bool is_focused,
                            unsigned long titlebar_bg, unsigned long titlebar_unf,
                            unsigned long cyan_color, unsigned long border_color,
                            unsigned long close_btn_col, unsigned long min_btn_col,
                            unsigned long max_btn_col, unsigned long text_focused,
                            unsigned long text_unf) {
    if (!cw || cw->frame == None) return;

    /* 1. Titlebar background */
    XSetForeground(dpy, gc, is_focused ? titlebar_bg : titlebar_unf);
    XFillRectangle(dpy, cw->frame, gc, 0, 0, (unsigned int)cw->width, TITLEBAR_HEIGHT);

    /* 2. Top accent highlight line */
    XSetForeground(dpy, gc, is_focused ? cyan_color : border_color);
    XDrawLine(dpy, cw->frame, gc, 0, 0, cw->width, 0);

    /* 3. Bottom accent separator line */
    XSetForeground(dpy, gc, is_focused ? cyan_color : border_color);
    XDrawLine(dpy, cw->frame, gc, 0, TITLEBAR_HEIGHT - 1, cw->width, TITLEBAR_HEIGHT - 1);

    /* 4. Traffic light control dots */
    /* Close (Red) */
    XSetForeground(dpy, gc, close_btn_col);
    XFillArc(dpy, cw->frame, gc, 10, 7, 12, 12, 0, 360 * 64);

    /* Minimize (Yellow) */
    XSetForeground(dpy, gc, min_btn_col);
    XFillArc(dpy, cw->frame, gc, 28, 7, 12, 12, 0, 360 * 64);

    /* Maximize (Green) */
    XSetForeground(dpy, gc, max_btn_col);
    XFillArc(dpy, cw->frame, gc, 46, 7, 12, 12, 0, 360 * 64);

    /* 5. Window Title Text */
    XSetForeground(dpy, gc, is_focused ? text_focused : text_unf);
    XDrawString(dpy, cw->frame, gc, 68, 18, cw->title, (int)strlen(cw->title));

    /* 6. Border width & color on the frame */
    XSetWindowBorderWidth(dpy, cw->frame, BORDER_WIDTH);
    XSetWindowBorder(dpy, cw->frame, is_focused ? cyan_color : border_color);
}

static void set_focus(Display *dpy, client_window_t *cw, GC gc,
                      unsigned long titlebar_bg, unsigned long titlebar_unf,
                      unsigned long cyan_color, unsigned long border_color,
                      unsigned long close_btn_col, unsigned long min_btn_col,
                      unsigned long max_btn_col, unsigned long text_focused,
                      unsigned long text_unf) {
    if (!cw) return;
    if (g_focused_client && g_focused_client != cw) {
        /* Grab any button on previously focused client so clicking anywhere refocuses it */
        XGrabButton(dpy, AnyButton, AnyModifier, g_focused_client->client, False,
                    ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
        render_titlebar(dpy, g_focused_client, gc, false,
                        titlebar_bg, titlebar_unf, cyan_color, border_color,
                        close_btn_col, min_btn_col, max_btn_col, text_focused, text_unf);
    }
    g_focused_client = cw;
    /* Ungrab buttons on active client so application handles clicks directly */
    XUngrabButton(dpy, AnyButton, AnyModifier, cw->client);
    XRaiseWindow(dpy, cw->frame);
    render_titlebar(dpy, cw, gc, true,
                    titlebar_bg, titlebar_unf, cyan_color, border_color,
                    close_btn_col, min_btn_col, max_btn_col, text_focused, text_unf);
    XSetInputFocus(dpy, cw->client, RevertToParent, CurrentTime);
}

static void close_client(Display *dpy, client_window_t *cw) {
    if (!cw) return;
    Atom wm_proto = XInternAtom(dpy, "WM_PROTOCOLS", False);
    Atom wm_del = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XEvent msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = ClientMessage;
    msg.xclient.window = cw->client;
    msg.xclient.message_type = wm_proto;
    msg.xclient.format = 32;
    msg.xclient.data.l[0] = (long)wm_del;
    msg.xclient.data.l[1] = CurrentTime;
    XSendEvent(dpy, cw->client, False, NoEventMask, &msg);
}

static client_window_t *decorate_window(Display *dpy, Window root, Window client, Window win_topbar,
                                        unsigned long border_color, unsigned long titlebar_bg) {
    if (client == root || client == win_topbar) return NULL;
    client_window_t *existing = find_client_by_window(client);
    if (existing) return existing;
    if (find_client_by_frame(client)) return NULL;

    XWindowAttributes attr;
    if (!XGetWindowAttributes(dpy, client, &attr)) return NULL;
    if (attr.override_redirect) return NULL;

    int cx = attr.x;
    int cy = attr.y;
    int cw = attr.width;
    int ch = attr.height;
    if (cy < TOPBAR_HEIGHT) cy = TOPBAR_HEIGHT + 10;
    if (cx < 10) cx = 10;

    char title[128] = "SzpontOS Application";
    char *name = NULL;
    if (XFetchName(dpy, client, &name) && name) {
        strncpy(title, name, sizeof(title) - 1);
        XFree(name);
    }

    Window frame = XCreateSimpleWindow(dpy, root, cx, cy,
                                       (unsigned int)cw, (unsigned int)(ch + TITLEBAR_HEIGHT),
                                       BORDER_WIDTH, border_color, titlebar_bg);

    /* Frame events: Do NOT select SubstructureRedirectMask on frame, only substructure notifications */
    XSelectInput(dpy, frame, SubstructureNotifyMask | ExposureMask | ButtonPressMask |
                             ButtonReleaseMask | PointerMotionMask);

    if (g_default_cursor != None) {
        XDefineCursor(dpy, frame, g_default_cursor);
    }

    XSetWindowBorderWidth(dpy, client, 0);
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

        /* Initial button grab so clicking on unfocused client window will focus & raise it */
        XGrabButton(dpy, AnyButton, AnyModifier, client, False,
                    ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);

        XMapWindow(dpy, client);
        XMapWindow(dpy, frame);
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
    const char *disp_name = (argc > 1) ? argv[1] : getenv("DISPLAY");
    if (!disp_name || !*disp_name) disp_name = ":0";

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

    XSetErrorHandler(xerror_handler);

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    int screen_w = DisplayWidth(dpy, screen);

    /* Palette */
    unsigned long titlebar_bg   = make_rgb(dpy, screen, 0x1818, 0x1818, 0x2525); /* Dark charcoal header */
    unsigned long titlebar_unf  = make_rgb(dpy, screen, 0x1111, 0x1111, 0x1b1b); /* Dimmed header */
    unsigned long cyan_color    = make_rgb(dpy, screen, 0x0000, 0xf0f0, 0xffff); /* Electric Cyan #00f0ff (Active) */
    unsigned long pink_color    = make_rgb(dpy, screen, 0xf5f5, 0xc2c2, 0xe7e7);
    unsigned long green_color   = make_rgb(dpy, screen, 0xa6a6, 0xe3e3, 0xa1a1);
    unsigned long yellow_color  = make_rgb(dpy, screen, 0xf9f9, 0xe2e2, 0xafaf);
    unsigned long blue_color    = make_rgb(dpy, screen, 0x8989, 0xb4b4, 0xfafa);
    unsigned long border_color  = make_rgb(dpy, screen, 0x8181, 0x8c8c, 0xf8f8); /* Vibrant Indigo #818cf8 (Inactive) */
    unsigned long close_btn_col = make_rgb(dpy, screen, 0xefef, 0x4444, 0x4444); /* Red #ef4444 */
    unsigned long min_btn_col   = make_rgb(dpy, screen, 0xf5f5, 0x9e9e, 0x0b0b); /* Yellow #f59e0b */
    unsigned long max_btn_col   = make_rgb(dpy, screen, 0x1010, 0xb9b9, 0x8181); /* Green #10b981 */
    unsigned long text_focused  = make_rgb(dpy, screen, 0xf8f8, 0xfafa, 0xfcfc); /* Soft White */
    unsigned long text_unf      = make_rgb(dpy, screen, 0x9494, 0xa3a3, 0xb8b8); /* Slate */

    /* Top Menu Bar Window (screen_w x 36) with override_redirect */
    Window win_topbar = XCreateSimpleWindow(dpy, root, 0, 0,
                                           (unsigned int)screen_w, TOPBAR_HEIGHT, 0,
                                           border_color, titlebar_unf);
    XSetWindowAttributes top_attr;
    top_attr.override_redirect = True;
    XChangeWindowAttributes(dpy, win_topbar, CWOverrideRedirect, &top_attr);
    XSelectInput(dpy, win_topbar, ExposureMask | ButtonPressMask | KeyPressMask);
    XMapWindow(dpy, win_topbar);

    GC gc = XCreateGC(dpy, root, 0, NULL);

    time_t last_time = 0;
    render_topbar(dpy, win_topbar, gc, screen_w, time(NULL),
                  titlebar_unf, blue_color, cyan_color, green_color,
                  pink_color, yellow_color, close_btn_col);
    XFlush(dpy);

    /* Initialize default arrow cursor for root and all desktop windows */
    g_default_cursor = create_default_cursor(dpy, root);
    if (g_default_cursor != None) {
        XDefineCursor(dpy, root, g_default_cursor);
        XDefineCursor(dpy, win_topbar, g_default_cursor);
    }

    /* Window Manager: select root redirection and window management events */
    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask |
                            ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XGrabButton(dpy, 1, Mod1Mask, root, True,
                ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);

    /* Grab global Alt+1..4 and Alt+q shortcuts on root */
    KeyCode kc_1 = XKeysymToKeycode(dpy, XK_1);
    KeyCode kc_2 = XKeysymToKeycode(dpy, XK_2);
    KeyCode kc_3 = XKeysymToKeycode(dpy, XK_3);
    KeyCode kc_4 = XKeysymToKeycode(dpy, XK_4);
    KeyCode kc_q = XKeysymToKeycode(dpy, XK_q);
    if (kc_1) XGrabKey(dpy, kc_1, Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);
    if (kc_2) XGrabKey(dpy, kc_2, Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);
    if (kc_3) XGrabKey(dpy, kc_3, Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);
    if (kc_4) XGrabKey(dpy, kc_4, Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);
    if (kc_q) XGrabKey(dpy, kc_q, Mod1Mask, root, True, GrabModeAsync, GrabModeAsync);

    /* Decorate any existing windows */
    Window root_ret, parent_ret, *children = NULL;
    unsigned int nchildren = 0;
    if (XQueryTree(dpy, root, &root_ret, &parent_ret, &children, &nchildren)) {
        for (unsigned int i = 0; i < nchildren; i++) {
            if (children[i] != win_topbar && children[i] != root) {
                decorate_window(dpy, root, children[i], win_topbar, border_color, titlebar_bg);
            }
        }
        if (children) XFree(children);
    }

    /* Automatically spawn initial native application processes (szponterm last to be foreground) */
    spawn_app("/bin/makaljer");
    spawn_app("/bin/szpontdetected");
    spawn_app("/bin/szponterm");

    printf("[szpontdesktop] Desktop environment and Window Manager initialized. Entering event loop.\n");

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
                client_window_t *cw = decorate_window(dpy, root, w, win_topbar, border_color, titlebar_bg);
                if (cw) {
                    if (!g_focused_client || strstr(cw->title, "szponterm") || strstr(cw->title, "SzponTerm")) {
                        set_focus(dpy, cw, gc, titlebar_bg, titlebar_unf, cyan_color, border_color,
                                  close_btn_col, min_btn_col, max_btn_col, text_focused, text_unf);
                    }
                } else {
                    XMapWindow(dpy, w);
                }
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
                        render_titlebar(dpy, cw, gc, (cw == g_focused_client),
                                        titlebar_bg, titlebar_unf, cyan_color, border_color,
                                        close_btn_col, min_btn_col, max_btn_col, text_focused, text_unf);
                    }
                } else {
                    XWindowChanges wc;
                    wc.x = ev.xconfigurerequest.x;
                    wc.y = ev.xconfigurerequest.y;
                    if (wc.y < TOPBAR_HEIGHT) wc.y = TOPBAR_HEIGHT;
                    wc.width = ev.xconfigurerequest.width;
                    wc.height = ev.xconfigurerequest.height;
                    wc.border_width = 0;
                    wc.sibling = ev.xconfigurerequest.above;
                    wc.stack_mode = ev.xconfigurerequest.detail;
                    XConfigureWindow(dpy, ev.xconfigurerequest.window,
                                     ev.xconfigurerequest.value_mask, &wc);
                }
                break;
            }

            case ConfigureNotify: {
                client_window_t *cw = find_client_by_window(ev.xconfigure.window);
                if (cw && ev.xconfigure.window == cw->client) {
                    if (!cw->is_shaded && (cw->width != ev.xconfigure.width || cw->height != ev.xconfigure.height)) {
                        cw->width = ev.xconfigure.width;
                        cw->height = ev.xconfigure.height;
                        XResizeWindow(dpy, cw->frame, (unsigned int)cw->width, (unsigned int)(cw->height + TITLEBAR_HEIGHT));
                        render_titlebar(dpy, cw, gc, (cw == g_focused_client),
                                        titlebar_bg, titlebar_unf, cyan_color, border_color,
                                        close_btn_col, min_btn_col, max_btn_col, text_focused, text_unf);
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
                if (cw) {
                    Window frame = cw->frame;
                    if (g_focused_client == cw) g_focused_client = NULL;
                    if (g_drag_client == cw) g_drag_client = NULL;
                    remove_client(w);
                    XDestroyWindow(dpy, frame);
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
                            render_titlebar(dpy, cw, gc, (cw == g_focused_client),
                                            titlebar_bg, titlebar_unf, cyan_color, border_color,
                                            close_btn_col, min_btn_col, max_btn_col, text_focused, text_unf);
                        }
                    }
                }
                break;
            }

            case Expose:
                if (ev.xexpose.window == win_topbar && ev.xexpose.count == 0) {
                    render_topbar(dpy, win_topbar, gc, screen_w, time(NULL),
                                  titlebar_unf, blue_color, cyan_color, green_color,
                                  pink_color, yellow_color, close_btn_col);
                } else {
                    client_window_t *cw = find_client_by_frame(ev.xexpose.window);
                    if (cw && ev.xexpose.count == 0) {
                        render_titlebar(dpy, cw, gc, (cw == g_focused_client),
                                        titlebar_bg, titlebar_unf, cyan_color, border_color,
                                        close_btn_col, min_btn_col, max_btn_col, text_focused, text_unf);
                    }
                }
                break;

            case ButtonPress:
                if (ev.xbutton.window == win_topbar && ev.xbutton.button == 1) {
                    int bx = ev.xbutton.x;
                    if (bx >= 170 && bx < 300) {
                        spawn_app("/bin/szponterm");
                    } else if (bx >= 310 && bx < 430) {
                        spawn_app("/bin/makaljer");
                    } else if (bx >= 440 && bx < 630) {
                        spawn_app("/bin/szpontdetected");
                    } else if (bx >= 760 && bx < 870) {
                        printf("[szpontdesktop] Logout clicked. Exiting session...\n");
                        running = 0;
                    }
                } else {
                    client_window_t *cw = find_client_by_frame(ev.xbutton.window);
                    if (!cw) cw = find_client_by_window(ev.xbutton.window);
                    if (!cw && ev.xbutton.subwindow != None) {
                        cw = find_client_by_window(ev.xbutton.subwindow);
                        if (!cw) cw = find_client_by_frame(ev.xbutton.subwindow);
                    }

                    if (cw) {
                        set_focus(dpy, cw, gc, titlebar_bg, titlebar_unf, cyan_color, border_color,
                                  close_btn_col, min_btn_col, max_btn_col, text_focused, text_unf);

                        if (ev.xbutton.button == 1) {
                            if (ev.xbutton.window == cw->frame && ev.xbutton.y < TITLEBAR_HEIGHT) {
                                if (ev.xbutton.x >= 6 && ev.xbutton.x <= 24) {
                                    /* Red close dot */
                                    close_client(dpy, cw);
                                } else if (ev.xbutton.x >= 25 && ev.xbutton.x <= 42) {
                                    /* Yellow shade/minimize dot */
                                    if (!cw->is_shaded) {
                                        cw->is_shaded = true;
                                        XUnmapWindow(dpy, cw->client);
                                        XResizeWindow(dpy, cw->frame, (unsigned int)cw->width, TITLEBAR_HEIGHT);
                                    } else {
                                        cw->is_shaded = false;
                                        XMapWindow(dpy, cw->client);
                                        XResizeWindow(dpy, cw->frame, (unsigned int)cw->width, (unsigned int)(cw->height + TITLEBAR_HEIGHT));
                                    }
                                    render_titlebar(dpy, cw, gc, (cw == g_focused_client),
                                                    titlebar_bg, titlebar_unf, cyan_color, border_color,
                                                    close_btn_col, min_btn_col, max_btn_col, text_focused, text_unf);
                                } else if (ev.xbutton.x >= 43 && ev.xbutton.x <= 60) {
                                    /* Green maximize/restore dot */
                                    if (!cw->is_maximized) {
                                        cw->saved_x = cw->x;
                                        cw->saved_y = cw->y;
                                        cw->saved_width = cw->width;
                                        cw->saved_height = cw->height;
                                        cw->is_maximized = true;

                                        int screen_h = DisplayHeight(dpy, screen);
                                        int max_w = screen_w - (BORDER_WIDTH * 2);
                                        int max_h = screen_h - TOPBAR_HEIGHT - (BORDER_WIDTH * 2);
                                        cw->x = 0;
                                        cw->y = TOPBAR_HEIGHT;
                                        cw->width = max_w;
                                        cw->height = max_h - TITLEBAR_HEIGHT;

                                        XMoveResizeWindow(dpy, cw->frame, 0, TOPBAR_HEIGHT, (unsigned int)max_w, (unsigned int)max_h);
                                        XMoveResizeWindow(dpy, cw->client, 0, TITLEBAR_HEIGHT, (unsigned int)cw->width, (unsigned int)cw->height);
                                    } else {
                                        cw->is_maximized = false;
                                        cw->x = cw->saved_x;
                                        cw->y = cw->saved_y;
                                        cw->width = cw->saved_width;
                                        cw->height = cw->saved_height;

                                        XMoveResizeWindow(dpy, cw->frame, cw->x, cw->y, (unsigned int)cw->width, (unsigned int)(cw->height + TITLEBAR_HEIGHT));
                                        XMoveResizeWindow(dpy, cw->client, 0, TITLEBAR_HEIGHT, (unsigned int)cw->width, (unsigned int)cw->height);
                                    }
                                    render_titlebar(dpy, cw, gc, (cw == g_focused_client),
                                                    titlebar_bg, titlebar_unf, cyan_color, border_color,
                                                    close_btn_col, min_btn_col, max_btn_col, text_focused, text_unf);
                                } else {
                                    /* Drag titlebar */
                                    g_drag_client = cw;
                                    g_drag_start_x = ev.xbutton.x_root;
                                    g_drag_start_y = ev.xbutton.y_root;
                                    g_drag_win_x = cw->x;
                                    g_drag_win_y = cw->y;
                                    XGrabPointer(dpy, root, False,
                                                 ButtonReleaseMask | PointerMotionMask,
                                                 GrabModeAsync, GrabModeAsync,
                                                 None, None, CurrentTime);
                                }
                            } else if (ev.xbutton.state & Mod1Mask) {
                                /* Alt + Left Click drag anywhere */
                                g_drag_client = cw;
                                g_drag_start_x = ev.xbutton.x_root;
                                g_drag_start_y = ev.xbutton.y_root;
                                g_drag_win_x = cw->x;
                                g_drag_win_y = cw->y;
                                XGrabPointer(dpy, root, False,
                                             ButtonReleaseMask | PointerMotionMask,
                                             GrabModeAsync, GrabModeAsync,
                                             None, None, CurrentTime);
                            } else {
                                /* Click inside client window: replay pointer event to app */
                                XAllowEvents(dpy, ReplayPointer, CurrentTime);
                            }
                        } else {
                            XAllowEvents(dpy, ReplayPointer, CurrentTime);
                        }
                    }
                }
                break;

            case MotionNotify:
                if (g_drag_client) {
                    int dx = ev.xmotion.x_root - g_drag_start_x;
                    int dy = ev.xmotion.y_root - g_drag_start_y;
                    int new_x = g_drag_win_x + dx;
                    int new_y = g_drag_win_y + dy;
                    if (new_y < TOPBAR_HEIGHT) new_y = TOPBAR_HEIGHT;
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
                        spawn_app("/bin/szponterm");
                    } else if (sym == XK_2 || sym == XK_m || sym == XK_M) {
                        spawn_app("/bin/makaljer");
                    } else if (sym == XK_3 || sym == XK_d || sym == XK_D) {
                        spawn_app("/bin/szpontdetected");
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
            render_topbar(dpy, win_topbar, gc, screen_w, cur_time,
                          titlebar_unf, blue_color, cyan_color, green_color,
                          pink_color, yellow_color, close_btn_col);
            XFlush(dpy);
        }

        /* Reap any terminated child processes */
        while (waitpid(-1, NULL, WNOHANG) > 0) {}

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
    while (waitpid(-1, NULL, WNOHANG) > 0) {}

    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win_topbar);
    XCloseDisplay(dpy);
    return 0;
}
