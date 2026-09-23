#include "ImageViewerWindow.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <X11/keysym.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#include "stb_image.h"

namespace SzpontView {

using namespace SzpontUI;

static unsigned char *read_file_to_buffer(const char *path, size_t *out_size) {
    if (!path || !path[0]) return nullptr;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return nullptr;

    off_t size = lseek(fd, 0, SEEK_END);
    if (size <= 0) {
        close(fd);
        return nullptr;
    }
    lseek(fd, 0, SEEK_SET);

    unsigned char *buf = (unsigned char *)malloc((size_t)size);
    if (!buf) {
        close(fd);
        return nullptr;
    }

    size_t total_read = 0;
    while (total_read < (size_t)size) {
        ssize_t n = read(fd, buf + total_read, (size_t)size - total_read);
        if (n <= 0) break;
        total_read += (size_t)n;
    }
    close(fd);

    if (total_read != (size_t)size) {
        free(buf);
        return nullptr;
    }

    *out_size = (size_t)size;
    return buf;
}

ImageViewerWindow::ImageViewerWindow(int width, int height, const std::string &initial_path)
    : Window(Size{width, height}, "SzpontView - Image Viewer") {

    std::string start = initial_path;
    if (start.empty() || access(start.c_str(), R_OK) != 0) {
        // Try fallback to wallpaper
        if (access("/usr/share/artwork/wallpaper.jpg", R_OK) == 0) {
            start = "/usr/share/artwork/wallpaper.jpg";
        } else if (access("/usr/share/artwork/szpont-detected.jpg", R_OK) == 0) {
            start = "/usr/share/artwork/szpont-detected.jpg";
        }
    }

    if (!start.empty()) {
        open_image(start);
    }
}

std::string ImageViewerWindow::filename_only(const std::string &path) const {
    size_t slash = path.find_last_of('/');
    return (slash != std::string::npos) ? path.substr(slash + 1) : path;
}

void ImageViewerWindow::scan_directory(const std::string &filepath) {
    folder_images_.clear();
    current_index_ = -1;

    size_t slash = filepath.find_last_of('/');
    std::string dir_path = (slash != std::string::npos) ? filepath.substr(0, slash) : ".";
    if (dir_path.empty()) dir_path = "/";

    DIR *dir = opendir(dir_path.c_str());
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        std::string name = ent->d_name;
        size_t dot = name.find_last_of('.');
        if (dot == std::string::npos) continue;

        std::string ext = name.substr(dot);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp") {
            std::string full = dir_path;
            if (full.back() != '/') full += '/';
            full += name;
            folder_images_.push_back(full);
        }
    }
    closedir(dir);

    std::sort(folder_images_.begin(), folder_images_.end());

    for (size_t i = 0; i < folder_images_.size(); ++i) {
        if (folder_images_[i] == filepath) {
            current_index_ = (int)i;
            break;
        }
    }
}

bool ImageViewerWindow::load_image_file(const std::string &path) {
    size_t file_size = 0;
    unsigned char *file_buf = read_file_to_buffer(path.c_str(), &file_size);
    if (!file_buf) return false;

    int w = 0, h = 0, channels = 0;
    unsigned char *data = stbi_load_from_memory(file_buf, (int)file_size, &w, &h, &channels, 4);
    free(file_buf);

    if (!data || w <= 0 || h <= 0) return false;

    orig_w_ = w;
    orig_h_ = h;
    rotation_deg_ = 0;

    original_surface_.resize(w, h);
    const uint32_t *src32 = (const uint32_t *)data;

    for (int y = 0; y < h; ++y) {
        const uint32_t *src_row = src32 + y * w;
        for (int x = 0; x < w; ++x) {
            uint32_t rgba = src_row[x];
            uint32_t r = rgba & 0xFF;
            uint32_t g = (rgba >> 8) & 0xFF;
            uint32_t b = (rgba >> 16) & 0xFF;
            uint32_t a = (rgba >> 24) & 0xFF;

            if (a == 0) {
                r = g = b = 0;
            } else if (a < 255) {
                r = (r * a) / 255;
                g = (g * a) / 255;
                b = (b * a) / 255;
            }

            original_surface_.set_pixel(x, y, Color((uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a));
        }
    }

    stbi_image_free(data);
    return true;
}

bool ImageViewerWindow::open_image(const std::string &path) {
    if (!load_image_file(path)) return false;

    current_file_ = path;
    scan_directory(path);

    is_fit_ = true;
    zoom_scale_ = 1.0f;
    pan_x_ = 0;
    pan_y_ = 0;
    rotation_deg_ = 0;

    cached_w_ = 0;
    cached_h_ = 0;
    cached_rot_ = 0;

    set_title("SzpontView - " + filename_only(current_file_));
    update(Rect{0, 0, bounds_.width, bounds_.height});
    return true;
}

void ImageViewerWindow::next_image() {
    if (folder_images_.empty()) return;
    int next_idx = (current_index_ + 1) % folder_images_.size();
    open_image(folder_images_[next_idx]);
}

void ImageViewerWindow::prev_image() {
    if (folder_images_.empty()) return;
    int prev_idx = (current_index_ - 1 + (int)folder_images_.size()) % folder_images_.size();
    open_image(folder_images_[prev_idx]);
}

void ImageViewerWindow::zoom_in() {
    is_fit_ = false;
    zoom_scale_ *= 1.25f;
    if (zoom_scale_ > 10.0f) zoom_scale_ = 10.0f;
    cached_w_ = 0;
    update(Rect{0, 0, bounds_.width, bounds_.height});
}

void ImageViewerWindow::zoom_out() {
    is_fit_ = false;
    zoom_scale_ /= 1.25f;
    if (zoom_scale_ < 0.1f) zoom_scale_ = 0.1f;
    cached_w_ = 0;
    update(Rect{0, 0, bounds_.width, bounds_.height});
}

void ImageViewerWindow::zoom_fit() {
    is_fit_ = true;
    pan_x_ = 0;
    pan_y_ = 0;
    cached_w_ = 0;
    update(Rect{0, 0, bounds_.width, bounds_.height});
}

void ImageViewerWindow::zoom_actual() {
    is_fit_ = false;
    zoom_scale_ = 1.0f;
    pan_x_ = 0;
    pan_y_ = 0;
    cached_w_ = 0;
    update(Rect{0, 0, bounds_.width, bounds_.height});
}

void ImageViewerWindow::rotate_90() {
    rotation_deg_ = (rotation_deg_ + 90) % 360;
    cached_w_ = 0;
    update(Rect{0, 0, bounds_.width, bounds_.height});
}

void ImageViewerWindow::recompute_scaled_image() {
    if (orig_w_ <= 0 || orig_h_ <= 0) return;

    int eff_orig_w = (rotation_deg_ == 90 || rotation_deg_ == 270) ? orig_h_ : orig_w_;
    int eff_orig_h = (rotation_deg_ == 90 || rotation_deg_ == 270) ? orig_w_ : orig_h_;

    int avail_w = canvas_rect_.width - 32;
    int avail_h = canvas_rect_.height - 32;
    if (avail_w < 10) avail_w = 10;
    if (avail_h < 10) avail_h = 10;

    int target_w = 0;
    int target_h = 0;

    if (is_fit_) {
        float scale_w = (float)avail_w / (float)eff_orig_w;
        float scale_h = (float)avail_h / (float)eff_orig_h;
        float fit_scale = std::min(scale_w, scale_h);
        if (fit_scale > 1.0f) fit_scale = 1.0f; // don't upscale beyond 100% on fit
        zoom_scale_ = fit_scale;
        target_w = (int)(eff_orig_w * fit_scale);
        target_h = (int)(eff_orig_h * fit_scale);
    } else {
        target_w = (int)(eff_orig_w * zoom_scale_);
        target_h = (int)(eff_orig_h * zoom_scale_);
    }

    if (target_w < 4) target_w = 4;
    if (target_h < 4) target_h = 4;

    if (target_w == cached_w_ && target_h == cached_h_ && rotation_deg_ == cached_rot_) {
        return;
    }

    cached_w_ = target_w;
    cached_h_ = target_h;
    cached_rot_ = rotation_deg_;

    rendered_surface_.resize(target_w, target_h);

    for (int dy = 0; dy < target_h; ++dy) {
        int sy = (dy * eff_orig_h) / target_h;
        if (sy >= eff_orig_h) sy = eff_orig_h - 1;

        for (int dx = 0; dx < target_w; ++dx) {
            int sx = (dx * eff_orig_w) / target_w;
            if (sx >= eff_orig_w) sx = eff_orig_w - 1;

            // Map (sx, sy) through rotation
            int src_x = sx;
            int src_y = sy;
            if (rotation_deg_ == 90) {
                src_x = sy;
                src_y = orig_h_ - 1 - sx;
            } else if (rotation_deg_ == 180) {
                src_x = orig_w_ - 1 - sx;
                src_y = orig_h_ - 1 - sy;
            } else if (rotation_deg_ == 270) {
                src_x = orig_w_ - 1 - sy;
                src_y = sx;
            }

            if (src_x >= 0 && src_x < orig_w_ && src_y >= 0 && src_y < orig_h_) {
                rendered_surface_.set_pixel(dx, dy, original_surface_.get_pixel(src_x, src_y));
            }
        }
    }
}

void ImageViewerWindow::on_paint(Painter &painter) {
    int w = bounds_.width;
    int h = bounds_.height;

    toolbar_rect_ = Rect{0, 0, w, 44};
    canvas_rect_  = Rect{0, 44, w, h - 44};

    // 1. Toolbar Background
    painter.fill_rect(toolbar_rect_, Color(22, 26, 36));
    painter.draw_line(Point{0, toolbar_rect_.bottom() - 1}, Point{w, toolbar_rect_.bottom() - 1},
                      Color(37, 43, 58), 1);

    // Toolbar Buttons
    auto draw_tb_btn = [&](const Rect &r, const char *lbl, bool active = false) {
        painter.fill_rounded_rect(r, 6, active ? Color(59, 130, 246, 50) : Color(255, 255, 255, 16));
        painter.draw_rounded_rect(r, 6, active ? Color(59, 130, 246, 120) : Color(255, 255, 255, 26), 1);
        painter.draw_text(r, lbl, Font("Inter", 10, FontWeight::Bold),
                          Color(241, 245, 249), TextAlignment::Center, VerticalAlignment::Center);
    };

    btn_prev_rect_     = Rect{12, 9, 32, 26};
    btn_next_rect_     = Rect{48, 9, 32, 26};
    btn_zoom_out_rect_ = Rect{90, 9, 30, 26};
    btn_zoom_in_rect_  = Rect{124, 9, 30, 26};
    btn_fit_rect_      = Rect{164, 9, 38, 26};
    btn_actual_rect_   = Rect{206, 9, 44, 26};
    btn_rotate_rect_   = Rect{258, 9, 50, 26};

    draw_tb_btn(btn_prev_rect_, "<");
    draw_tb_btn(btn_next_rect_, ">");
    draw_tb_btn(btn_zoom_out_rect_, "-");
    draw_tb_btn(btn_zoom_in_rect_, "+");
    draw_tb_btn(btn_fit_rect_, "Fit", is_fit_);
    draw_tb_btn(btn_actual_rect_, "100%", !is_fit_ && std::abs(zoom_scale_ - 1.0f) < 0.05f);
    draw_tb_btn(btn_rotate_rect_, "Rot");

    // Metadata HUD (Center & Right)
    if (!current_file_.empty()) {
        char hud_buf[128];
        int pct = (int)(zoom_scale_ * 100.0f);
        snprintf(hud_buf, sizeof(hud_buf), "%s  (%d × %d)   •   %d%%   •   [%d / %zu]",
                 filename_only(current_file_).c_str(),
                 orig_w_, orig_h_, pct,
                 current_index_ + 1, folder_images_.size());
        painter.draw_text(Rect{320, 0, w - 330, 44}, hud_buf,
                          Font("Inter", 11, FontWeight::Medium), Color(203, 213, 225),
                          TextAlignment::Right, VerticalAlignment::Center);
    }

    // 2. Viewport Canvas Background
    painter.fill_rect(canvas_rect_, Color(13, 16, 22));

    if (orig_w_ <= 0 || orig_h_ <= 0) {
        painter.draw_text(canvas_rect_, "No image loaded",
                          Font("Inter", 13, FontWeight::Medium), Color(148, 163, 184),
                          TextAlignment::Center, VerticalAlignment::Center);
        return;
    }

    recompute_scaled_image();

    // Center in canvas + pan offset
    int img_x = canvas_rect_.x + (canvas_rect_.width - cached_w_) / 2 + pan_x_;
    int img_y = canvas_rect_.y + (canvas_rect_.height - cached_h_) / 2 + pan_y_;

    // Draw bitmap
    painter.draw_bitmap(Point{img_x, img_y}, rendered_surface_);

    // 1px subtle image border sheen
    painter.draw_rect(Rect{img_x - 1, img_y - 1, cached_w_ + 2, cached_h_ + 2}, Color(255, 255, 255, 20), 1);
}

void ImageViewerWindow::on_mouse_down(MouseEvent &event) {
    Point p = event.pos();

    // Toolbar buttons
    if (btn_prev_rect_.contains(p)) { prev_image(); return; }
    if (btn_next_rect_.contains(p)) { next_image(); return; }
    if (btn_zoom_out_rect_.contains(p)) { zoom_out(); return; }
    if (btn_zoom_in_rect_.contains(p)) { zoom_in(); return; }
    if (btn_fit_rect_.contains(p)) { zoom_fit(); return; }
    if (btn_actual_rect_.contains(p)) { zoom_actual(); return; }
    if (btn_rotate_rect_.contains(p)) { rotate_90(); return; }

    // Canvas click & drag
    if (canvas_rect_.contains(p)) {
        is_dragging_ = true;
        drag_start_mouse_ = p;
        drag_start_pan_x_ = pan_x_;
        drag_start_pan_y_ = pan_y_;
    }
}

void ImageViewerWindow::on_mouse_up(MouseEvent &) {
    is_dragging_ = false;
}

void ImageViewerWindow::on_mouse_move(MouseEvent &event) {
    if (is_dragging_) {
        Point p = event.pos();
        pan_x_ = drag_start_pan_x_ + (p.x - drag_start_mouse_.x);
        pan_y_ = drag_start_pan_y_ + (p.y - drag_start_mouse_.y);
        update(canvas_rect_);
    }
}

void ImageViewerWindow::on_key_down(KeyEvent &event) {
    uint32_t sym = event.keysym();
    if (sym == XK_Left || sym == XK_BackSpace) {
        prev_image();
        return;
    }
    if (sym == XK_Right || sym == XK_space) {
        next_image();
        return;
    }
    if (sym == XK_plus || sym == XK_equal || sym == XK_KP_Add) {
        zoom_in();
        return;
    }
    if (sym == XK_minus || sym == XK_KP_Subtract) {
        zoom_out();
        return;
    }
    if (sym == XK_f || sym == XK_0) {
        zoom_fit();
        return;
    }
    if (sym == XK_1) {
        zoom_actual();
        return;
    }
    if (sym == XK_r || sym == XK_R) {
        rotate_90();
        return;
    }
    if (sym == XK_q || sym == XK_Q || sym == XK_Escape) {
        close();
        return;
    }
}

} // namespace SzpontView
