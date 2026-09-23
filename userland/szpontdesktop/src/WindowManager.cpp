#include "WindowManager.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

namespace SzpontDesktop {

static unsigned long make_hex_color(Display *dpy, int screen, uint32_t hex) {
    (void)dpy; (void)screen;
    return (unsigned long)(hex & 0x00FFFFFF);
}

WindowManager::WindowManager(Display *dpy, int screen, Window root, const DesktopConfig &config)
    : dpy_(dpy), screen_(screen), root_(root), config_(config) {
    screen_w_ = DisplayWidth(dpy_, screen_);
    screen_h_ = DisplayHeight(dpy_, screen_);
}

WindowManager::~WindowManager() {
    if (gc_) {
        XFreeGC(dpy_, gc_);
        gc_ = nullptr;
    }
}

void WindowManager::initialize() {
    gc_ = XCreateGC(dpy_, root_, 0, nullptr);

    int shape_ev, shape_err;
    has_shape_ = XShapeQueryExtension(dpy_, &shape_ev, &shape_err);
    printf("[szpontdesktop] XShape extension: %s\n",
           has_shape_ ? "available (rounded borders enabled)" : "unavailable");

    col_win_edge_active_ = make_hex_color(dpy_, screen_, 0x3a4252);
    col_win_edge_inact_  = make_hex_color(dpy_, screen_, 0x242832);
    col_title_active_    = make_hex_color(dpy_, screen_, 0x181b24);
    col_title_inact_     = make_hex_color(dpy_, screen_, 0x12141a);
    col_title_highlight_ = make_hex_color(dpy_, screen_, 0x2e3544);
    col_title_divider_   = make_hex_color(dpy_, screen_, 0x262c38);
    col_traffic_close_   = make_hex_color(dpy_, screen_, 0xff5f56);
    col_traffic_min_     = make_hex_color(dpy_, screen_, 0xffbd2e);
    col_traffic_max_     = make_hex_color(dpy_, screen_, 0x27c93f);
    col_traffic_idle_    = make_hex_color(dpy_, screen_, 0x323846);
    col_traffic_border_  = make_hex_color(dpy_, screen_, 0x1e222c);
    col_text_white_      = make_hex_color(dpy_, screen_, 0xf1f5f9);
    col_text_muted_      = make_hex_color(dpy_, screen_, 0x64748b);

    // Select SubstructureRedirect & Notify on root to intercept client window creation
    XSelectInput(dpy_, root_, SubstructureRedirectMask | SubstructureNotifyMask);
    XGrabButton(dpy_, 1, Mod1Mask, root_, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);

    // Global Key shortcuts
    KeyCode kc_1  = XKeysymToKeycode(dpy_, XK_1);
    KeyCode kc_2  = XKeysymToKeycode(dpy_, XK_2);
    KeyCode kc_3  = XKeysymToKeycode(dpy_, XK_3);
    KeyCode kc_4  = XKeysymToKeycode(dpy_, XK_4);
    KeyCode kc_q  = XKeysymToKeycode(dpy_, XK_q);
    KeyCode kc_w  = XKeysymToKeycode(dpy_, XK_w);
    KeyCode kc_f4 = XKeysymToKeycode(dpy_, XK_F4);
    KeyCode kc_tab = XKeysymToKeycode(dpy_, XK_Tab);
    KeyCode kc_super = XKeysymToKeycode(dpy_, XK_Super_L);
    KeyCode kc_f1  = XKeysymToKeycode(dpy_, XK_F1);
    KeyCode kc_space = XKeysymToKeycode(dpy_, XK_space);
    KeyCode kc_m  = XKeysymToKeycode(dpy_, XK_m);

    if (kc_1)  XGrabKey(dpy_, kc_1,  Mod1Mask, root_, True, GrabModeAsync, GrabModeAsync);
    if (kc_2)  XGrabKey(dpy_, kc_2,  Mod1Mask, root_, True, GrabModeAsync, GrabModeAsync);
    if (kc_3)  XGrabKey(dpy_, kc_3,  Mod1Mask, root_, True, GrabModeAsync, GrabModeAsync);
    if (kc_4)  XGrabKey(dpy_, kc_4,  Mod1Mask, root_, True, GrabModeAsync, GrabModeAsync);
    if (kc_q)  XGrabKey(dpy_, kc_q,  Mod1Mask, root_, True, GrabModeAsync, GrabModeAsync);
    if (kc_w)  XGrabKey(dpy_, kc_w,  Mod1Mask, root_, True, GrabModeAsync, GrabModeAsync);
    if (kc_f4) XGrabKey(dpy_, kc_f4, Mod1Mask, root_, True, GrabModeAsync, GrabModeAsync);
    if (kc_tab) XGrabKey(dpy_, kc_tab, Mod1Mask, root_, True, GrabModeAsync, GrabModeAsync);
    if (kc_super) XGrabKey(dpy_, kc_super, AnyModifier, root_, True, GrabModeAsync, GrabModeAsync);
    if (kc_f1) XGrabKey(dpy_, kc_f1, Mod1Mask, root_, True, GrabModeAsync, GrabModeAsync);
    if (kc_space) XGrabKey(dpy_, kc_space, Mod1Mask, root_, True, GrabModeAsync, GrabModeAsync);
    if (kc_m)  XGrabKey(dpy_, kc_m,  Mod1Mask, root_, True, GrabModeAsync, GrabModeAsync);
}

ClientWindow *WindowManager::find_by_client(Window client) const {
    for (const auto &cw : clients_) {
        if (cw->client == client) return cw.get();
    }
    return nullptr;
}

ClientWindow *WindowManager::find_by_frame(Window frame) const {
    for (const auto &cw : clients_) {
        if (cw->frame == frame) return cw.get();
    }
    return nullptr;
}

void WindowManager::apply_window_shape(ClientWindow *cw) {
    if (!has_shape_ || !cw || cw->frame == None) return;

    if (cw->is_maximized) {
        XShapeCombineMask(dpy_, cw->frame, ShapeBounding, 0, 0, None, ShapeSet);
        XShapeCombineMask(dpy_, cw->frame, ShapeClip, 0, 0, None, ShapeSet);
        return;
    }

    int w = cw->width;
    int h = cw->height + TITLEBAR_HEIGHT;
    int r_out = config_.window_radius;
    int r_in = (r_out > 1) ? (r_out - 1) : 1;

    if (w <= 2 * r_out || h <= 2 * r_out) return;

    int bw = config_.border_width;
    int total_w = w + 2 * bw;
    int total_h = h + 2 * bw;

    // 1. ShapeBounding mask
    Pixmap mask_b = XCreatePixmap(dpy_, cw->frame, (unsigned int)total_w, (unsigned int)total_h, 1);
    if (mask_b != None) {
        GC mgc = XCreateGC(dpy_, mask_b, 0, nullptr);
        XSetForeground(dpy_, mgc, 0);
        XFillRectangle(dpy_, mask_b, mgc, 0, 0, (unsigned int)total_w, (unsigned int)total_h);

        XSetForeground(dpy_, mgc, 1);
        int d_out = r_out * 2;
        XFillRectangle(dpy_, mask_b, mgc, 0, r_out, (unsigned int)total_w, (unsigned int)(total_h - d_out));
        XFillRectangle(dpy_, mask_b, mgc, r_out, 0, (unsigned int)(total_w - d_out), (unsigned int)total_h);

        XFillArc(dpy_, mask_b, mgc, 0, 0, (unsigned int)d_out, (unsigned int)d_out, 90 * 64, 90 * 64);
        XFillArc(dpy_, mask_b, mgc, total_w - d_out, 0, (unsigned int)d_out, (unsigned int)d_out, 0 * 64, 90 * 64);
        XFillArc(dpy_, mask_b, mgc, 0, total_h - d_out, (unsigned int)d_out, (unsigned int)d_out, 180 * 64, 90 * 64);
        XFillArc(dpy_, mask_b, mgc, total_w - d_out, total_h - d_out, (unsigned int)d_out, (unsigned int)d_out, 270 * 64, 90 * 64);

        XShapeCombineMask(dpy_, cw->frame, ShapeBounding, -bw, -bw, mask_b, ShapeSet);
        XFreeGC(dpy_, mgc);
        XFreePixmap(dpy_, mask_b);
    }

    // 2. ShapeClip mask
    Pixmap mask_c = XCreatePixmap(dpy_, cw->frame, (unsigned int)w, (unsigned int)h, 1);
    if (mask_c != None) {
        GC mgc = XCreateGC(dpy_, mask_c, 0, nullptr);
        XSetForeground(dpy_, mgc, 0);
        XFillRectangle(dpy_, mask_c, mgc, 0, 0, (unsigned int)w, (unsigned int)h);

        XSetForeground(dpy_, mgc, 1);
        int d_in = r_in * 2;
        XFillRectangle(dpy_, mask_c, mgc, 0, r_in, (unsigned int)w, (unsigned int)(h - d_in));
        XFillRectangle(dpy_, mask_c, mgc, r_in, 0, (unsigned int)(w - d_in), (unsigned int)h);

        XFillArc(dpy_, mask_c, mgc, 0, 0, (unsigned int)d_in, (unsigned int)d_in, 90 * 64, 90 * 64);
        XFillArc(dpy_, mask_c, mgc, w - d_in, 0, (unsigned int)d_in, (unsigned int)d_in, 0 * 64, 90 * 64);
        XFillArc(dpy_, mask_c, mgc, 0, h - d_in, (unsigned int)d_in, (unsigned int)d_in, 180 * 64, 90 * 64);
        XFillArc(dpy_, mask_c, mgc, w - d_in, h - d_in, (unsigned int)d_in, (unsigned int)d_in, 270 * 64, 90 * 64);

        XShapeCombineMask(dpy_, cw->frame, ShapeClip, 0, 0, mask_c, ShapeSet);
        XFreeGC(dpy_, mgc);
        XFreePixmap(dpy_, mask_c);
    }
}

void WindowManager::render_titlebar(ClientWindow *cw, bool is_focused) {
    if (!cw || cw->frame == None || !gc_) return;

    // 1. Titlebar background
    XSetForeground(dpy_, gc_, is_focused ? col_title_active_ : col_title_inact_);
    XFillRectangle(dpy_, cw->frame, gc_, 0, 0, (unsigned int)cw->width, TITLEBAR_HEIGHT);

    // 2. Top accent highlight line
    XSetForeground(dpy_, gc_, is_focused ? col_title_highlight_ : col_win_edge_inact_);
    int top_start = has_shape_ ? config_.window_radius : 0;
    int top_end = has_shape_ ? (cw->width - config_.window_radius) : cw->width;
    if (top_end > top_start) {
        XDrawLine(dpy_, cw->frame, gc_, top_start, 0, top_end, 0);
    }

    // 3. Bottom divider line
    XSetForeground(dpy_, gc_, is_focused ? col_title_divider_ : col_win_edge_inact_);
    XDrawLine(dpy_, cw->frame, gc_, 0, TITLEBAR_HEIGHT - 1, cw->width, TITLEBAR_HEIGHT - 1);

    // 4. Traffic light control dots (12px diameter)
    int dot_y = (TITLEBAR_HEIGHT - 12) / 2;
    int dot_x1 = 14;
    int dot_x2 = 32;
    int dot_x3 = 50;

    if (is_focused) {
        // Close (Red)
        XSetForeground(dpy_, gc_, col_traffic_close_);
        XFillArc(dpy_, cw->frame, gc_, dot_x1, dot_y, 12, 12, 0, 360 * 64);
        XSetForeground(dpy_, gc_, col_traffic_border_);
        XDrawArc(dpy_, cw->frame, gc_, dot_x1, dot_y, 12, 12, 0, 360 * 64);

        // Minimize (Yellow)
        XSetForeground(dpy_, gc_, col_traffic_min_);
        XFillArc(dpy_, cw->frame, gc_, dot_x2, dot_y, 12, 12, 0, 360 * 64);
        XSetForeground(dpy_, gc_, col_traffic_border_);
        XDrawArc(dpy_, cw->frame, gc_, dot_x2, dot_y, 12, 12, 0, 360 * 64);

        // Maximize (Green)
        XSetForeground(dpy_, gc_, col_traffic_max_);
        XFillArc(dpy_, cw->frame, gc_, dot_x3, dot_y, 12, 12, 0, 360 * 64);
        XSetForeground(dpy_, gc_, col_traffic_border_);
        XDrawArc(dpy_, cw->frame, gc_, dot_x3, dot_y, 12, 12, 0, 360 * 64);
    } else {
        XSetForeground(dpy_, gc_, col_traffic_idle_);
        XFillArc(dpy_, cw->frame, gc_, dot_x1, dot_y, 12, 12, 0, 360 * 64);
        XFillArc(dpy_, cw->frame, gc_, dot_x2, dot_y, 12, 12, 0, 360 * 64);
        XFillArc(dpy_, cw->frame, gc_, dot_x3, dot_y, 12, 12, 0, 360 * 64);

        XSetForeground(dpy_, gc_, col_traffic_border_);
        XDrawArc(dpy_, cw->frame, gc_, dot_x1, dot_y, 12, 12, 0, 360 * 64);
        XDrawArc(dpy_, cw->frame, gc_, dot_x2, dot_y, 12, 12, 0, 360 * 64);
        XDrawArc(dpy_, cw->frame, gc_, dot_x3, dot_y, 12, 12, 0, 360 * 64);
    }

    // 5. Window Title Text
    XSetForeground(dpy_, gc_, is_focused ? col_text_white_ : col_text_muted_);
    int text_y = dot_y + 10;
    XDrawString(dpy_, cw->frame, gc_, 72, text_y, cw->title.c_str(), (int)cw->title.length());

    // 6. Border width & color
    XSetWindowBorderWidth(dpy_, cw->frame, config_.border_width);
    XSetWindowBorder(dpy_, cw->frame, is_focused ? col_win_edge_active_ : col_win_edge_inact_);
}

void WindowManager::set_focus(ClientWindow *cw) {
    if (!cw || cw->is_minimized) return;

    if (focused_client_ && focused_client_ != cw) {
        XGrabButton(dpy_, AnyButton, AnyModifier, focused_client_->client, False,
                    ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
        render_titlebar(focused_client_, false);
    }

    focused_client_ = cw;
    XUngrabButton(dpy_, AnyButton, AnyModifier, cw->client);
    XRaiseWindow(dpy_, cw->frame);
    render_titlebar(cw, true);
    XSetInputFocus(dpy_, cw->client, RevertToParent, CurrentTime);

    if (on_focus_changed) {
        on_focus_changed(cw);
    }
    XFlush(dpy_);
}

void WindowManager::minimize_window(ClientWindow *cw) {
    if (!cw || cw->is_minimized) return;
    cw->is_minimized = true;

    Atom wm_state = XInternAtom(dpy_, "WM_STATE", False);
    long data[2] = { IconicState, None };
    XChangeProperty(dpy_, cw->client, wm_state, wm_state, 32, PropModeReplace, (unsigned char*)data, 2);

    XUnmapWindow(dpy_, cw->frame);
    XUnmapWindow(dpy_, cw->client);

    if (focused_client_ == cw) {
        focused_client_ = nullptr;
        for (auto it = clients_.rbegin(); it != clients_.rend(); ++it) {
            if (!(*it)->is_minimized) {
                set_focus(it->get());
                break;
            }
        }
        if (!focused_client_) {
            XSetInputFocus(dpy_, root_, RevertToPointerRoot, CurrentTime);
        }
    }

    if (on_windows_changed) on_windows_changed();
    XFlush(dpy_);
}

void WindowManager::restore_window(ClientWindow *cw) {
    if (!cw || !cw->is_minimized) return;
    cw->is_minimized = false;

    Atom wm_state = XInternAtom(dpy_, "WM_STATE", False);
    long data[2] = { NormalState, None };
    XChangeProperty(dpy_, cw->client, wm_state, wm_state, 32, PropModeReplace, (unsigned char*)data, 2);

    XMapWindow(dpy_, cw->client);
    XMapWindow(dpy_, cw->frame);
    set_focus(cw);

    if (on_windows_changed) on_windows_changed();
    XFlush(dpy_);
}

void WindowManager::maximize_window(ClientWindow *cw) {
    if (!cw) return;

    if (!cw->is_maximized) {
        cw->saved_x = cw->x;
        cw->saved_y = cw->y;
        cw->saved_width = cw->width;
        cw->saved_height = cw->height;
        cw->is_maximized = true;

        int bw = config_.border_width;
        int max_w = screen_w_ - (bw * 2);
        int dock_reserve = config_.dock_enabled ? 72 : 0;
        int max_h = screen_h_ - TOPBAR_HEIGHT - dock_reserve - (bw * 2);

        cw->x = 0;
        cw->y = TOPBAR_HEIGHT;
        cw->width = max_w;
        cw->height = max_h - TITLEBAR_HEIGHT;

        XMoveResizeWindow(dpy_, cw->frame, 0, TOPBAR_HEIGHT, (unsigned int)max_w, (unsigned int)max_h);
        XMoveResizeWindow(dpy_, cw->client, 0, TITLEBAR_HEIGHT, (unsigned int)cw->width, (unsigned int)cw->height);
        apply_window_shape(cw);
    } else {
        cw->is_maximized = false;
        cw->x = cw->saved_x;
        cw->y = cw->saved_y;
        cw->width = cw->saved_width;
        cw->height = cw->saved_height;

        XMoveResizeWindow(dpy_, cw->frame, cw->x, cw->y, (unsigned int)cw->width,
                          (unsigned int)(cw->height + TITLEBAR_HEIGHT));
        XMoveResizeWindow(dpy_, cw->client, 0, TITLEBAR_HEIGHT, (unsigned int)cw->width,
                          (unsigned int)cw->height);
        apply_window_shape(cw);
    }

    render_titlebar(cw, (cw == focused_client_));
    XFlush(dpy_);
}

void WindowManager::close_window(ClientWindow *cw) {
    if (!cw || !cw->client) return;

    cw->close_attempts++;
    if (cw->close_attempts > 1) {
        XKillClient(dpy_, cw->client);
        XFlush(dpy_);
        return;
    }

    Atom wm_del = XInternAtom(dpy_, "WM_DELETE_WINDOW", False);
    Atom wm_proto = XInternAtom(dpy_, "WM_PROTOCOLS", False);

    Atom *protocols = nullptr;
    int count = 0;
    bool has_delete = false;
    if (XGetWMProtocols(dpy_, cw->client, &protocols, &count) && protocols) {
        for (int i = 0; i < count; ++i) {
            if (protocols[i] == wm_del) {
                has_delete = true;
                break;
            }
        }
        XFree(protocols);
    }

    if (has_delete) {
        XEvent msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = ClientMessage;
        msg.xclient.window = cw->client;
        msg.xclient.message_type = wm_proto;
        msg.xclient.format = 32;
        msg.xclient.data.l[0] = (long)wm_del;
        msg.xclient.data.l[1] = CurrentTime;
        XSendEvent(dpy_, cw->client, False, NoEventMask, &msg);
    } else {
        XKillClient(dpy_, cw->client);
    }
    XFlush(dpy_);
}

ClientWindow *WindowManager::decorate_window(Window client) {
    if (client == root_ || client == None) return nullptr;
    if (find_by_client(client) || find_by_frame(client)) return nullptr;

    Window root_ret = None;
    int cx = 30, cy = TOPBAR_HEIGHT + 15;
    unsigned int cw = 640, ch = 480, border_w = 0, depth = 0;
    if (!XGetGeometry(dpy_, client, &root_ret, &cx, &cy, &cw, &ch, &border_w, &depth)) {
        return nullptr;
    }

    if (cy < TOPBAR_HEIGHT) cy = TOPBAR_HEIGHT + 10;
    if (cx < 10) cx = 10;

    char title_buf[128] = "SzpontOS Application";
    char *name = nullptr;
    if (XFetchName(dpy_, client, &name) && name) {
        strncpy(title_buf, name, sizeof(title_buf) - 1);
        XFree(name);
    }

    Window frame = XCreateSimpleWindow(
        dpy_, root_, cx, cy, (unsigned int)cw, (unsigned int)(ch + TITLEBAR_HEIGHT),
        config_.border_width, col_win_edge_inact_, col_title_inact_
    );

    XSelectInput(dpy_, frame, SubstructureNotifyMask | ExposureMask | ButtonPressMask |
                              ButtonReleaseMask | PointerMotionMask);

    XSetWindowBorderWidth(dpy_, client, 0);
    XReparentWindow(dpy_, client, frame, 0, TITLEBAR_HEIGHT);

    auto entry = std::make_unique<ClientWindow>();
    entry->client = client;
    entry->frame = frame;
    entry->x = cx;
    entry->y = cy;
    entry->width = cw;
    entry->height = ch;
    entry->title = title_buf;

    // Detect app_id from title or WM_CLASS
    XClassHint hint;
    if (XGetClassHint(dpy_, client, &hint)) {
        if (hint.res_name) entry->app_id = hint.res_name;
        if (hint.res_name) XFree(hint.res_name);
        if (hint.res_class) XFree(hint.res_class);
    }

    XSelectInput(dpy_, client, StructureNotifyMask | PropertyChangeMask | FocusChangeMask);
    XGrabButton(dpy_, AnyButton, AnyModifier, client, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);

    apply_window_shape(entry.get());
    XMapWindow(dpy_, client);
    XMapWindow(dpy_, frame);

    ClientWindow *ptr = entry.get();
    clients_.push_back(std::move(entry));

    if (on_windows_changed) on_windows_changed();
    XFlush(dpy_);
    return ptr;
}

void WindowManager::remove_client(Window win) {
    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
        if ((*it)->client == win || (*it)->frame == win) {
            Window frame = (*it)->frame;
            if (focused_client_ == it->get()) {
                focused_client_ = nullptr;
            }
            if (drag_client_ == it->get()) {
                drag_client_ = nullptr;
            }
            clients_.erase(it);
            XDestroyWindow(dpy_, frame);

            if (!focused_client_ && !clients_.empty()) {
                for (auto rit = clients_.rbegin(); rit != clients_.rend(); ++rit) {
                    if (!(*rit)->is_minimized) {
                        set_focus(rit->get());
                        break;
                    }
                }
            }
            if (!focused_client_) {
                XSetInputFocus(dpy_, root_, RevertToPointerRoot, CurrentTime);
            }

            if (on_windows_changed) on_windows_changed();
            XFlush(dpy_);
            break;
        }
    }
}

bool WindowManager::handle_event(const XEvent &ev) {
    switch (ev.type) {
    case MapRequest: {
        Window w = ev.xmaprequest.window;
        ClientWindow *cw = decorate_window(w);
        if (cw) {
            set_focus(cw);
        } else {
            XMapWindow(dpy_, w);
        }
        return true;
    }

    case ConfigureRequest: {
        ClientWindow *cw = find_by_client(ev.xconfigurerequest.window);
        if (cw) {
            int nw = ev.xconfigurerequest.width;
            int nh = ev.xconfigurerequest.height;
            if (nw > 10 && nh > 10) {
                cw->width = nw;
                cw->height = nh;
                XResizeWindow(dpy_, cw->client, (unsigned int)nw, (unsigned int)nh);
                XResizeWindow(dpy_, cw->frame, (unsigned int)nw, (unsigned int)(nh + TITLEBAR_HEIGHT));
                apply_window_shape(cw);
                render_titlebar(cw, (cw == focused_client_));
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
            XConfigureWindow(dpy_, ev.xconfigurerequest.window, ev.xconfigurerequest.value_mask, &wc);
        }
        return true;
    }

    case ConfigureNotify: {
        ClientWindow *cw = find_by_client(ev.xconfigure.window);
        if (cw && ev.xconfigure.window == cw->client) {
            if (!cw->is_minimized && (cw->width != ev.xconfigure.width || cw->height != ev.xconfigure.height)) {
                cw->width = ev.xconfigure.width;
                cw->height = ev.xconfigure.height;
                XResizeWindow(dpy_, cw->frame, (unsigned int)cw->width, (unsigned int)(cw->height + TITLEBAR_HEIGHT));
                apply_window_shape(cw);
                render_titlebar(cw, (cw == focused_client_));
            }
            return true;
        }
        break;
    }

    case DestroyNotify: {
        Window w = ev.xdestroywindow.window;
        if (find_by_client(w) || find_by_frame(w)) {
            remove_client(w);
            return true;
        }
        break;
    }

    case UnmapNotify: {
        Window w = ev.xunmap.window;
        ClientWindow *cw = find_by_client(w);
        if (cw) {
            if (cw->is_minimized) {
                return true; // Window is minimized, keep it alive in WM
            }
            remove_client(w);
            return true;
        }
        break;
    }

    case PropertyNotify: {
        if (ev.xproperty.atom == XA_WM_NAME) {
            ClientWindow *cw = find_by_client(ev.xproperty.window);
            if (cw) {
                char *name = nullptr;
                if (XFetchName(dpy_, cw->client, &name) && name) {
                    cw->title = name;
                    XFree(name);
                    render_titlebar(cw, (cw == focused_client_));
                    if (on_windows_changed) on_windows_changed();
                }
                return true;
            }
        }
        break;
    }

    case Expose: {
        ClientWindow *cw = find_by_frame(ev.xexpose.window);
        if (cw && ev.xexpose.count == 0) {
            render_titlebar(cw, (cw == focused_client_));
            return true;
        }
        break;
    }

    case ButtonPress: {
        Window clicked = ev.xbutton.window;
        ClientWindow *cw = find_by_frame(clicked);
        if (!cw) cw = find_by_client(clicked);

        if (cw) {
            set_focus(cw);

            if (ev.xbutton.button == 1) {
                int click_x = (ev.xbutton.window == cw->frame) ? ev.xbutton.x : (ev.xbutton.x_root - cw->x);
                int click_y = (ev.xbutton.window == cw->frame) ? ev.xbutton.y : (ev.xbutton.y_root - cw->y);

                if ((clicked == cw->frame || click_y < TITLEBAR_HEIGHT) && click_y >= 0 && click_y < TITLEBAR_HEIGHT) {
                    int dot_y = (TITLEBAR_HEIGHT - 12) / 2;
                    if (click_x >= 8 && click_x <= 26 && click_y >= dot_y - 3 && click_y <= dot_y + 15) {
                        // Red close dot
                        close_window(cw);
                    } else if (click_x >= 28 && click_x <= 44 && click_y >= dot_y - 3 && click_y <= dot_y + 15) {
                        // Yellow minimize dot
                        minimize_window(cw);
                    } else if (click_x >= 46 && click_x <= 62 && click_y >= dot_y - 3 && click_y <= dot_y + 15) {
                        // Green maximize dot
                        maximize_window(cw);
                    } else {
                        // Drag titlebar
                        drag_client_ = cw;
                        drag_start_x_ = ev.xbutton.x_root;
                        drag_start_y_ = ev.xbutton.y_root;
                        drag_win_x_ = cw->x;
                        drag_win_y_ = cw->y;
                        XGrabPointer(dpy_, root_, False, ButtonReleaseMask | PointerMotionMask,
                                     GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
                    }
                } else if (ev.xbutton.state & Mod1Mask) {
                    // Alt + Left Click drag anywhere
                    drag_client_ = cw;
                    drag_start_x_ = ev.xbutton.x_root;
                    drag_start_y_ = ev.xbutton.y_root;
                    drag_win_x_ = cw->x;
                    drag_win_y_ = cw->y;
                    XGrabPointer(dpy_, root_, False, ButtonReleaseMask | PointerMotionMask,
                                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
                }
            }
            return true;
        }
        break;
    }

    case MotionNotify: {
        if (drag_client_) {
            int dx = ev.xmotion.x_root - drag_start_x_;
            int dy = ev.xmotion.y_root - drag_start_y_;
            int new_x = drag_win_x_ + dx;
            int new_y = drag_win_y_ + dy;
            if (new_y < TOPBAR_HEIGHT) new_y = TOPBAR_HEIGHT;
            XMoveWindow(dpy_, drag_client_->frame, new_x, new_y);
            drag_client_->x = new_x;
            drag_client_->y = new_y;
            return true;
        }
        break;
    }

    case ButtonRelease: {
        if (ev.xbutton.button == 1 && drag_client_) {
            XUngrabPointer(dpy_, CurrentTime);
            drag_client_ = nullptr;
            return true;
        }
        break;
    }

    case KeyPress: {
        KeySym sym = XLookupKeysym((XKeyEvent*)&ev.xkey, 0);
        if (sym == XK_Super_L || sym == XK_Super_R ||
            (sym == XK_F1 && (ev.xkey.state & Mod1Mask)) ||
            (sym == XK_space && (ev.xkey.state & Mod1Mask))) {
            if (on_toggle_app_menu) on_toggle_app_menu();
            return true;
        }
        if (ev.xkey.state & (Mod1Mask | Mod4Mask)) {
            if (sym == XK_1 && on_quick_launch) { on_quick_launch(0); return true; }
            if (sym == XK_2 && on_quick_launch) { on_quick_launch(1); return true; }
            if (sym == XK_3 && on_quick_launch) { on_quick_launch(2); return true; }
            if (sym == XK_4 && on_quick_launch) { on_quick_launch(3); return true; }
            if (sym == XK_Tab) {
                // Cycle windows
                if (clients_.size() > 1) {
                    size_t cur_idx = 0;
                    for (size_t i = 0; i < clients_.size(); ++i) {
                        if (clients_[i].get() == focused_client_) {
                            cur_idx = i;
                            break;
                        }
                    }
                    size_t next_idx = (cur_idx + 1) % clients_.size();
                    if (clients_[next_idx]->is_minimized) {
                        restore_window(clients_[next_idx].get());
                    } else {
                        set_focus(clients_[next_idx].get());
                    }
                }
                return true;
            } else if (sym == XK_w || sym == XK_W || sym == XK_F4) {
                if (focused_client_) {
                    close_window(focused_client_);
                }
                return true;
            } else if (sym == XK_m || sym == XK_M) {
                if (focused_client_) {
                    minimize_window(focused_client_);
                }
                return true;
            } else if (sym == XK_q || sym == XK_Q) {
                if (on_session_exit_requested) on_session_exit_requested();
                return true;
            }
        }
        break;
    }
    }

    return false;
}

} // namespace SzpontDesktop
