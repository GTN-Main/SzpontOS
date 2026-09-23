#include <SzpontUI/Platform/X11Backend.hpp>
#include <SzpontUI/Widgets/Window.hpp>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <cstdio>
#include <cstring>

namespace SzpontUI {

X11Backend &X11Backend::instance() {
    static X11Backend instance;
    return instance;
}

X11Backend::X11Backend() {
    initialize();
}

X11Backend::~X11Backend() {
    shutdown();
}

bool X11Backend::initialize() {
    if (dpy_) return true;

    dpy_ = XOpenDisplay(nullptr);
    if (!dpy_) {
        fprintf(stderr, "[SzpontUI] Failed to open X11 Display!\n");
        return false;
    }

    screen_ = DefaultScreen(dpy_);
    root_ = RootWindow(dpy_, screen_);
    wm_delete_window_ = XInternAtom(dpy_, "WM_DELETE_WINDOW", False);

    has_shm_ = (XShmQueryExtension(dpy_) == True);
    if (has_shm_) {
        printf("[SzpontUI] MIT-SHM extension is active: using zero-copy shared memory backend.\n");
    } else {
        printf("[SzpontUI] MIT-SHM extension not available: falling back to standard XPutImage.\n");
    }

    return true;
}

void X11Backend::shutdown() {
    if (!dpy_) return;

    for (auto &pair : windows_) {
        if (pair.second) {
            if (pair.second->gc) XFreeGC(dpy_, pair.second->gc);
            delete pair.second;
        }
    }
    windows_.clear();

    XCloseDisplay(dpy_);
    dpy_ = nullptr;
}

int X11Backend::connection_fd() const {
    if (!dpy_) return -1;
    return ConnectionNumber(dpy_);
}

void *X11Backend::create_window(Window *win, Size size, const std::string &title) {
    if (!dpy_ && !initialize()) return nullptr;

    int depth = DefaultDepth(dpy_, screen_);
    Visual *visual = DefaultVisual(dpy_, screen_);

    XSetWindowAttributes swa;
    swa.background_pixel = 0x101726; // Szpont theme background
    swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                     StructureNotifyMask | FocusChangeMask;

    Window_X11 x_win = XCreateWindow(
        dpy_, root_,
        100, 100,
        size.width, size.height,
        0, depth, InputOutput, visual,
        CWBackPixel | CWEventMask, &swa
    );

    if (!x_win) {
        fprintf(stderr, "[SzpontUI] XCreateWindow failed\n");
        return nullptr;
    }

    XStoreName(dpy_, x_win, title.c_str());
    Atom net_wm_name = XInternAtom(dpy_, "_NET_WM_NAME", False);
    Atom utf8_str = XInternAtom(dpy_, "UTF8_STRING", False);
    XChangeProperty(dpy_, x_win, net_wm_name, utf8_str, 8, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(title.data()), title.size());

    Atom protocols[] = { static_cast<Atom>(wm_delete_window_) };
    XSetWMProtocols(dpy_, x_win, protocols, 1);

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
    Pixmap c_src = XCreateBitmapFromData(dpy_, root_, (const char *)cursor_bits, 16, 16);
    Pixmap c_msk = XCreateBitmapFromData(dpy_, root_, (const char *)cursor_mask, 16, 16);
    XColor fg, bg;
    fg.red = 0xffff; fg.green = 0xffff; fg.blue = 0xffff; fg.flags = DoRed|DoGreen|DoBlue;
    bg.red = 0x0000; bg.green = 0x0000; bg.blue = 0x0000; bg.flags = DoRed|DoGreen|DoBlue;
    Cursor cur = XCreatePixmapCursor(dpy_, c_src, c_msk, &fg, &bg, 0, 0);
    XFreePixmap(dpy_, c_src);
    XFreePixmap(dpy_, c_msk);
    if (cur != None) {
        XDefineCursor(dpy_, x_win, cur);
        XDefineCursor(dpy_, root_, cur);
    }

    GC gc = XCreateGC(dpy_, x_win, 0, nullptr);

    auto data = new X11WindowData();
    data->ui_window = win;
    data->x_win = x_win;
    data->gc = gc;
    data->size = size;

    windows_[x_win] = data;
    return data;
}

void X11Backend::destroy_window(void *handle) {
    auto data = static_cast<X11WindowData*>(handle);
    if (!data || !dpy_) return;

    if (data->use_shm) {
        auto *shminfo = reinterpret_cast<XShmSegmentInfo*>(data->shm_info);
        if (shminfo) {
            XShmDetach(dpy_, shminfo);
            delete shminfo;
            data->shm_info = nullptr;
        }
        if (data->ximage) {
            XDestroyImage(data->ximage);
            data->ximage = nullptr;
        }
        if (data->shmaddr) {
            shmdt(data->shmaddr);
            data->shmaddr = nullptr;
        }
        data->use_shm = false;
    } else if (data->ximage) {
        data->ximage->data = nullptr;
        XDestroyImage(data->ximage);
        data->ximage = nullptr;
    }

    windows_.erase(data->x_win);
    if (data->gc) XFreeGC(dpy_, data->gc);
    XDestroyWindow(dpy_, data->x_win);
    delete data;
}

void X11Backend::set_title(void *handle, const std::string &title) {
    auto data = static_cast<X11WindowData*>(handle);
    if (!data || !dpy_) return;
    XStoreName(dpy_, data->x_win, title.c_str());
    Atom net_wm_name = XInternAtom(dpy_, "_NET_WM_NAME", False);
    Atom utf8_str = XInternAtom(dpy_, "UTF8_STRING", False);
    XChangeProperty(dpy_, data->x_win, net_wm_name, utf8_str, 8, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(title.data()), title.size());
}

void X11Backend::resize_window(void *handle, Size size) {
    auto data = static_cast<X11WindowData*>(handle);
    if (!data || !dpy_) return;

    if (data->use_shm) {
        auto *shminfo = reinterpret_cast<XShmSegmentInfo*>(data->shm_info);
        if (shminfo) {
            XShmDetach(dpy_, shminfo);
            delete shminfo;
            data->shm_info = nullptr;
        }
        if (data->ximage) {
            XDestroyImage(data->ximage);
            data->ximage = nullptr;
        }
        if (data->shmaddr) {
            shmdt(data->shmaddr);
            data->shmaddr = nullptr;
        }
        data->use_shm = false;
    } else if (data->ximage) {
        data->ximage->data = nullptr;
        XDestroyImage(data->ximage);
        data->ximage = nullptr;
    }
    data->size = size;
    XResizeWindow(dpy_, data->x_win, size.width, size.height);
}

void X11Backend::set_position(void *handle, Point pos) {
    auto data = static_cast<X11WindowData*>(handle);
    if (!data || !dpy_) return;
    XMoveWindow(dpy_, data->x_win, pos.x, pos.y);
    XFlush(dpy_);
}

void X11Backend::set_override_redirect(void *handle, bool val) {
    auto data = static_cast<X11WindowData*>(handle);
    if (!data || !dpy_) return;
    XSetWindowAttributes swa;
    swa.override_redirect = val ? True : False;
    XChangeWindowAttributes(dpy_, data->x_win, CWOverrideRedirect, &swa);
    XFlush(dpy_);
}

Size X11Backend::screen_size() const {
    if (!dpy_) return Size{1024, 768};
    return Size{DisplayWidth(dpy_, screen_), DisplayHeight(dpy_, screen_)};
}

void X11Backend::show_window(void *handle) {
    auto data = static_cast<X11WindowData*>(handle);
    if (!data || !dpy_) return;
    XMapWindow(dpy_, data->x_win);
    XFlush(dpy_);
}

void X11Backend::hide_window(void *handle) {
    auto data = static_cast<X11WindowData*>(handle);
    if (!data || !dpy_) return;
    XUnmapWindow(dpy_, data->x_win);
    XFlush(dpy_);
}

void X11Backend::present(void *handle, const BitmapSurface &surface, const Rect &dirty_rect) {
    auto data = static_cast<X11WindowData*>(handle);
    if (!data || !dpy_ || dirty_rect.is_empty()) return;

    if (!data->ximage || data->ximage->width != surface.width() || data->ximage->height != surface.height()) {
        if (data->use_shm) {
            auto *shminfo = reinterpret_cast<XShmSegmentInfo*>(data->shm_info);
            if (shminfo) {
                XShmDetach(dpy_, shminfo);
                delete shminfo;
                data->shm_info = nullptr;
            }
            if (data->ximage) {
                XDestroyImage(data->ximage);
                data->ximage = nullptr;
            }
            if (data->shmaddr) {
                shmdt(data->shmaddr);
                data->shmaddr = nullptr;
            }
            data->use_shm = false;
        } else if (data->ximage) {
            data->ximage->data = nullptr;
            XDestroyImage(data->ximage);
            data->ximage = nullptr;
        }

        Visual *visual = DefaultVisual(dpy_, screen_);
        int depth = DefaultDepth(dpy_, screen_);

        if (has_shm_) {
            auto *shminfo = new XShmSegmentInfo();
            memset(shminfo, 0, sizeof(*shminfo));
            data->ximage = XShmCreateImage(
                dpy_, visual, depth, ZPixmap,
                nullptr, shminfo,
                surface.width(), surface.height()
            );
            if (data->ximage) {
                size_t shm_size = (size_t)data->ximage->bytes_per_line * data->ximage->height;
                int shmid = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | 0777);
                if (shmid >= 0) {
                    char *addr = static_cast<char*>(shmat(shmid, nullptr, 0));
                    if (addr != (char*)-1) {
                        shminfo->shmid = shmid;
                        shminfo->shmaddr = addr;
                        shminfo->readOnly = False;
                        data->ximage->data = addr;
                        if (XShmAttach(dpy_, shminfo)) {
                            shmctl(shmid, IPC_RMID, nullptr);
                            data->use_shm = true;
                            data->shm_info = shminfo;
                            data->shmaddr = addr;
                            printf("[SzpontUI] Aktywacja akceleracji MIT-SHM dla okna %dx%d (zero-copy X11 presentation)\n",
                                   surface.width(), surface.height());
                            fflush(stdout);
                        } else {
                            shmdt(addr);
                            shmctl(shmid, IPC_RMID, nullptr);
                        }
                    } else {
                        shmctl(shmid, IPC_RMID, nullptr);
                    }
                }
                if (!data->use_shm) {
                    XDestroyImage(data->ximage);
                    data->ximage = nullptr;
                    delete shminfo;
                    data->shm_info = nullptr;
                }
            } else {
                delete shminfo;
                data->shm_info = nullptr;
            }
        }

        if (!data->ximage) {
            printf("[SzpontUI] Standardowy bufor XPutImage dla okna %dx%d\n",
                   surface.width(), surface.height());
            fflush(stdout);
            data->ximage = XCreateImage(
                dpy_, visual, depth, ZPixmap, 0,
                reinterpret_cast<char*>(const_cast<uint32_t*>(surface.pixels())),
                surface.width(), surface.height(), 32, surface.stride()
            );
        }
    }

    if (!data->ximage) return;

    if (data->use_shm) {
        // Fast copy dirty rectangle into shared memory buffer
        const uint32_t *src_pixels = surface.pixels();
        uint32_t *dst_pixels = reinterpret_cast<uint32_t*>(data->ximage->data);
        int stride_pixels = data->ximage->bytes_per_line / 4;
        int surf_stride_pixels = surface.stride() / 4;

        for (int y = dirty_rect.y; y < dirty_rect.bottom(); ++y) {
            const uint32_t *src_row = src_pixels + y * surf_stride_pixels + dirty_rect.x;
            uint32_t *dst_row = dst_pixels + y * stride_pixels + dirty_rect.x;
            memcpy(dst_row, src_row, dirty_rect.width * sizeof(uint32_t));
        }

        XShmPutImage(
            dpy_, data->x_win, data->gc, data->ximage,
            dirty_rect.x, dirty_rect.y,
            dirty_rect.x, dirty_rect.y,
            dirty_rect.width, dirty_rect.height,
            False
        );
        XFlush(dpy_);
    } else {
        data->ximage->data = reinterpret_cast<char*>(const_cast<uint32_t*>(surface.pixels()));
        XPutImage(
            dpy_, data->x_win, data->gc, data->ximage,
            dirty_rect.x, dirty_rect.y,
            dirty_rect.x, dirty_rect.y,
            dirty_rect.width, dirty_rect.height
        );
        XFlush(dpy_);
    }
}

void X11Backend::pump_events() {
    if (!dpy_) return;

    while (XPending(dpy_) > 0) {
        XEvent xev;
        XNextEvent(dpy_, &xev);

        if (raw_event_filter_ && raw_event_filter_(&xev)) {
            continue;
        }

        auto it = windows_.find(xev.xany.window);
        if (it == windows_.end() || !it->second || !it->second->ui_window) {
            continue;
        }

        X11WindowData *data = it->second;
        Window *ui_win = data->ui_window;

        switch (xev.type) {
            case Expose: {
                PaintEvent pe(Rect{xev.xexpose.x, xev.xexpose.y, xev.xexpose.width, xev.xexpose.height});
                ui_win->handle_backend_event(pe);
                break;
            }
            case ConfigureNotify: {
                if (xev.xconfigure.width != data->size.width || xev.xconfigure.height != data->size.height) {
                    data->size = Size{xev.xconfigure.width, xev.xconfigure.height};
                    ResizeEvent re(data->size);
                    ui_win->handle_backend_event(re);
                }
                break;
            }
            case MotionNotify: {
                MouseEvent me(EventType::MouseMove, Point{xev.xmotion.x, xev.xmotion.y});
                ui_win->handle_backend_event(me);
                break;
            }
            case ButtonPress: {
                MouseButton btn = MouseButton::Left;
                if (xev.xbutton.button == Button2) btn = MouseButton::Middle;
                else if (xev.xbutton.button == Button3) btn = MouseButton::Right;

                MouseEvent me(EventType::MouseDown, Point{xev.xbutton.x, xev.xbutton.y}, btn);
                ui_win->handle_backend_event(me);
                break;
            }
            case ButtonRelease: {
                MouseButton btn = MouseButton::Left;
                if (xev.xbutton.button == Button2) btn = MouseButton::Middle;
                else if (xev.xbutton.button == Button3) btn = MouseButton::Right;

                MouseEvent me(EventType::MouseUp, Point{xev.xbutton.x, xev.xbutton.y}, btn);
                ui_win->handle_backend_event(me);
                break;
            }
            case KeyPress: {
                char text_buf[32] = {0};
                KeySym ksym = NoSymbol;
                XLookupString(&xev.xkey, text_buf, sizeof(text_buf) - 1, &ksym, nullptr);

                KeyEvent ke(EventType::KeyDown, xev.xkey.keycode, static_cast<uint32_t>(ksym), text_buf);
                ui_win->handle_backend_event(ke);
                break;
            }
            case KeyRelease: {
                KeyEvent ke(EventType::KeyUp, xev.xkey.keycode, 0);
                ui_win->handle_backend_event(ke);
                break;
            }
            case FocusIn: {
                Event fe(EventType::GainedFocus);
                ui_win->handle_backend_event(fe);
                break;
            }
            case FocusOut: {
                Event fe(EventType::LostFocus);
                ui_win->handle_backend_event(fe);
                break;
            }
            case ClientMessage: {
                if (static_cast<Atom>(xev.xclient.data.l[0]) == static_cast<Atom>(wm_delete_window_)) {
                    Event ce(EventType::Close);
                    ui_win->handle_backend_event(ce);
                }
                break;
            }
            default:
                break;
        }
    }
}

} // namespace SzpontUI
