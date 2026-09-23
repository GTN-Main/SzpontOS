#pragma once

#include <SzpontUI/Core/Geometry.hpp>
#include <SzpontUI/Gfx/BitmapSurface.hpp>
#include <string>

namespace SzpontUI {

class Window;

class IPlatformBackend {
public:
    virtual ~IPlatformBackend() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    virtual void *create_window(Window *win, Size size, const std::string &title) = 0;
    virtual void destroy_window(void *handle) = 0;
    virtual void set_title(void *handle, const std::string &title) = 0;
    virtual void resize_window(void *handle, Size size) = 0;
    virtual void show_window(void *handle) = 0;
    virtual void hide_window(void *handle) = 0;

    virtual void present(void *handle, const BitmapSurface &surface, const Rect &dirty_rect) = 0;

    virtual int connection_fd() const = 0;
    virtual void pump_events() = 0;

    virtual void set_position(void *handle, Point pos) = 0;
    virtual void set_override_redirect(void *handle, bool val) = 0;
    virtual Size screen_size() const = 0;
};

} // namespace SzpontUI
