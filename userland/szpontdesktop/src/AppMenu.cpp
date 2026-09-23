#include "AppMenu.hpp"
#include "IconLoader.hpp"
#include <cctype>
#include <algorithm>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

namespace SzpontDesktop {

using namespace SzpontUI;

AppMenuWindow::AppMenuWindow(const std::vector<XdgApp> &apps, const DesktopConfig &config)
    : Window(Size{360, 460}, "SzpontAppMenu"), all_apps_(apps), config_(config) {
    set_override_redirect(true);
    set_position(Point{10, 38});
    filtered_apps_ = all_apps_;
}

void AppMenuWindow::toggle() {
    if (is_open_) {
        hide();
        is_open_ = false;
    } else {
        search_query_.clear();
        update_filter();
        show();
        is_open_ = true;
        if (auto app = Application::instance()) {
            if (backend_handle()) {
                auto data = static_cast<X11WindowData*>(backend_handle());
                auto &b = dynamic_cast<X11Backend&>(app->backend());
                Display *d = b.display();
                if (d && data && data->x_win) {
                    XRaiseWindow(d, data->x_win);
                    XSetInputFocus(d, data->x_win, RevertToPointerRoot, CurrentTime);
                    XFlush(d);
                }
            }
        }
    }
}

void AppMenuWindow::update_filter() {
    filtered_apps_.clear();
    if (search_query_.empty()) {
        filtered_apps_ = all_apps_;
    } else {
        std::string q = search_query_;
        std::transform(q.begin(), q.end(), q.begin(), ::tolower);
        for (const auto &app : all_apps_) {
            std::string n = app.name;
            std::string c = app.comment;
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            std::transform(c.begin(), c.end(), c.begin(), ::tolower);
            if (n.find(q) != std::string::npos || c.find(q) != std::string::npos ||
                app.id.find(q) != std::string::npos) {
                filtered_apps_.push_back(app);
            }
        }
    }
    hovered_idx_ = -1;
    update(Rect{0, 0, bounds_.width, bounds_.height});
}

void AppMenuWindow::on_paint(Painter &painter) {
    int w = bounds_.width;
    int h = bounds_.height;

    // 1. Subdued Obsidian Glass Card
    painter.fill_rounded_rect(Rect{0, 0, w, h}, 14, Color(20, 24, 34));
    painter.draw_rounded_rect(Rect{0, 0, w, h}, 14, Color(255, 255, 255, 30), 1);
    painter.draw_rounded_rect(Rect{1, 1, w - 2, h - 2}, 13, Color(255, 255, 255, 12), 1);

    // 2. Search Box
    search_rect_ = Rect{14, 14, w - 28, 36};
    painter.fill_rounded_rect(search_rect_, 8, Color(14, 17, 24));
    painter.draw_rounded_rect(search_rect_, 8, Color(255, 255, 255, 22), 1);

    std::string search_display = search_query_.empty() ? "Search applications..." : (search_query_ + "|");
    Color search_color = search_query_.empty() ? Color(148, 163, 184) : Color(248, 250, 252);
    painter.draw_text(Rect{search_rect_.x + 12, search_rect_.y, search_rect_.width - 24, search_rect_.height},
                      search_display, SzpontUI::Font("Inter", 11, FontWeight::Regular), search_color,
                      TextAlignment::Left, VerticalAlignment::Center);

    // 3. Application Rows
    app_row_rects_.clear();
    int row_y = 58;
    int row_h = 44;
    int max_rows = 7;
    int n = std::min((int)filtered_apps_.size(), max_rows);

    for (int i = 0; i < n; ++i) {
        const XdgApp &app = filtered_apps_[i];
        Rect r{10, row_y, w - 20, row_h};
        app_row_rects_.push_back({&app, r});

        bool is_hover = (i == hovered_idx_);
        if (is_hover) {
            painter.fill_rounded_rect(r, 8, Color(255, 255, 255, 20));
            painter.draw_rounded_rect(r, 8, Color(255, 255, 255, 32), 1);
        }

        // Real 32x32 App Icon
        const BitmapSurface *icon_bmp = IconLoader::instance().get_icon(app.icon, app.id, app.name, 32);
        if (icon_bmp) {
            painter.draw_bitmap(Point{r.x + 8, r.y + 6}, *icon_bmp);
        }

        // App Name
        painter.draw_text(Rect{r.x + 48, r.y + 5, r.width - 52, 18}, app.name,
                          SzpontUI::Font("Inter", 12, FontWeight::Medium), Color(248, 250, 252),
                          TextAlignment::Left, VerticalAlignment::Center);

        // App Comment or Generic Name
        std::string subtitle = app.comment.empty() ? (app.generic_name.empty() ? app.exec : app.generic_name) : app.comment;
        if (subtitle.length() > 36) subtitle = subtitle.substr(0, 34) + "..";
        painter.draw_text(Rect{r.x + 48, r.y + 23, r.width - 52, 16}, subtitle,
                          SzpontUI::Font("Inter", 10, FontWeight::Regular), Color(148, 163, 184),
                          TextAlignment::Left, VerticalAlignment::Center);

        row_y += row_h + 2;
    }

    if (filtered_apps_.empty()) {
        painter.draw_text(Rect{14, 130, w - 28, 40}, "No applications found.",
                          SzpontUI::Font("Inter", 12, FontWeight::Medium), Color(148, 163, 184),
                          TextAlignment::Center, VerticalAlignment::Center);
    }

    // 4. Bottom Footer with Power Actions
    painter.draw_line(Point{14, h - 46}, Point{w - 14, h - 46}, Color(255, 255, 255, 18), 1);

    // Current user info
    const char *u = getenv("USER");
    if (!u || !*u) u = "szpont";
    painter.draw_text(Rect{16, h - 40, 100, 32}, std::string("u: ") + u,
                      SzpontUI::Font("Inter", 11, FontWeight::Regular), Color(148, 163, 184),
                      TextAlignment::Left, VerticalAlignment::Center);

    // Action buttons: Logout, Reboot, Power Off
    logout_btn_rect_ = Rect{w - 196, h - 38, 58, 26};
    painter.fill_rounded_rect(logout_btn_rect_, 6, Color(255, 255, 255, 14));
    painter.draw_rounded_rect(logout_btn_rect_, 6, Color(255, 255, 255, 24), 1);
    painter.draw_text(logout_btn_rect_, "Log Out",
                      SzpontUI::Font("Inter", 10, FontWeight::Regular), Color(226, 232, 240),
                      TextAlignment::Center, VerticalAlignment::Center);

    reboot_btn_rect_ = Rect{w - 132, h - 38, 58, 26};
    painter.fill_rounded_rect(reboot_btn_rect_, 6, Color(255, 255, 255, 14));
    painter.draw_rounded_rect(reboot_btn_rect_, 6, Color(255, 255, 255, 24), 1);
    painter.draw_text(reboot_btn_rect_, "Restart",
                      SzpontUI::Font("Inter", 10, FontWeight::Regular), Color(226, 232, 240),
                      TextAlignment::Center, VerticalAlignment::Center);

    power_btn_rect_ = Rect{w - 68, h - 38, 56, 26};
    painter.fill_rounded_rect(power_btn_rect_, 6, Color(239, 68, 68, 30));
    painter.draw_rounded_rect(power_btn_rect_, 6, Color(239, 68, 68, 60), 1);
    painter.draw_text(power_btn_rect_, "Power Off",
                      SzpontUI::Font("Inter", 10, FontWeight::Medium), Color(252, 165, 165),
                      TextAlignment::Center, VerticalAlignment::Center);
}

void AppMenuWindow::on_mouse_down(MouseEvent &event) {
    Point p = event.pos();

    if (logout_btn_rect_.contains(p)) {
        if (on_logout_clicked) on_logout_clicked();
        toggle();
        return;
    }
    if (reboot_btn_rect_.contains(p)) {
        if (on_reboot_clicked) on_reboot_clicked();
        toggle();
        return;
    }
    if (power_btn_rect_.contains(p)) {
        if (on_poweroff_clicked) on_poweroff_clicked();
        toggle();
        return;
    }

    for (const auto &item : app_row_rects_) {
        if (item.rect.contains(p)) {
            if (on_launch_app) on_launch_app(*item.app);
            toggle();
            return;
        }
    }
}

void AppMenuWindow::on_mouse_move(MouseEvent &event) {
    Point p = event.pos();
    int new_hover = -1;
    for (size_t i = 0; i < app_row_rects_.size(); ++i) {
        if (app_row_rects_[i].rect.contains(p)) {
            new_hover = (int)i;
            break;
        }
    }
    if (new_hover != hovered_idx_) {
        hovered_idx_ = new_hover;
        update(Rect{0, 0, bounds_.width, bounds_.height});
    }
}

void AppMenuWindow::on_key_down(KeyEvent &event) {
    uint32_t sym = event.keysym();

    if (sym == XK_Escape) {
        toggle();
        return;
    }

    if (sym == XK_Return) {
        if (!filtered_apps_.empty()) {
            int idx = (hovered_idx_ >= 0 && hovered_idx_ < (int)filtered_apps_.size()) ? hovered_idx_ : 0;
            if (on_launch_app) on_launch_app(filtered_apps_[idx]);
            toggle();
            return;
        }
    }

    if (sym == XK_BackSpace) {
        if (!search_query_.empty()) {
            search_query_.pop_back();
            update_filter();
        }
        return;
    }

    if (sym == XK_Down) {
        if (!filtered_apps_.empty()) {
            hovered_idx_ = (hovered_idx_ + 1) % (int)filtered_apps_.size();
            update(Rect{0, 0, bounds_.width, bounds_.height});
        }
        return;
    }

    if (sym == XK_Up) {
        if (!filtered_apps_.empty()) {
            hovered_idx_ = (hovered_idx_ <= 0) ? ((int)filtered_apps_.size() - 1) : (hovered_idx_ - 1);
            update(Rect{0, 0, bounds_.width, bounds_.height});
        }
        return;
    }

    // Append typed printable characters
    if (sym >= 32 && sym <= 126) {
        search_query_ += (char)sym;
        update_filter();
        return;
    }
}

} // namespace SzpontDesktop
