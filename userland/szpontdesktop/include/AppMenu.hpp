#pragma once

#include <SzpontUI/SzpontUI.hpp>
#include <vector>
#include <string>
#include <functional>
#include "XdgApp.hpp"
#include "Config.hpp"

namespace SzpontDesktop {

class AppMenuWindow : public SzpontUI::Window {
public:
    AppMenuWindow(const std::vector<XdgApp> &apps, const DesktopConfig &config);

    void toggle();
    bool is_open() const { return is_open_; }

    std::function<void(const XdgApp &app)> on_launch_app;
    std::function<void()> on_reboot_clicked;
    std::function<void()> on_poweroff_clicked;
    std::function<void()> on_logout_clicked;

protected:
    void on_paint(SzpontUI::Painter &painter) override;
    void on_mouse_down(SzpontUI::MouseEvent &event) override;
    void on_mouse_move(SzpontUI::MouseEvent &event) override;
    void on_key_down(SzpontUI::KeyEvent &event) override;

private:
    struct AppRowRect {
        const XdgApp *app{nullptr};
        SzpontUI::Rect rect;
    };

    std::vector<XdgApp> all_apps_;
    std::vector<XdgApp> filtered_apps_;
    DesktopConfig config_;
    bool is_open_{false};
    std::string search_query_;
    int hovered_idx_{-1};

    SzpontUI::Rect search_rect_;
    SzpontUI::Rect logout_btn_rect_;
    SzpontUI::Rect reboot_btn_rect_;
    SzpontUI::Rect power_btn_rect_;
    std::vector<AppRowRect> app_row_rects_;

    void update_filter();
};

} // namespace SzpontDesktop
