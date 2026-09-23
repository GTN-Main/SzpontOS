#pragma once

#include <SzpontUI/SzpontUI.hpp>
#include <string>
#include <vector>
#include <deque>
#include <sys/types.h>

namespace SzpontMon {

struct ProcessInfo {
    pid_t pid{0};
    pid_t ppid{0};
    std::string name;
    std::string state;
    size_t mem_kb{0};
    int threads{1};
};

struct SystemStats {
    float cpu_usage_pct{0.0f};
    uint64_t mem_total_kb{0};
    uint64_t mem_free_kb{0};
    uint64_t mem_used_kb{0};
    float mem_usage_pct{0.0f};
    uint32_t uptime_sec{0};
    int total_tasks{0};
    int running_tasks{0};
};

class SystemMonitorWindow : public SzpontUI::Window {
public:
    SystemMonitorWindow(int width = 860, int height = 580);

    void refresh_data();
    void kill_selected_process();

protected:
    void on_paint(SzpontUI::Painter &painter) override;
    void on_mouse_down(SzpontUI::MouseEvent &event) override;
    void on_mouse_move(SzpontUI::MouseEvent &event) override;
    void on_key_down(SzpontUI::KeyEvent &event) override;

private:
    SystemStats stats_;
    std::vector<ProcessInfo> all_processes_;
    std::vector<ProcessInfo> filtered_processes_;
    std::deque<float> cpu_history_; // last 30 values for sparkline

    uint64_t prev_cpu_work_{0};
    uint64_t prev_cpu_total_{0};

    int selected_idx_{-1};
    int hovered_idx_{-1};
    int scroll_offset_{0};
    std::string search_query_;

    // UI Layout Rectangles
    SzpontUI::Rect dashboard_rect_;
    SzpontUI::Rect table_header_rect_;
    SzpontUI::Rect table_rect_;
    SzpontUI::Rect action_bar_rect_;

    SzpontUI::Rect card_cpu_rect_;
    SzpontUI::Rect card_mem_rect_;
    SzpontUI::Rect card_sys_rect_;

    SzpontUI::Rect btn_kill_rect_;
    SzpontUI::Rect btn_refresh_rect_;
    SzpontUI::Rect search_rect_;

    void read_system_stats();
    void scan_processes();
    void update_filter();
    std::string format_uptime(uint32_t sec) const;
    std::string format_kb(size_t kb) const;
};

} // namespace SzpontMon
