#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include "Config.hpp"

namespace SzpontDesktop {

#define TITLEBAR_HEIGHT 30
#define TOPBAR_HEIGHT 32

struct ClientWindow {
    Window client{None};
    Window frame{None};
    int x{0}, y{0};
    int width{640}, height{480};
    bool is_maximized{false};
    bool is_minimized{false};
    int saved_x{0}, saved_y{0};
    int saved_width{640}, saved_height{480};
    std::string title{"SzpontOS Application"};
    std::string app_id;
    int close_attempts{0};
};

class WindowManager {
public:
    WindowManager(Display *dpy, int screen, Window root, const DesktopConfig &config);
    ~WindowManager();

    void initialize();
    bool handle_event(const XEvent &ev);

    void minimize_window(ClientWindow *cw);
    void restore_window(ClientWindow *cw);
    void close_window(ClientWindow *cw);
    void maximize_window(ClientWindow *cw);
    void set_focus(ClientWindow *cw);

    ClientWindow *focused_client() const { return focused_client_; }
    const std::vector<std::unique_ptr<ClientWindow>> &clients() const { return clients_; }
    ClientWindow *find_by_client(Window client) const;
    ClientWindow *find_by_frame(Window frame) const;

    // Callbacks to synchronize with Panel and Dock
    std::function<void()> on_windows_changed;
    std::function<void(ClientWindow *active)> on_focus_changed;
    std::function<void()> on_session_exit_requested;
    std::function<void()> on_toggle_app_menu;
    std::function<void(int index)> on_quick_launch;

    int screen_width() const { return screen_w_; }
    int screen_height() const { return screen_h_; }

private:
    ClientWindow *decorate_window(Window client);
    void remove_client(Window win);
    void render_titlebar(ClientWindow *cw, bool is_focused);
    void apply_window_shape(ClientWindow *cw);

    Display *dpy_{nullptr};
    int screen_{0};
    Window root_{None};
    DesktopConfig config_;
    GC gc_{nullptr};
    int screen_w_{1024};
    int screen_h_{768};
    bool has_shape_{false};

    std::vector<std::unique_ptr<ClientWindow>> clients_;
    ClientWindow *focused_client_{nullptr};

    // Dragging state
    ClientWindow *drag_client_{nullptr};
    int drag_start_x_{0};
    int drag_start_y_{0};
    int drag_win_x_{0};
    int drag_win_y_{0};

    // Theme color cache
    unsigned long col_win_edge_active_{0};
    unsigned long col_win_edge_inact_{0};
    unsigned long col_title_active_{0};
    unsigned long col_title_inact_{0};
    unsigned long col_title_highlight_{0};
    unsigned long col_title_divider_{0};
    unsigned long col_traffic_close_{0};
    unsigned long col_traffic_min_{0};
    unsigned long col_traffic_max_{0};
    unsigned long col_traffic_idle_{0};
    unsigned long col_traffic_border_{0};
    unsigned long col_text_white_{0};
    unsigned long col_text_muted_{0};
};

} // namespace SzpontDesktop
