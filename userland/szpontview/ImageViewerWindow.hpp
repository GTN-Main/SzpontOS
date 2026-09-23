#pragma once

#include <SzpontUI/SzpontUI.hpp>
#include <string>
#include <vector>
#include <memory>

namespace SzpontView {

class ImageViewerWindow : public SzpontUI::Window {
public:
    ImageViewerWindow(int width = 920, int height = 620, const std::string &initial_path = "");

    bool open_image(const std::string &path);
    void next_image();
    void prev_image();
    void zoom_in();
    void zoom_out();
    void zoom_fit();
    void zoom_actual();
    void rotate_90();

protected:
    void on_paint(SzpontUI::Painter &painter) override;
    void on_mouse_down(SzpontUI::MouseEvent &event) override;
    void on_mouse_up(SzpontUI::MouseEvent &event) override;
    void on_mouse_move(SzpontUI::MouseEvent &event) override;
    void on_key_down(SzpontUI::KeyEvent &event) override;

private:
    std::string current_file_;
    std::vector<std::string> folder_images_;
    int current_index_{-1};

    // Original loaded image
    int orig_w_{0};
    int orig_h_{0};
    int rotation_deg_{0}; // 0, 90, 180, 270
    SzpontUI::BitmapSurface original_surface_;

    // View state
    bool is_fit_{true};
    float zoom_scale_{1.0f};
    int pan_x_{0};
    int pan_y_{0};

    // Drag state
    bool is_dragging_{false};
    SzpontUI::Point drag_start_mouse_;
    int drag_start_pan_x_{0};
    int drag_start_pan_y_{0};

    // Cached scaled surface for current view
    SzpontUI::BitmapSurface rendered_surface_;
    int cached_w_{0};
    int cached_h_{0};
    int cached_rot_{0};

    // UI Regions
    SzpontUI::Rect toolbar_rect_;
    SzpontUI::Rect canvas_rect_;

    SzpontUI::Rect btn_prev_rect_;
    SzpontUI::Rect btn_next_rect_;
    SzpontUI::Rect btn_zoom_out_rect_;
    SzpontUI::Rect btn_zoom_in_rect_;
    SzpontUI::Rect btn_fit_rect_;
    SzpontUI::Rect btn_actual_rect_;
    SzpontUI::Rect btn_rotate_rect_;

    void scan_directory(const std::string &filepath);
    bool load_image_file(const std::string &path);
    void recompute_scaled_image();
    std::string filename_only(const std::string &path) const;
};

} // namespace SzpontView
