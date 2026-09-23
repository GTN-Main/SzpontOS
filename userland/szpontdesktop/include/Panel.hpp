#pragma once

#include <SzpontUI/SzpontUI.hpp>
#include <memory>
#include <functional>
#include <vector>
#include <string>
#include "WindowManager.hpp"
#include "Config.hpp"

namespace SzpontDesktop {

class PanelWindow : public SzpontUI::Window {
public:
    PanelWindow(int width, int height, WindowManager &wm, const DesktopConfig &config);

    void update_time();
    void refresh_windows();

    std::function<void()> on_start_clicked;
    std::function<void()> on_exit_clicked;

protected:
    void on_paint(SzpontUI::Painter &painter) override;
    void on_mouse_down(SzpontUI::MouseEvent &event) override;

private:
    struct TaskItemRect {
        ClientWindow *client{nullptr};
        SzpontUI::Rect rect;
    };

    WindowManager &wm_;
    DesktopConfig config_;
    std::string current_time_str_;
    std::string user_str_;
    std::vector<TaskItemRect> task_rects_;

    SzpontUI::Rect start_btn_rect_;
    SzpontUI::Rect exit_btn_rect_;
};

} // namespace SzpontDesktop
