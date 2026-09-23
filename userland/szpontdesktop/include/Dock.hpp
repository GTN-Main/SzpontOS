#pragma once

#include <SzpontUI/SzpontUI.hpp>
#include <vector>
#include <string>
#include <functional>
#include "XdgApp.hpp"
#include "WindowManager.hpp"
#include "Config.hpp"
#include "IconLoader.hpp"

namespace SzpontDesktop {

struct DockItem {
    std::string id;
    std::string name;
    std::string icon;
    const XdgApp *app{nullptr};
    ClientWindow *client{nullptr}; // running window if any
    bool is_running{false};
    bool is_active{false};
    bool is_minimized{false};
};

class DockWindow : public SzpontUI::Window {
public:
    DockWindow(int screen_w, int screen_h, const std::vector<XdgApp> &apps,
               WindowManager &wm, const DesktopConfig &config);

    void refresh();

    std::function<void(const XdgApp &app)> on_launch_app;

protected:
    void on_paint(SzpontUI::Painter &painter) override;
    void on_mouse_down(SzpontUI::MouseEvent &event) override;
    void on_mouse_move(SzpontUI::MouseEvent &event) override;

private:
    struct ItemHitRect {
        DockItem item;
        SzpontUI::Rect rect;
    };

    int screen_w_;
    int screen_h_;
    std::vector<XdgApp> all_apps_;
    WindowManager &wm_;
    DesktopConfig config_;

    std::vector<DockItem> items_;
    std::vector<ItemHitRect> hit_rects_;
    int hovered_idx_{-1};

    void rebuild_items();
    void apply_shape();
};

} // namespace SzpontDesktop
