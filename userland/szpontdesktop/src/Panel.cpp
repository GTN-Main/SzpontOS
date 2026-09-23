#include "Panel.hpp"
#include "Wallpaper.hpp"
#include <ctime>
#include <unistd.h>

namespace SzpontDesktop {

using namespace SzpontUI;

static std::string get_english_time_str() {
    time_t now = time(nullptr);
    struct tm *tm_info = localtime(&now);
    if (!tm_info) tm_info = gmtime(&now);
    if (!tm_info) return "12:00:00 UTC";

    static const char *s_days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char *s_months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    char buf[64];
    snprintf(buf, sizeof(buf), "%s %s %02d  %02d:%02d:%02d",
             s_days[tm_info->tm_wday % 7],
             s_months[tm_info->tm_mon % 12],
             tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    return std::string(buf);
}

PanelWindow::PanelWindow(int width, int height, WindowManager &wm, const DesktopConfig &config)
    : Window(Size{width, height}, "SzpontPanel"), wm_(wm), config_(config) {
    set_override_redirect(true);
    set_position(Point{0, 0});

    const char *u = getenv("USER");
    if (!u || !*u) u = "szpont";
    user_str_ = u;
    current_time_str_ = get_english_time_str();

    wm_.on_windows_changed = [this]() {
        refresh_windows();
    };
    wm_.on_focus_changed = [this](ClientWindow *) {
        refresh_windows();
    };
}

void PanelWindow::update_time() {
    current_time_str_ = get_english_time_str();
    int clock_w = 175;
    int clock_x = bounds_.width - 52 - clock_w;
    update(Rect{clock_x, 0, clock_w + 10, bounds_.height});
}

void PanelWindow::refresh_windows() {
    update(Rect{0, 0, bounds_.width, bounds_.height});
}

void PanelWindow::on_paint(Painter &painter) {
    int w = bounds_.width;
    int h = bounds_.height;

    // 1. Wallpaper-backed Frosted Glass Translucency
    const BitmapSurface *wp = WallpaperManager::wallpaper_surface();
    if (wp && wp->width() >= w && wp->height() >= h) {
        painter.draw_bitmap(Point{0, 0}, *wp, Rect{0, 0, w, h});
        // Translucent dark obsidian glass tint
        painter.fill_rect(Rect{0, 0, w, h}, Color(18, 22, 30, 205));
    } else {
        painter.fill_rect(Rect{0, 0, w, h}, Color(18, 22, 30));
    }

    // 1px subtle glass boundary highlight at bottom
    painter.draw_line(Point{0, h - 1}, Point{w, h - 1}, Color(255, 255, 255, 22), 1);

    int pill_y = 4;
    int pill_h = h - 8;

    // 2. Apple / macOS style Menu Button (Left)
    start_btn_rect_ = Rect{10, pill_y, 82, pill_h};
    painter.fill_rounded_rect(start_btn_rect_, 6, Color(255, 255, 255, 18));
    painter.draw_rounded_rect(start_btn_rect_, 6, Color(255, 255, 255, 30), 1);

    painter.draw_text(start_btn_rect_, "SZPONT",
                      SzpontUI::Font("Inter", 12, FontWeight::Bold), Color(248, 250, 252),
                      TextAlignment::Center, VerticalAlignment::Center);

    // 3. System Tray & Status Area (Right Side)
    // Exit button
    exit_btn_rect_ = Rect{w - 46, pill_y, 36, pill_h};
    painter.fill_rounded_rect(exit_btn_rect_, 6, Color(239, 68, 68, 35));
    painter.draw_rounded_rect(exit_btn_rect_, 6, Color(239, 68, 68, 70), 1);
    painter.draw_text(exit_btn_rect_, "Exit",
                      SzpontUI::Font("Inter", 11, FontWeight::Bold), Color(252, 165, 165),
                      TextAlignment::Center, VerticalAlignment::Center);

    // Clock
    int clock_w = 175;
    int clock_x = exit_btn_rect_.x - 8 - clock_w;
    Rect clock_rect{clock_x, pill_y, clock_w, pill_h};
    painter.fill_rounded_rect(clock_rect, 6, Color(255, 255, 255, 14));
    painter.draw_rounded_rect(clock_rect, 6, Color(255, 255, 255, 22), 1);
    painter.draw_text(clock_rect, current_time_str_,
                      SzpontUI::Font("Inter", 11, FontWeight::Medium), Color(241, 245, 249),
                      TextAlignment::Center, VerticalAlignment::Center);

    // User badge
    int user_w = 74;
    int user_x = clock_x - 8 - user_w;
    Rect user_rect{user_x, pill_y, user_w, pill_h};
    painter.fill_rounded_rect(user_rect, 6, Color(255, 255, 255, 14));
    painter.draw_rounded_rect(user_rect, 6, Color(255, 255, 255, 22), 1);
    painter.draw_text(user_rect, user_str_,
                      SzpontUI::Font("Inter", 11, FontWeight::Regular), Color(203, 213, 225),
                      TextAlignment::Center, VerticalAlignment::Center);

    // 4. Subdued Dynamic Window Taskbar Tabs (Center Area)
    task_rects_.clear();
    const auto &clients = wm_.clients();
    int task_start_x = 102;
    int task_end_x = user_x - 12;
    int avail_w = task_end_x - task_start_x;

    if (!clients.empty() && avail_w > 60) {
        int max_item_w = 150;
        int gap = 6;
        int n = (int)clients.size();
        int item_w = max_item_w;
        if (n * (max_item_w + gap) > avail_w) {
            item_w = (avail_w / n) - gap;
            if (item_w < 60) item_w = 60;
        }

        for (int i = 0; i < n; ++i) {
            ClientWindow *cw = clients[i].get();
            int tx = task_start_x + i * (item_w + gap);
            if (tx + item_w > task_end_x) break;

            Rect item_rect{tx, pill_y, item_w, pill_h};
            task_rects_.push_back({cw, item_rect});

            bool is_active = (cw == wm_.focused_client() && !cw->is_minimized);
            bool is_min = cw->is_minimized;

            Color bg_col = is_active ? Color(255, 255, 255, 36) :
                           (is_min ? Color(255, 255, 255, 8) : Color(255, 255, 255, 16));
            Color border_col = is_active ? Color(255, 255, 255, 60) : Color(255, 255, 255, 24);
            Color text_col = is_active ? Color(255, 255, 255) :
                             (is_min ? Color(148, 163, 184) : Color(226, 232, 240));

            painter.fill_rounded_rect(item_rect, 6, bg_col);
            painter.draw_rounded_rect(item_rect, 6, border_col, 1);

            if (is_active) {
                // Subtle 4px active indicator dot
                painter.fill_rounded_rect(Rect{tx + 6, pill_y + pill_h / 2 - 2, 4, 4}, 2, Color(255, 255, 255));
            }

            std::string disp_title = is_min ? ("[-] " + cw->title) : cw->title;
            int text_offset = is_active ? 16 : 8;
            int max_chars = (item_w - text_offset - 8) / 7;
            if (max_chars < 3) max_chars = 3;
            if ((int)disp_title.length() > max_chars) {
                disp_title = disp_title.substr(0, max_chars - 2) + "..";
            }

            painter.draw_text(Rect{tx + text_offset, pill_y, item_w - text_offset - 4, pill_h}, disp_title,
                              SzpontUI::Font("Inter", 11, is_active ? FontWeight::Medium : FontWeight::Regular),
                              text_col, TextAlignment::Left, VerticalAlignment::Center);
        }
    }
}

void PanelWindow::on_mouse_down(MouseEvent &event) {
    Point p = event.pos();

    if (start_btn_rect_.contains(p)) {
        if (on_start_clicked) on_start_clicked();
        return;
    }

    if (exit_btn_rect_.contains(p)) {
        if (on_exit_clicked) on_exit_clicked();
        return;
    }

    // Window tabs click
    for (const auto &item : task_rects_) {
        if (item.rect.contains(p)) {
            ClientWindow *cw = item.client;
            if (cw->is_minimized) {
                wm_.restore_window(cw);
            } else if (cw == wm_.focused_client()) {
                wm_.minimize_window(cw);
            } else {
                wm_.set_focus(cw);
            }
            refresh_windows();
            return;
        }
    }
}

} // namespace SzpontDesktop
