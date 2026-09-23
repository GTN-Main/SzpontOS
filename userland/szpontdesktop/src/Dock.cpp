#include "Dock.hpp"
#include "Wallpaper.hpp"
#include "IconLoader.hpp"
#include <algorithm>
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>

namespace SzpontDesktop {

using namespace SzpontUI;

DockWindow::DockWindow(int screen_w, int screen_h, const std::vector<XdgApp> &apps,
                       WindowManager &wm, const DesktopConfig &config)
    : Window(Size{420, 76}, "SzpontDock"), screen_w_(screen_w), screen_h_(screen_h),
      all_apps_(apps), wm_(wm), config_(config) {
    set_override_redirect(true);

    rebuild_items();

    wm_.on_windows_changed = [this]() {
        refresh();
    };
    wm_.on_focus_changed = [this](ClientWindow *) {
        refresh();
    };
}

void DockWindow::rebuild_items() {
    items_.clear();

    const auto &clients = wm_.clients();
    ClientWindow *focused = wm_.focused_client();

    std::vector<ClientWindow*> matched_clients;

    // 1. Add pinned applications
    for (const auto &pinned_id : config_.dock_pinned) {
        const XdgApp *app = XdgRegistry::find_by_id(all_apps_, pinned_id);
        DockItem item;
        item.id = pinned_id;
        item.name = app ? app->name : pinned_id;
        item.icon = app ? app->icon : pinned_id;
        item.app = app;

        // Check if running
        for (const auto &cw : clients) {
            std::string t = cw->title;
            std::transform(t.begin(), t.end(), t.begin(), ::tolower);
            std::string pid = pinned_id;
            std::transform(pid.begin(), pid.end(), pid.begin(), ::tolower);

            if (cw->app_id == pinned_id || t.find(pid) != std::string::npos ||
                (app && !app->name.empty() && t.find(app->name) != std::string::npos)) {
                item.client = cw.get();
                item.is_running = true;
                item.is_active = (cw.get() == focused && !cw->is_minimized);
                item.is_minimized = cw->is_minimized;
                matched_clients.push_back(cw.get());
                break;
            }
        }
        items_.push_back(item);
    }

    // 2. Add unpinned running client windows
    for (const auto &cw : clients) {
        bool already_matched = false;
        for (auto *mc : matched_clients) {
            if (mc == cw.get()) {
                already_matched = true;
                break;
            }
        }
        if (!already_matched) {
            DockItem item;
            item.id = cw->app_id.empty() ? cw->title : cw->app_id;
            item.name = cw->title;
            item.icon = cw->app_id;
            item.client = cw.get();
            item.is_running = true;
            item.is_active = (cw.get() == focused && !cw->is_minimized);
            item.is_minimized = cw->is_minimized;

            // Try to find matching XDG app
            for (const auto &app : all_apps_) {
                std::string t = cw->title;
                std::transform(t.begin(), t.end(), t.begin(), ::tolower);
                std::string aname = app.name;
                std::transform(aname.begin(), aname.end(), aname.begin(), ::tolower);
                if (t.find(aname) != std::string::npos || app.id == cw->app_id) {
                    item.app = &app;
                    item.icon = app.icon;
                    item.name = app.name;
                    break;
                }
            }
            items_.push_back(item);
        }
    }

    // macOS Style Dynamic width calculation
    int item_size = 48;
    int spacing = 12;
    int padding_x = 16;
    int dock_w = (int)items_.size() * (item_size + spacing) - spacing + 2 * padding_x;
    if (dock_w < 160) dock_w = 160;
    int dock_h = 76;
    int dock_x = (screen_w_ - dock_w) / 2;
    int dock_y = screen_h_ - dock_h - 12;

    if (bounds_.width != dock_w || bounds_.height != dock_h || bounds_.x != dock_x || bounds_.y != dock_y) {
        resize(Size{dock_w, dock_h});
        set_position(Point{dock_x, dock_y});
        apply_shape();
    }
}

void DockWindow::apply_shape() {
    auto app = Application::instance();
    if (!app || !backend_handle()) return;
    auto data = static_cast<X11WindowData*>(backend_handle());
    if (!data || !data->x_win) return;
    auto &b = dynamic_cast<X11Backend&>(app->backend());
    Display *dpy = b.display();
    if (!dpy) return;

    int w = bounds_.width;
    int h = bounds_.height;
    if (w <= 0 || h <= 0) return;

    Pixmap mask = XCreatePixmap(dpy, data->x_win, (unsigned int)w, (unsigned int)h, 1);
    if (mask == None) return;

    GC gc = XCreateGC(dpy, mask, 0, nullptr);
    XSetForeground(dpy, gc, 0);
    XFillRectangle(dpy, mask, gc, 0, 0, (unsigned int)w, (unsigned int)h);
    XSetForeground(dpy, gc, 1);

    int r = 22;
    XFillRectangle(dpy, mask, gc, r, 0, (unsigned int)(w - 2 * r), (unsigned int)h);
    XFillRectangle(dpy, mask, gc, 0, r, (unsigned int)w, (unsigned int)(h - 2 * r));
    XFillArc(dpy, mask, gc, 0, 0, (unsigned int)(2 * r), (unsigned int)(2 * r), 90 * 64, 90 * 64);
    XFillArc(dpy, mask, gc, w - 2 * r, 0, (unsigned int)(2 * r), (unsigned int)(2 * r), 0, 90 * 64);
    XFillArc(dpy, mask, gc, 0, h - 2 * r, (unsigned int)(2 * r), (unsigned int)(2 * r), 180 * 64, 90 * 64);
    XFillArc(dpy, mask, gc, w - 2 * r, h - 2 * r, (unsigned int)(2 * r), (unsigned int)(2 * r), 270 * 64, 90 * 64);

    XShapeCombineMask(dpy, data->x_win, ShapeBounding, 0, 0, mask, ShapeSet);
    XFreeGC(dpy, gc);
    XFreePixmap(dpy, mask);
}

void DockWindow::refresh() {
    rebuild_items();
    update(Rect{0, 0, bounds_.width, bounds_.height});
}

void DockWindow::on_paint(Painter &painter) {
    int w = bounds_.width;
    int h = bounds_.height;
    int dock_x = bounds_.x;
    int dock_y = bounds_.y;

    // 1. Wallpaper-backed Frosted Glass Translucency
    const BitmapSurface *wp = WallpaperManager::wallpaper_surface();
    if (wp && wp->width() >= (dock_x + w) && wp->height() >= (dock_y + h)) {
        painter.draw_bitmap(Point{0, 0}, *wp, Rect{dock_x, dock_y, w, h});
        // Translucent dark obsidian frosted glass overlay
        painter.fill_rounded_rect(Rect{0, 0, w, h}, 22, Color(22, 26, 36, 195));
    } else {
        painter.fill_rounded_rect(Rect{0, 0, w, h}, 22, Color(22, 26, 36));
    }

    // 1px outer glass highlight & 1px inner sheen
    painter.draw_rounded_rect(Rect{0, 0, w, h}, 22, Color(255, 255, 255, 34), 1);
    painter.draw_rounded_rect(Rect{1, 1, w - 2, h - 2}, 21, Color(255, 255, 255, 12), 1);

    hit_rects_.clear();
    int item_size = 48;
    int spacing = 12;
    int padding_x = 16;
    int start_x = padding_x;
    int icon_y = 12;

    for (size_t i = 0; i < items_.size(); ++i) {
        const DockItem &item = items_[i];
        int ix = start_x + (int)i * (item_size + spacing);
        Rect r{ix, icon_y, item_size, item_size};
        hit_rects_.push_back({item, Rect{ix - 2, 4, item_size + 4, h - 8}});

        bool is_hover = ((int)i == hovered_idx_);

        if (is_hover) {
            // Subtle card highlight on hover
            painter.fill_rounded_rect(Rect{ix - 2, icon_y - 2, item_size + 4, item_size + 4}, 12, Color(255, 255, 255, 25));
        }

        // Load & render 48x48 icon
        const BitmapSurface *icon_bmp = IconLoader::instance().get_icon(item.icon, item.id, item.name, 48);
        if (icon_bmp) {
            painter.draw_bitmap(Point{ix, icon_y}, *icon_bmp);
        }

        // Running indicator dot beneath icon
        if (item.is_running) {
            if (item.is_active) {
                // Bright white active dot
                painter.fill_rounded_rect(Rect{ix + item_size / 2 - 4, h - 8, 8, 3}, 1, Color(255, 255, 255));
            } else if (item.is_minimized) {
                // Dimmed slate dot
                painter.fill_rounded_rect(Rect{ix + item_size / 2 - 2, h - 8, 4, 3}, 1, Color(148, 163, 184, 180));
            } else {
                // Subtle white running dot
                painter.fill_rounded_rect(Rect{ix + item_size / 2 - 2, h - 8, 4, 3}, 1, Color(226, 232, 240, 220));
            }
        }
    }

    // 3. Floating Tooltip Tag on Hover
    if (hovered_idx_ >= 0 && hovered_idx_ < (int)items_.size()) {
        const DockItem &h_item = items_[hovered_idx_];
        int ix = start_x + hovered_idx_ * (item_size + spacing);

        std::string tip = h_item.name;
        if (tip.length() > 24) tip = tip.substr(0, 22) + "..";

        Size text_sz = painter.measure_text(tip, SzpontUI::Font("Inter", 10, FontWeight::Medium));
        int tip_w = text_sz.width + 12;
        int tip_h = 16;
        int tip_x = ix + (item_size - tip_w) / 2;
        if (tip_x < 4) tip_x = 4;
        if (tip_x + tip_w > w - 4) tip_x = w - 4 - tip_w;
        int tip_y = 2;

        Rect tip_rect{tip_x, tip_y, tip_w, tip_h};
        painter.fill_rounded_rect(tip_rect, 4, Color(15, 20, 28, 230));
        painter.draw_rounded_rect(tip_rect, 4, Color(255, 255, 255, 30), 1);
        painter.draw_text(tip_rect, tip, SzpontUI::Font("Inter", 10, FontWeight::Medium),
                          Color(248, 250, 252), TextAlignment::Center, VerticalAlignment::Center);
    }
}

void DockWindow::on_mouse_down(MouseEvent &event) {
    Point p = event.pos();

    for (const auto &hit : hit_rects_) {
        if (hit.rect.contains(p)) {
            const DockItem &item = hit.item;
            if (item.client) {
                ClientWindow *cw = item.client;
                if (cw->is_minimized) {
                    wm_.restore_window(cw);
                } else if (cw == wm_.focused_client()) {
                    wm_.minimize_window(cw);
                } else {
                    wm_.set_focus(cw);
                }
            } else if (item.app) {
                if (on_launch_app) on_launch_app(*item.app);
            }
            refresh();
            return;
        }
    }
}

void DockWindow::on_mouse_move(MouseEvent &event) {
    Point p = event.pos();
    int new_hover = -1;
    for (size_t i = 0; i < hit_rects_.size(); ++i) {
        if (hit_rects_[i].rect.contains(p)) {
            new_hover = (int)i;
            break;
        }
    }
    if (new_hover != hovered_idx_) {
        hovered_idx_ = new_hover;
        update(Rect{0, 0, bounds_.width, bounds_.height});
    }
}

} // namespace SzpontDesktop
