#pragma once

#include <SzpontUI/Platform/PlatformBackend.hpp>
#include <map>
#include <cstdint>

// Forward declarations for X11 types to keep header lean
typedef struct _XDisplay Display;
typedef unsigned long XID;
typedef XID Window_X11;
typedef struct _XGC *GC;
typedef struct _XImage XImage;

#include <functional>

namespace SzpontUI {

using RawEventFilter = std::function<bool(const void *native_event)>;

struct X11WindowData {
    Window *ui_window{nullptr};
    Window_X11 x_win{0};
    GC gc{nullptr};
    Size size;
    XImage *ximage{nullptr};
    bool use_shm{false};
    void *shm_info{nullptr};
    char *shmaddr{nullptr};
};

class X11Backend : public IPlatformBackend {
public:
    static X11Backend &instance();

    bool initialize() override;
    void shutdown() override;

    void *create_window(Window *win, Size size, const std::string &title) override;
    void destroy_window(void *handle) override;
    void set_title(void *handle, const std::string &title) override;
    void resize_window(void *handle, Size size) override;
    void show_window(void *handle) override;
    void hide_window(void *handle) override;

    void present(void *handle, const BitmapSurface &surface, const Rect &dirty_rect) override;

    int connection_fd() const override;
    void pump_events() override;

    void set_position(void *handle, Point pos) override;
    void set_override_redirect(void *handle, bool val) override;
    Size screen_size() const override;

    Display *display() const { return dpy_; }
    Window_X11 root_window() const { return root_; }
    int screen() const { return screen_; }

    void set_raw_event_filter(RawEventFilter filter) { raw_event_filter_ = std::move(filter); }

private:
    X11Backend();
    ~X11Backend() override;

    Display *dpy_{nullptr};
    int screen_{0};
    Window_X11 root_{0};
    unsigned long wm_delete_window_{0};
    bool has_shm_{false};
    std::map<Window_X11, X11WindowData*> windows_;
    RawEventFilter raw_event_filter_;
};

} // namespace SzpontUI
