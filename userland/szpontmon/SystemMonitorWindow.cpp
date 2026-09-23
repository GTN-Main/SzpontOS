#include "SystemMonitorWindow.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <X11/keysym.h>

namespace SzpontMon {

using namespace SzpontUI;

SystemMonitorWindow::SystemMonitorWindow(int width, int height)
    : Window(Size{width, height}, "SzpontMon - System Monitor") {

    for (int i = 0; i < 30; ++i) {
        cpu_history_.push_back(0.0f);
    }

    refresh_data();
}

std::string SystemMonitorWindow::format_uptime(uint32_t sec) const {
    uint32_t h = sec / 3600;
    uint32_t m = (sec % 3600) / 60;
    uint32_t s = sec % 60;
    char buf[64];
    snprintf(buf, sizeof(buf), "%02uh %02um %02us", h, m, s);
    return std::string(buf);
}

std::string SystemMonitorWindow::format_kb(size_t kb) const {
    char buf[32];
    if (kb < 1024) {
        snprintf(buf, sizeof(buf), "%zu KB", kb);
    } else {
        snprintf(buf, sizeof(buf), "%.1f MB", (double)kb / 1024.0);
    }
    return std::string(buf);
}

void SystemMonitorWindow::read_system_stats() {
    // 1. /proc/stat for CPU load
    FILE *f_stat = fopen("/proc/stat", "r");
    if (f_stat) {
        char line[256];
        if (fgets(line, sizeof(line), f_stat)) {
            if (strncmp(line, "cpu ", 4) == 0) {
                unsigned long u, n, s, idle, iowait = 0, irq = 0, softirq = 0;
                sscanf(line + 4, "%lu %lu %lu %lu %lu %lu %lu",
                       &u, &n, &s, &idle, &iowait, &irq, &softirq);
                uint64_t work = u + n + s + irq + softirq;
                uint64_t total = work + idle + iowait;

                if (prev_cpu_total_ > 0 && total > prev_cpu_total_) {
                    uint64_t delta_work = work - prev_cpu_work_;
                    uint64_t delta_total = total - prev_cpu_total_;
                    stats_.cpu_usage_pct = ((float)delta_work / (float)delta_total) * 100.0f;
                    if (stats_.cpu_usage_pct < 0.0f) stats_.cpu_usage_pct = 0.0f;
                    if (stats_.cpu_usage_pct > 100.0f) stats_.cpu_usage_pct = 100.0f;
                }
                prev_cpu_work_ = work;
                prev_cpu_total_ = total;
            }
        }
        fclose(f_stat);
    }

    // Update history
    cpu_history_.push_back(stats_.cpu_usage_pct);
    if (cpu_history_.size() > 30) {
        cpu_history_.pop_front();
    }

    // 2. /proc/meminfo for Memory stats
    FILE *f_mem = fopen("/proc/meminfo", "r");
    if (f_mem) {
        char line[256];
        while (fgets(line, sizeof(line), f_mem)) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                sscanf(line + 9, "%lu", &stats_.mem_total_kb);
            } else if (strncmp(line, "MemFree:", 8) == 0) {
                sscanf(line + 8, "%lu", &stats_.mem_free_kb);
            }
        }
        fclose(f_mem);

        if (stats_.mem_total_kb > 0) {
            stats_.mem_used_kb = (stats_.mem_total_kb > stats_.mem_free_kb) ?
                                  (stats_.mem_total_kb - stats_.mem_free_kb) : 0;
            stats_.mem_usage_pct = ((float)stats_.mem_used_kb / (float)stats_.mem_total_kb) * 100.0f;
        }
    }

    // 3. /proc/uptime for Uptime
    FILE *f_up = fopen("/proc/uptime", "r");
    if (f_up) {
        double up = 0.0;
        if (fscanf(f_up, "%lf", &up) == 1) {
            stats_.uptime_sec = (uint32_t)up;
        }
        fclose(f_up);
    }
}

void SystemMonitorWindow::scan_processes() {
    all_processes_.clear();
    stats_.total_tasks = 0;
    stats_.running_tasks = 0;

    DIR *dir = opendir("/proc");
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (!isdigit((unsigned char)ent->d_name[0])) continue;

        pid_t pid = (pid_t)atoi(ent->d_name);
        if (pid <= 0) continue;

        char status_path[64];
        snprintf(status_path, sizeof(status_path), "/proc/%d/status", pid);

        FILE *f = fopen(status_path, "r");
        if (!f) continue;

        ProcessInfo p;
        p.pid = pid;
        p.ppid = 0;
        p.mem_kb = 4096;
        p.threads = 1;
        p.state = "R";

        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "Name:\t", 6) == 0) {
                char *s = line + 6;
                char *end = s + strlen(s) - 1;
                while (end > s && (*end == '\n' || *end == '\r' || *end == ' ')) *end-- = '\0';
                p.name = s;
            } else if (strncmp(line, "State:\t", 7) == 0) {
                p.state = std::string(1, line[7]);
            } else if (strncmp(line, "PPid:\t", 6) == 0) {
                p.ppid = (pid_t)atoi(line + 6);
            } else if (strncmp(line, "Threads:\t", 9) == 0) {
                p.threads = atoi(line + 9);
            } else if (strncmp(line, "VmRSS:\t", 7) == 0) {
                p.mem_kb = (size_t)strtoul(line + 7, nullptr, 10);
            } else if (strncmp(line, "VmSize:\t", 8) == 0 && p.mem_kb == 4096) {
                p.mem_kb = (size_t)strtoul(line + 8, nullptr, 10);
            }
        }
        fclose(f);

        stats_.total_tasks++;
        if (p.state == "R" || p.state.empty()) stats_.running_tasks++;

        all_processes_.push_back(p);
    }
    closedir(dir);

    // Sort by PID
    std::sort(all_processes_.begin(), all_processes_.end(), [](const ProcessInfo &a, const ProcessInfo &b) {
        return a.pid < b.pid;
    });

    update_filter();
}

void SystemMonitorWindow::update_filter() {
    filtered_processes_.clear();
    if (search_query_.empty()) {
        filtered_processes_ = all_processes_;
    } else {
        std::string q = search_query_;
        std::transform(q.begin(), q.end(), q.begin(), ::tolower);
        for (const auto &p : all_processes_) {
            std::string n = p.name;
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            char pid_buf[32];
            snprintf(pid_buf, sizeof(pid_buf), "%d", p.pid);
            if (n.find(q) != std::string::npos || std::string(pid_buf).find(q) != std::string::npos) {
                filtered_processes_.push_back(p);
            }
        }
    }

    if (selected_idx_ >= (int)filtered_processes_.size()) {
        selected_idx_ = (int)filtered_processes_.size() - 1;
    }
}

void SystemMonitorWindow::refresh_data() {
    read_system_stats();
    scan_processes();
    update(Rect{0, 0, bounds_.width, bounds_.height});
}

void SystemMonitorWindow::kill_selected_process() {
    if (selected_idx_ >= 0 && selected_idx_ < (int)filtered_processes_.size()) {
        pid_t pid = filtered_processes_[selected_idx_].pid;
        if (pid > 1) {
            kill(pid, SIGTERM);
            usleep(10000);
            refresh_data();
        }
    }
}

void SystemMonitorWindow::on_paint(Painter &painter) {
    int w = bounds_.width;
    int h = bounds_.height;

    dashboard_rect_     = Rect{0, 0, w, 126};
    table_header_rect_  = Rect{0, 126, w, 32};
    action_bar_rect_    = Rect{0, h - 44, w, 44};
    table_rect_         = Rect{0, 158, w, h - 158 - 44};

    // 1. Dashboard Area Background
    painter.fill_rect(dashboard_rect_, Color(20, 24, 32));
    painter.draw_line(Point{0, dashboard_rect_.bottom() - 1}, Point{w, dashboard_rect_.bottom() - 1},
                      Color(36, 42, 54), 1);

    // Three Performance Metric Cards
    int card_w = (w - 32) / 3;
    int card_h = 104;
    int card_y = 11;

    card_cpu_rect_ = Rect{12, card_y, card_w, card_h};
    card_mem_rect_ = Rect{12 + card_w + 4, card_y, card_w, card_h};
    card_sys_rect_ = Rect{12 + (card_w + 4) * 2, card_y, card_w, card_h};

    auto draw_card_base = [&](const Rect &r, const char *title) {
        painter.fill_rounded_rect(r, 8, Color(26, 31, 42));
        painter.draw_rounded_rect(r, 8, Color(255, 255, 255, 24), 1);
        painter.draw_text(Rect{r.x + 12, r.y + 10, r.width - 24, 16}, title,
                          Font("Inter", 10, FontWeight::Bold), Color(148, 163, 184),
                          TextAlignment::Left, VerticalAlignment::Center);
    };

    // Card 1: CPU Activity
    draw_card_base(card_cpu_rect_, "CPU ACTIVITY");
    char cpu_str[32];
    snprintf(cpu_str, sizeof(cpu_str), "%.1f%%", stats_.cpu_usage_pct);
    painter.draw_text(Rect{card_cpu_rect_.x + 12, card_cpu_rect_.y + 28, 90, 30}, cpu_str,
                      Font("Inter", 20, FontWeight::Bold), Color(56, 189, 248),
                      TextAlignment::Left, VerticalAlignment::Center);

    // Sparkline Graph
    int spark_x = card_cpu_rect_.x + 110;
    int spark_y = card_cpu_rect_.y + 30;
    int spark_w = card_cpu_rect_.width - 124;
    int spark_h = 58;

    painter.fill_rounded_rect(Rect{spark_x, spark_y, spark_w, spark_h}, 4, Color(15, 20, 28));
    painter.draw_rounded_rect(Rect{spark_x, spark_y, spark_w, spark_h}, 4, Color(255, 255, 255, 14), 1);

    if (cpu_history_.size() >= 2 && spark_w > 10) {
        float step_x = (float)spark_w / (float)(cpu_history_.size() - 1);
        for (size_t i = 0; i < cpu_history_.size() - 1; ++i) {
            float v1 = cpu_history_[i] / 100.0f;
            float v2 = cpu_history_[i + 1] / 100.0f;
            int x1 = spark_x + (int)(i * step_x);
            int x2 = spark_x + (int)((i + 1) * step_x);
            int y1 = spark_y + spark_h - (int)(v1 * (spark_h - 4)) - 2;
            int y2 = spark_y + spark_h - (int)(v2 * (spark_h - 4)) - 2;
            painter.draw_line(Point{x1, y1}, Point{x2, y2}, Color(56, 189, 248), 2);
        }
    }

    // Card 2: Memory (RAM)
    draw_card_base(card_mem_rect_, "MEMORY (RAM)");
    char mem_str[64];
    snprintf(mem_str, sizeof(mem_str), "%lu MB / %lu MB",
             stats_.mem_used_kb / 1024, stats_.mem_total_kb / 1024);
    painter.draw_text(Rect{card_mem_rect_.x + 12, card_mem_rect_.y + 28, card_mem_rect_.width - 24, 24}, mem_str,
                      Font("Inter", 15, FontWeight::Bold), Color(248, 250, 252),
                      TextAlignment::Left, VerticalAlignment::Center);

    // RAM Progress Bar
    Rect pbar{card_mem_rect_.x + 12, card_mem_rect_.y + 58, card_mem_rect_.width - 24, 14};
    painter.fill_rounded_rect(pbar, 4, Color(15, 20, 28));
    painter.draw_rounded_rect(pbar, 4, Color(255, 255, 255, 20), 1);
    int fill_w = (int)((pbar.width - 2) * (stats_.mem_usage_pct / 100.0f));
    if (fill_w > 0) {
        painter.fill_rounded_rect(Rect{pbar.x + 1, pbar.y + 1, fill_w, pbar.height - 2}, 3, Color(168, 85, 247));
    }
    char pbar_pct[32];
    snprintf(pbar_pct, sizeof(pbar_pct), "%.1f%% used", stats_.mem_usage_pct);
    painter.draw_text(Rect{card_mem_rect_.x + 12, card_mem_rect_.y + 78, card_mem_rect_.width - 24, 16}, pbar_pct,
                      Font("Inter", 10, FontWeight::Regular), Color(148, 163, 184),
                      TextAlignment::Left, VerticalAlignment::Center);

    // Card 3: System Status & Uptime
    draw_card_base(card_sys_rect_, "SYSTEM STATUS");
    char tasks_str[64];
    snprintf(tasks_str, sizeof(tasks_str), "%d processes (%d running)", stats_.total_tasks, stats_.running_tasks);
    painter.draw_text(Rect{card_sys_rect_.x + 12, card_sys_rect_.y + 28, card_sys_rect_.width - 24, 20}, tasks_str,
                      Font("Inter", 12, FontWeight::Medium), Color(241, 245, 249),
                      TextAlignment::Left, VerticalAlignment::Center);

    std::string up_str = "Uptime: " + format_uptime(stats_.uptime_sec);
    painter.draw_text(Rect{card_sys_rect_.x + 12, card_sys_rect_.y + 52, card_sys_rect_.width - 24, 18}, up_str,
                      Font("Inter", 11, FontWeight::Regular), Color(203, 213, 225),
                      TextAlignment::Left, VerticalAlignment::Center);

    painter.draw_text(Rect{card_sys_rect_.x + 12, card_sys_rect_.y + 74, card_sys_rect_.width - 24, 16}, "SzpontOS 1.0 (x86_64)",
                      Font("Inter", 10, FontWeight::Regular), Color(100, 116, 139),
                      TextAlignment::Left, VerticalAlignment::Center);

    // 2. Table Header
    painter.fill_rect(table_header_rect_, Color(24, 28, 38));
    painter.draw_line(Point{0, table_header_rect_.bottom() - 1}, Point{w, table_header_rect_.bottom() - 1},
                      Color(36, 42, 54), 1);

    auto draw_th = [&](int x, int tw, const char *title) {
        painter.draw_text(Rect{x, table_header_rect_.y, tw, table_header_rect_.height}, title,
                          Font("Inter", 10, FontWeight::Bold), Color(148, 163, 184),
                          TextAlignment::Left, VerticalAlignment::Center);
    };

    draw_th(20, 70, "PID");
    draw_th(100, 300, "PROCESS NAME");
    draw_th(410, 100, "STATE");
    draw_th(520, 120, "MEMORY");
    draw_th(650, 80, "THREADS");

    // 3. Process Table Rows
    painter.fill_rect(table_rect_, Color(15, 18, 24));

    int row_h = 28;
    int max_rows = table_rect_.height / row_h;
    int start_idx = scroll_offset_;

    for (int i = start_idx; i < (int)filtered_processes_.size() && i < start_idx + max_rows; ++i) {
        const ProcessInfo &p = filtered_processes_[i];
        int ry = table_rect_.y + (i - start_idx) * row_h;
        Rect r{10, ry + 1, w - 20, row_h - 2};

        bool is_selected = (i == selected_idx_);
        bool is_hover = (i == hovered_idx_);

        if (is_selected) {
            painter.fill_rounded_rect(r, 4, Color(59, 130, 246, 50));
            painter.draw_rounded_rect(r, 4, Color(59, 130, 246, 110), 1);
        } else if (is_hover) {
            painter.fill_rounded_rect(r, 4, Color(255, 255, 255, 12));
        }

        Color text_col = is_selected ? Color(255, 255, 255) : Color(241, 245, 249);

        // PID
        char pid_buf[32];
        snprintf(pid_buf, sizeof(pid_buf), "%d", p.pid);
        painter.draw_text(Rect{20, ry, 70, row_h}, pid_buf,
                          Font("Inter", 11, FontWeight::Regular), text_col,
                          TextAlignment::Left, VerticalAlignment::Center);

        // Name
        painter.draw_text(Rect{100, ry, 300, row_h}, p.name,
                          Font("Inter", 11, FontWeight::Medium), text_col,
                          TextAlignment::Left, VerticalAlignment::Center);

        // State pill
        Rect state_pill{410, ry + 5, 28, 18};
        Color state_col = (p.state == "R") ? Color(16, 185, 129) : Color(100, 116, 139);
        painter.fill_rounded_rect(state_pill, 4, state_col);
        painter.draw_text(state_pill, p.state, Font("Inter", 9, FontWeight::Bold),
                          Color(255, 255, 255), TextAlignment::Center, VerticalAlignment::Center);

        // Memory
        painter.draw_text(Rect{520, ry, 120, row_h}, format_kb(p.mem_kb),
                          Font("Inter", 11, FontWeight::Regular), Color(203, 213, 225),
                          TextAlignment::Left, VerticalAlignment::Center);

        // Threads
        char th_buf[16];
        snprintf(th_buf, sizeof(th_buf), "%d", p.threads);
        painter.draw_text(Rect{650, ry, 80, row_h}, th_buf,
                          Font("Inter", 11, FontWeight::Regular), Color(148, 163, 184),
                          TextAlignment::Left, VerticalAlignment::Center);
    }

    // 4. Action Bar (Bottom)
    painter.fill_rect(action_bar_rect_, Color(20, 24, 32));
    painter.draw_line(Point{0, action_bar_rect_.y}, Point{w, action_bar_rect_.y}, Color(36, 42, 54), 1);

    // Selected process status info
    std::string sel_info;
    if (selected_idx_ >= 0 && selected_idx_ < (int)filtered_processes_.size()) {
        const ProcessInfo &p = filtered_processes_[selected_idx_];
        char buf[128];
        snprintf(buf, sizeof(buf), "Selected: %s (PID %d)  •  Memory: %s",
                 p.name.c_str(), p.pid, format_kb(p.mem_kb).c_str());
        sel_info = buf;
    } else {
        sel_info = "Select a process to manage or end task.";
    }
    painter.draw_text(Rect{16, action_bar_rect_.y, 400, action_bar_rect_.height}, sel_info,
                      Font("Inter", 11, FontWeight::Regular), Color(203, 213, 225),
                      TextAlignment::Left, VerticalAlignment::Center);

    // Filter box
    search_rect_ = Rect{w - 290, action_bar_rect_.y + 8, 120, 28};
    painter.fill_rounded_rect(search_rect_, 5, Color(15, 18, 25));
    painter.draw_rounded_rect(search_rect_, 5, Color(255, 255, 255, 20), 1);
    std::string s_txt = search_query_.empty() ? "Filter..." : search_query_;
    painter.draw_text(Rect{search_rect_.x + 8, search_rect_.y, search_rect_.width - 16, search_rect_.height}, s_txt,
                      Font("Inter", 10, FontWeight::Regular),
                      search_query_.empty() ? Color(148, 163, 184) : Color(248, 250, 252),
                      TextAlignment::Left, VerticalAlignment::Center);

    // Refresh Button
    btn_refresh_rect_ = Rect{w - 160, action_bar_rect_.y + 8, 64, 28};
    painter.fill_rounded_rect(btn_refresh_rect_, 5, Color(255, 255, 255, 16));
    painter.draw_rounded_rect(btn_refresh_rect_, 5, Color(255, 255, 255, 26), 1);
    painter.draw_text(btn_refresh_rect_, "Refresh", Font("Inter", 10, FontWeight::Medium),
                      Color(241, 245, 249), TextAlignment::Center, VerticalAlignment::Center);

    // Kill Task Button
    btn_kill_rect_ = Rect{w - 88, action_bar_rect_.y + 8, 76, 28};
    bool can_kill = (selected_idx_ >= 0 && selected_idx_ < (int)filtered_processes_.size() &&
                     filtered_processes_[selected_idx_].pid > 1);
    painter.fill_rounded_rect(btn_kill_rect_, 5, can_kill ? Color(239, 68, 68, 40) : Color(255, 255, 255, 8));
    painter.draw_rounded_rect(btn_kill_rect_, 5, can_kill ? Color(239, 68, 68, 90) : Color(255, 255, 255, 14), 1);
    painter.draw_text(btn_kill_rect_, "End Task", Font("Inter", 10, FontWeight::Bold),
                      can_kill ? Color(252, 165, 165) : Color(100, 116, 139),
                      TextAlignment::Center, VerticalAlignment::Center);
}

void SystemMonitorWindow::on_mouse_down(MouseEvent &event) {
    Point p = event.pos();

    // Kill button
    if (btn_kill_rect_.contains(p)) {
        kill_selected_process();
        return;
    }

    // Refresh button
    if (btn_refresh_rect_.contains(p)) {
        refresh_data();
        return;
    }

    // Process table row click
    if (table_rect_.contains(p)) {
        int row_h = 28;
        int row = (p.y - table_rect_.y) / row_h;
        int idx = scroll_offset_ + row;
        if (idx >= 0 && idx < (int)filtered_processes_.size()) {
            selected_idx_ = idx;
        } else {
            selected_idx_ = -1;
        }
        update(Rect{0, 0, bounds_.width, bounds_.height});
    }
}

void SystemMonitorWindow::on_mouse_move(MouseEvent &event) {
    Point p = event.pos();
    int new_hover = -1;
    if (table_rect_.contains(p)) {
        int row_h = 28;
        int row = (p.y - table_rect_.y) / row_h;
        int idx = scroll_offset_ + row;
        if (idx >= 0 && idx < (int)filtered_processes_.size()) {
            new_hover = idx;
        }
    }
    if (new_hover != hovered_idx_) {
        hovered_idx_ = new_hover;
        update(table_rect_);
    }
}

void SystemMonitorWindow::on_key_down(KeyEvent &event) {
    uint32_t sym = event.keysym();

    if (sym == XK_Up) {
        if (selected_idx_ > 0) {
            selected_idx_--;
            if (selected_idx_ < scroll_offset_) scroll_offset_ = selected_idx_;
            update(Rect{0, 0, bounds_.width, bounds_.height});
        }
        return;
    }

    if (sym == XK_Down) {
        if (selected_idx_ < (int)filtered_processes_.size() - 1) {
            selected_idx_++;
            int max_rows = table_rect_.height / 28;
            if (selected_idx_ >= scroll_offset_ + max_rows) scroll_offset_ = selected_idx_ - max_rows + 1;
            update(Rect{0, 0, bounds_.width, bounds_.height});
        }
        return;
    }

    if (sym == XK_Delete || sym == XK_k || sym == XK_K) {
        kill_selected_process();
        return;
    }

    if (sym == XK_BackSpace) {
        if (!search_query_.empty()) {
            search_query_.pop_back();
            update_filter();
            update(Rect{0, 0, bounds_.width, bounds_.height});
        }
        return;
    }

    const std::string &txt = event.text();
    if (!txt.empty() && txt[0] >= 32 && txt[0] <= 126) {
        search_query_ += txt;
        update_filter();
        update(Rect{0, 0, bounds_.width, bounds_.height});
    }
}

} // namespace SzpontMon
