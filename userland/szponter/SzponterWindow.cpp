#include "SzponterWindow.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <algorithm>
#include <cctype>
#include <X11/keysym.h>

namespace Szponter {

using namespace SzpontUI;

static uint64_t get_current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static void launch_command(const std::string &cmd, const std::string &arg = "") {
    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        if (arg.empty()) {
            char *const argv[] = {(char*)cmd.c_str(), nullptr};
            execvp(cmd.c_str(), argv);
        } else {
            char *const argv[] = {(char*)cmd.c_str(), (char*)arg.c_str(), nullptr};
            execvp(cmd.c_str(), argv);
        }
        _exit(127);
    }
}

SzponterWindow::SzponterWindow(int width, int height, const std::string &initial_path)
    : Window(Size{width, height}, "Szponter - File Manager") {

    sidebar_items_ = {
        {"Home", "/home/szpont", "~"},
        {"System Root", "/", "/"},
        {"Applications", "/usr/share/applications", "A"},
        {"Pictures", "/usr/share/artwork", "P"},
        {"Settings", "/etc", "E"},
        {"Binaries", "/bin", "B"}
    };

    std::string start = initial_path;
    if (access(start.c_str(), R_OK) != 0) {
        start = "/";
    }
    navigate_to(start);
}

void SzponterWindow::navigate_to(const std::string &path) {
    std::string clean = path;
    if (clean.empty()) clean = "/";
    // Collapse duplicate slashes
    std::string normalized;
    for (size_t i = 0; i < clean.length(); ++i) {
        if (clean[i] == '/' && !normalized.empty() && normalized.back() == '/') continue;
        normalized += clean[i];
    }
    if (normalized.length() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }

    // Add to history
    if (history_index_ < 0 || history_[history_index_] != normalized) {
        history_.erase(history_.begin() + (history_index_ + 1), history_.end());
        history_.push_back(normalized);
        history_index_ = (int)history_.size() - 1;
    }

    current_path_ = normalized;
    scroll_offset_ = 0;
    selected_item_ = -1;
    hovered_item_ = -1;
    load_directory(current_path_);

    // Check matching sidebar
    selected_sidebar_ = -1;
    for (size_t i = 0; i < sidebar_items_.size(); ++i) {
        if (sidebar_items_[i].path == current_path_) {
            selected_sidebar_ = (int)i;
            break;
        }
    }

    set_title("Szponter - " + current_path_);
    update(Rect{0, 0, bounds_.width, bounds_.height});
}

FileType SzponterWindow::detect_file_type(const std::string &name, mode_t mode) {
    if (S_ISDIR(mode)) return FileType::Directory;

    std::string ext;
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) {
        ext = name.substr(dot);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") return FileType::Image;
    if (ext == ".desktop") return FileType::DesktopApp;
    if (ext == ".txt" || ext == ".c" || ext == ".h" || ext == ".cpp" || ext == ".conf" || ext == ".sh") return FileType::Text;
    if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) return FileType::Executable;

    return FileType::Other;
}

std::string SzponterWindow::format_size(size_t bytes) const {
    char buf[32];
    if (bytes < 1024) {
        snprintf(buf, sizeof(buf), "%zu B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buf, sizeof(buf), "%.1f KB", (double)bytes / 1024.0);
    } else {
        snprintf(buf, sizeof(buf), "%.1f MB", (double)bytes / (1024.0 * 1024.0));
    }
    return std::string(buf);
}

void SzponterWindow::load_directory(const std::string &path) {
    all_items_.clear();

    DIR *dir = opendir(path.c_str());
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        std::string full = path;
        if (full.back() != '/') full += '/';
        full += ent->d_name;

        struct stat st;
        if (stat(full.c_str(), &st) != 0) {
            memset(&st, 0, sizeof(st));
        }

        FileItem item;
        item.name = ent->d_name;
        item.full_path = full;
        item.mode = st.st_mode;
        item.size = (size_t)st.st_size;
        item.is_dir = S_ISDIR(st.st_mode);
        item.type = detect_file_type(item.name, st.st_mode);

        all_items_.push_back(item);
    }
    closedir(dir);

    // Sort: directories first, then alphabetical
    std::sort(all_items_.begin(), all_items_.end(), [](const FileItem &a, const FileItem &b) {
        if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
        return a.name < b.name;
    });

    update_filter();
}

void SzponterWindow::update_filter() {
    filtered_items_.clear();
    if (search_query_.empty()) {
        filtered_items_ = all_items_;
    } else {
        std::string q = search_query_;
        std::transform(q.begin(), q.end(), q.begin(), ::tolower);
        for (const auto &it : all_items_) {
            std::string n = it.name;
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            if (n.find(q) != std::string::npos) {
                filtered_items_.push_back(it);
            }
        }
    }
}

void SzponterWindow::open_item(const FileItem &item) {
    if (item.is_dir) {
        navigate_to(item.full_path);
        return;
    }

    if (item.type == FileType::Image) {
        launch_command("/usr/bin/szpontview", item.full_path);
        return;
    }

    if (item.type == FileType::DesktopApp) {
        // Read Exec= from desktop file
        FILE *f = fopen(item.full_path.c_str(), "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "Exec=", 5) == 0) {
                    char *p = line + 5;
                    while (*p && (*p == ' ' || *p == '\t')) p++;
                    char *end = p + strlen(p) - 1;
                    while (end > p && (*end == '\n' || *end == '\r' || *end == ' ')) *end-- = '\0';
                    launch_command(p);
                    break;
                }
            }
            fclose(f);
        }
        return;
    }

    if (item.type == FileType::Text) {
        launch_command("/usr/bin/szponterm", "-e /bin/nano " + item.full_path);
        return;
    }

    if (item.type == FileType::Executable) {
        launch_command(item.full_path);
        return;
    }
}

void SzponterWindow::on_paint(Painter &painter) {
    int w = bounds_.width;
    int h = bounds_.height;

    // Layout definitions
    sidebar_rect_ = Rect{0, 0, 190, h - 28};
    toolbar_rect_ = Rect{190, 0, w - 190, 46};
    content_rect_ = Rect{190, 46, w - 190, h - 46 - 28};
    statusbar_rect_ = Rect{0, h - 28, w, 28};

    // 1. Sidebar Background (macOS Finder Dark Obsidian)
    painter.fill_rect(sidebar_rect_, Color(20, 24, 32));
    painter.draw_line(Point{sidebar_rect_.right() - 1, 0}, Point{sidebar_rect_.right() - 1, sidebar_rect_.height},
                      Color(36, 42, 54), 1);

    // Sidebar Section Header: FAVORITES
    painter.draw_text(Rect{16, 14, 158, 16}, "FAVORITES",
                      Font("Inter", 10, FontWeight::Bold), Color(148, 163, 184),
                      TextAlignment::Left, VerticalAlignment::Center);

    int sb_y = 38;
    int sb_h = 28;
    for (size_t i = 0; i < sidebar_items_.size(); ++i) {
        Rect r{10, sb_y, 170, sb_h};
        bool is_selected = ((int)i == selected_sidebar_);
        bool is_hover = ((int)i == hovered_sidebar_);

        if (is_selected) {
            painter.fill_rounded_rect(r, 6, Color(59, 130, 246, 50));
            painter.draw_rounded_rect(r, 6, Color(59, 130, 246, 120), 1);
        } else if (is_hover) {
            painter.fill_rounded_rect(r, 6, Color(255, 255, 255, 14));
        }

        // Icon badge
        Rect badge{r.x + 8, r.y + 4, 20, 20};
        painter.fill_rounded_rect(badge, 4, is_selected ? Color(59, 130, 246) : Color(45, 55, 72));
        painter.draw_text(badge, sidebar_items_[i].icon_char,
                          Font("Inter", 10, FontWeight::Bold), Color(248, 250, 252),
                          TextAlignment::Center, VerticalAlignment::Center);

        // Title
        Color text_col = is_selected ? Color(255, 255, 255) : Color(203, 213, 225);
        painter.draw_text(Rect{r.x + 36, r.y, r.width - 40, r.height}, sidebar_items_[i].title,
                          Font("Inter", 11, is_selected ? FontWeight::Medium : FontWeight::Regular),
                          text_col, TextAlignment::Left, VerticalAlignment::Center);

        sb_y += sb_h + 3;
    }

    // 2. Toolbar (Top)
    painter.fill_rect(toolbar_rect_, Color(24, 28, 38));
    painter.draw_line(Point{toolbar_rect_.x, toolbar_rect_.bottom() - 1},
                      Point{toolbar_rect_.right(), toolbar_rect_.bottom() - 1},
                      Color(36, 42, 54), 1);

    // Nav buttons: Back, Forward, Up, Refresh
    btn_back_rect_ = Rect{toolbar_rect_.x + 12, 10, 26, 26};
    btn_fwd_rect_  = Rect{toolbar_rect_.x + 44, 10, 26, 26};
    btn_up_rect_   = Rect{toolbar_rect_.x + 76, 10, 26, 26};
    btn_refresh_rect_ = Rect{toolbar_rect_.x + 108, 10, 26, 26};

    auto draw_btn = [&](const Rect &r, const char *label, bool enabled) {
        painter.fill_rounded_rect(r, 5, enabled ? Color(255, 255, 255, 18) : Color(255, 255, 255, 8));
        painter.draw_rounded_rect(r, 5, enabled ? Color(255, 255, 255, 30) : Color(255, 255, 255, 14), 1);
        painter.draw_text(r, label, Font("Inter", 11, FontWeight::Bold),
                          enabled ? Color(241, 245, 249) : Color(100, 116, 139),
                          TextAlignment::Center, VerticalAlignment::Center);
    };

    draw_btn(btn_back_rect_, "<", history_index_ > 0);
    draw_btn(btn_fwd_rect_, ">", history_index_ < (int)history_.size() - 1);
    draw_btn(btn_up_rect_, "^", current_path_ != "/");
    draw_btn(btn_refresh_rect_, "R", true);

    // View Mode Toggle button
    btn_mode_rect_ = Rect{w - 78, 10, 66, 26};
    painter.fill_rounded_rect(btn_mode_rect_, 5, Color(255, 255, 255, 16));
    painter.draw_rounded_rect(btn_mode_rect_, 5, Color(255, 255, 255, 28), 1);
    painter.draw_text(btn_mode_rect_, list_mode_ ? "List" : "Icons",
                      Font("Inter", 10, FontWeight::Medium), Color(241, 245, 249),
                      TextAlignment::Center, VerticalAlignment::Center);

    // Search Box
    search_box_rect_ = Rect{w - 220, 10, 134, 26};
    painter.fill_rounded_rect(search_box_rect_, 5, Color(16, 19, 26));
    painter.draw_rounded_rect(search_box_rect_, 5, Color(255, 255, 255, 20), 1);
    std::string s_text = search_query_.empty() ? "Search..." : search_query_;
    painter.draw_text(Rect{search_box_rect_.x + 8, search_box_rect_.y, search_box_rect_.width - 16, search_box_rect_.height},
                      s_text, Font("Inter", 10, FontWeight::Regular),
                      search_query_.empty() ? Color(148, 163, 184) : Color(248, 250, 252),
                      TextAlignment::Left, VerticalAlignment::Center);

    // Path Breadcrumb pill
    int path_x = btn_refresh_rect_.right() + 14;
    int path_w = search_box_rect_.x - path_x - 12;
    Rect path_rect{path_x, 10, path_w, 26};
    painter.fill_rounded_rect(path_rect, 5, Color(15, 18, 25));
    painter.draw_rounded_rect(path_rect, 5, Color(255, 255, 255, 20), 1);
    std::string disp_path = current_path_;
    if (disp_path.length() > 38) {
        disp_path = "..." + disp_path.substr(disp_path.length() - 35);
    }
    painter.draw_text(Rect{path_rect.x + 10, path_rect.y, path_rect.width - 20, path_rect.height},
                      disp_path, Font("Inter", 11, FontWeight::Medium), Color(226, 232, 240),
                      TextAlignment::Left, VerticalAlignment::Center);

    // 3. Main Content Area
    painter.fill_rect(content_rect_, Color(15, 18, 24));

    if (filtered_items_.empty()) {
        painter.draw_text(content_rect_, "Folder is empty",
                          Font("Inter", 12, FontWeight::Medium), Color(148, 163, 184),
                          TextAlignment::Center, VerticalAlignment::Center);
    } else if (!list_mode_) {
        // --- GRID VIEW ---
        int item_w = 96;
        int item_h = 96;
        int cols = (content_rect_.width - 24) / item_w;
        if (cols < 1) cols = 1;

        int start_idx = scroll_offset_ * cols;
        int max_visible = (content_rect_.height / item_h + 1) * cols;

        for (int i = start_idx; i < (int)filtered_items_.size() && i < start_idx + max_visible; ++i) {
            const FileItem &it = filtered_items_[i];
            int rel_idx = i - start_idx;
            int col = rel_idx % cols;
            int row = rel_idx / cols;

            int ix = content_rect_.x + 16 + col * item_w;
            int iy = content_rect_.y + 12 + row * item_h;
            if (iy + item_h > content_rect_.bottom() + 10) break;

            Rect item_rect{ix, iy, item_w - 8, item_h - 8};
            bool is_selected = (i == selected_item_);
            bool is_hover = (i == hovered_item_);

            if (is_selected) {
                painter.fill_rounded_rect(item_rect, 8, Color(59, 130, 246, 50));
                painter.draw_rounded_rect(item_rect, 8, Color(59, 130, 246, 120), 1);
            } else if (is_hover) {
                painter.fill_rounded_rect(item_rect, 8, Color(255, 255, 255, 14));
            }

            // File Icon Squircle
            Rect icon_rect{ix + (item_rect.width - 44) / 2, iy + 6, 44, 44};
            Color icon_bg = it.is_dir ? Color(37, 99, 235) :
                            (it.type == FileType::Image ? Color(147, 51, 234) :
                            (it.type == FileType::DesktopApp ? Color(16, 185, 129) :
                            (it.type == FileType::Executable ? Color(245, 158, 11) : Color(71, 85, 105))));
            painter.fill_rounded_rect(icon_rect, 10, icon_bg);
            painter.draw_rounded_rect(icon_rect, 10, Color(255, 255, 255, 50), 1);

            const char *glyph = it.is_dir ? "DIR" :
                                (it.type == FileType::Image ? "IMG" :
                                (it.type == FileType::DesktopApp ? "APP" :
                                (it.type == FileType::Executable ? "BIN" : "TXT")));
            painter.draw_text(icon_rect, glyph, Font("Inter", 11, FontWeight::Bold),
                              Color(255, 255, 255), TextAlignment::Center, VerticalAlignment::Center);

            // File Name Label (truncated)
            std::string disp_name = it.name;
            if (disp_name.length() > 11) {
                disp_name = disp_name.substr(0, 9) + "..";
            }
            Rect name_rect{ix + 2, iy + 54, item_rect.width - 4, 26};
            painter.draw_text(name_rect, disp_name, Font("Inter", 10, FontWeight::Regular),
                              is_selected ? Color(255, 255, 255) : Color(226, 232, 240),
                              TextAlignment::Center, VerticalAlignment::Center);
        }
    } else {
        // --- LIST VIEW ---
        int row_h = 28;
        int max_rows = content_rect_.height / row_h;
        int start_idx = scroll_offset_;

        for (int i = start_idx; i < (int)filtered_items_.size() && i < start_idx + max_rows; ++i) {
            const FileItem &it = filtered_items_[i];
            int iy = content_rect_.y + (i - start_idx) * row_h;
            Rect r{content_rect_.x + 6, iy, content_rect_.width - 12, row_h};

            bool is_selected = (i == selected_item_);
            bool is_hover = (i == hovered_item_);

            if (is_selected) {
                painter.fill_rounded_rect(r, 4, Color(59, 130, 246, 50));
            } else if (is_hover) {
                painter.fill_rounded_rect(r, 4, Color(255, 255, 255, 12));
            }

            // Type badge
            Rect b{r.x + 8, r.y + 5, 32, 18};
            Color badge_bg = it.is_dir ? Color(37, 99, 235) : Color(71, 85, 105);
            painter.fill_rounded_rect(b, 3, badge_bg);
            painter.draw_text(b, it.is_dir ? "DIR" : "FILE", Font("Inter", 8, FontWeight::Bold),
                              Color(255, 255, 255), TextAlignment::Center, VerticalAlignment::Center);

            // Name
            painter.draw_text(Rect{r.x + 48, r.y, 300, r.height}, it.name,
                              Font("Inter", 11, FontWeight::Medium),
                              is_selected ? Color(255, 255, 255) : Color(241, 245, 249),
                              TextAlignment::Left, VerticalAlignment::Center);

            // Size
            std::string s_size = it.is_dir ? "--" : format_size(it.size);
            painter.draw_text(Rect{r.x + 360, r.y, 100, r.height}, s_size,
                              Font("Inter", 10, FontWeight::Regular), Color(148, 163, 184),
                              TextAlignment::Left, VerticalAlignment::Center);
        }
    }

    // 4. Status Bar (Bottom)
    painter.fill_rect(statusbar_rect_, Color(20, 24, 32));
    painter.draw_line(Point{0, statusbar_rect_.y}, Point{w, statusbar_rect_.y}, Color(36, 42, 54), 1);

    char count_buf[64];
    snprintf(count_buf, sizeof(count_buf), "%zu items", filtered_items_.size());
    painter.draw_text(Rect{14, statusbar_rect_.y, 200, statusbar_rect_.height}, count_buf,
                      Font("Inter", 10, FontWeight::Regular), Color(148, 163, 184),
                      TextAlignment::Left, VerticalAlignment::Center);

    if (selected_item_ >= 0 && selected_item_ < (int)filtered_items_.size()) {
        const FileItem &it = filtered_items_[selected_item_];
        std::string sel_info = it.name + (it.is_dir ? "  (Folder)" : ("  •  " + format_size(it.size)));
        painter.draw_text(Rect{220, statusbar_rect_.y, w - 240, statusbar_rect_.height}, sel_info,
                          Font("Inter", 10, FontWeight::Medium), Color(203, 213, 225),
                          TextAlignment::Right, VerticalAlignment::Center);
    }
}

void SzponterWindow::on_mouse_down(MouseEvent &event) {
    Point p = event.pos();
    uint64_t now = get_current_time_ms();

    // 1. Sidebar click
    if (sidebar_rect_.contains(p)) {
        int sb_y = 38;
        int sb_h = 28;
        for (size_t i = 0; i < sidebar_items_.size(); ++i) {
            Rect r{10, sb_y, 170, sb_h};
            if (r.contains(p)) {
                selected_sidebar_ = (int)i;
                navigate_to(sidebar_items_[i].path);
                return;
            }
            sb_y += sb_h + 3;
        }
    }

    // 2. Toolbar buttons
    if (btn_back_rect_.contains(p)) {
        if (history_index_ > 0) {
            history_index_--;
            navigate_to(history_[history_index_]);
        }
        return;
    }
    if (btn_fwd_rect_.contains(p)) {
        if (history_index_ < (int)history_.size() - 1) {
            history_index_++;
            navigate_to(history_[history_index_]);
        }
        return;
    }
    if (btn_up_rect_.contains(p)) {
        size_t slash = current_path_.find_last_of('/');
        if (slash != std::string::npos) {
            std::string parent = (slash == 0) ? "/" : current_path_.substr(0, slash);
            navigate_to(parent);
        }
        return;
    }
    if (btn_refresh_rect_.contains(p)) {
        load_directory(current_path_);
        update(Rect{0, 0, bounds_.width, bounds_.height});
        return;
    }
    if (btn_mode_rect_.contains(p)) {
        list_mode_ = !list_mode_;
        update(Rect{0, 0, bounds_.width, bounds_.height});
        return;
    }

    // 3. Content item click & double click
    if (content_rect_.contains(p)) {
        int clicked_idx = -1;
        if (!list_mode_) {
            int item_w = 96;
            int item_h = 96;
            int cols = (content_rect_.width - 24) / item_w;
            if (cols < 1) cols = 1;

            int col = (p.x - content_rect_.x - 16) / item_w;
            int row = (p.y - content_rect_.y - 12) / item_h;
            if (col >= 0 && col < cols && row >= 0) {
                int idx = scroll_offset_ * cols + row * cols + col;
                if (idx >= 0 && idx < (int)filtered_items_.size()) {
                    clicked_idx = idx;
                }
            }
        } else {
            int row_h = 28;
            int row = (p.y - content_rect_.y) / row_h;
            int idx = scroll_offset_ + row;
            if (idx >= 0 && idx < (int)filtered_items_.size()) {
                clicked_idx = idx;
            }
        }

        if (clicked_idx >= 0) {
            bool is_double = (clicked_idx == last_click_index_ && (now - last_click_time_ < 450));
            selected_item_ = clicked_idx;
            last_click_time_ = now;
            last_click_index_ = clicked_idx;

            if (is_double) {
                open_item(filtered_items_[clicked_idx]);
            }
            update(Rect{0, 0, bounds_.width, bounds_.height});
            return;
        } else {
            selected_item_ = -1;
            update(Rect{0, 0, bounds_.width, bounds_.height});
        }
    }
}

void SzponterWindow::on_mouse_move(MouseEvent &event) {
    Point p = event.pos();

    // Check sidebar hover
    int new_sb_hover = -1;
    if (sidebar_rect_.contains(p)) {
        int sb_y = 38;
        int sb_h = 28;
        for (size_t i = 0; i < sidebar_items_.size(); ++i) {
            Rect r{10, sb_y, 170, sb_h};
            if (r.contains(p)) {
                new_sb_hover = (int)i;
                break;
            }
            sb_y += sb_h + 3;
        }
    }
    if (new_sb_hover != hovered_sidebar_) {
        hovered_sidebar_ = new_sb_hover;
        update(sidebar_rect_);
    }

    // Check content item hover
    int new_it_hover = -1;
    if (content_rect_.contains(p)) {
        if (!list_mode_) {
            int item_w = 96;
            int item_h = 96;
            int cols = (content_rect_.width - 24) / item_w;
            if (cols < 1) cols = 1;
            int col = (p.x - content_rect_.x - 16) / item_w;
            int row = (p.y - content_rect_.y - 12) / item_h;
            if (col >= 0 && col < cols && row >= 0) {
                int idx = scroll_offset_ * cols + row * cols + col;
                if (idx >= 0 && idx < (int)filtered_items_.size()) {
                    new_it_hover = idx;
                }
            }
        } else {
            int row_h = 28;
            int row = (p.y - content_rect_.y) / row_h;
            int idx = scroll_offset_ + row;
            if (idx >= 0 && idx < (int)filtered_items_.size()) {
                new_it_hover = idx;
            }
        }
    }
    if (new_it_hover != hovered_item_) {
        hovered_item_ = new_it_hover;
        update(content_rect_);
    }
}

void SzponterWindow::on_key_down(KeyEvent &event) {
    uint32_t sym = event.keysym();
    if (sym == XK_BackSpace) {
        if (!search_query_.empty()) {
            search_query_.pop_back();
            update_filter();
            update(Rect{0, 0, bounds_.width, bounds_.height});
        } else {
            // Go up
            size_t slash = current_path_.find_last_of('/');
            if (slash != std::string::npos) {
                std::string parent = (slash == 0) ? "/" : current_path_.substr(0, slash);
                navigate_to(parent);
            }
        }
        return;
    }

    if (sym == XK_Return || sym == XK_KP_Enter) {
        if (selected_item_ >= 0 && selected_item_ < (int)filtered_items_.size()) {
            open_item(filtered_items_[selected_item_]);
        }
        return;
    }

    if (sym == XK_Up) {
        if (scroll_offset_ > 0) {
            scroll_offset_--;
            update(content_rect_);
        }
        return;
    }

    if (sym == XK_Down) {
        scroll_offset_++;
        update(content_rect_);
        return;
    }

    // Alphanumeric typing for filter
    const std::string &txt = event.text();
    if (!txt.empty() && txt[0] >= 32 && txt[0] <= 126) {
        search_query_ += txt;
        update_filter();
        update(Rect{0, 0, bounds_.width, bounds_.height});
    }
}

} // namespace Szponter
