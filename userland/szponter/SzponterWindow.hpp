#pragma once

#include <SzpontUI/SzpontUI.hpp>
#include <string>
#include <vector>
#include <sys/types.h>

namespace Szponter {

enum class FileType {
    Directory,
    Image,
    DesktopApp,
    Text,
    Executable,
    Other
};

struct FileItem {
    std::string name;
    std::string full_path;
    FileType type{FileType::Other};
    size_t size{0};
    mode_t mode{0};
    bool is_dir{false};
};

struct SidebarItem {
    std::string title;
    std::string path;
    std::string icon_char;
};

class SzponterWindow : public SzpontUI::Window {
public:
    SzponterWindow(int width = 880, int height = 560, const std::string &initial_path = "/");

    void navigate_to(const std::string &path);

protected:
    void on_paint(SzpontUI::Painter &painter) override;
    void on_mouse_down(SzpontUI::MouseEvent &event) override;
    void on_mouse_move(SzpontUI::MouseEvent &event) override;
    void on_key_down(SzpontUI::KeyEvent &event) override;

private:
    std::string current_path_;
    std::vector<std::string> history_;
    int history_index_{-1};

    std::vector<SidebarItem> sidebar_items_;
    int selected_sidebar_{-1};
    int hovered_sidebar_{-1};

    std::vector<FileItem> all_items_;
    std::vector<FileItem> filtered_items_;
    int selected_item_{-1};
    int hovered_item_{-1};
    int scroll_offset_{0};
    bool list_mode_{false}; // false = Grid, true = List
    std::string search_query_;

    uint64_t last_click_time_{0};
    int last_click_index_{-1};

    // UI Regions
    SzpontUI::Rect sidebar_rect_;
    SzpontUI::Rect toolbar_rect_;
    SzpontUI::Rect content_rect_;
    SzpontUI::Rect statusbar_rect_;

    SzpontUI::Rect btn_back_rect_;
    SzpontUI::Rect btn_fwd_rect_;
    SzpontUI::Rect btn_up_rect_;
    SzpontUI::Rect btn_refresh_rect_;
    SzpontUI::Rect btn_mode_rect_;
    SzpontUI::Rect search_box_rect_;

    void load_directory(const std::string &path);
    void update_filter();
    void open_item(const FileItem &item);
    FileType detect_file_type(const std::string &name, mode_t mode);
    std::string format_size(size_t bytes) const;
};

} // namespace Szponter
