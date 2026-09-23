#include <SzpontUI/Widgets/Slider.hpp>
#include <SzpontUI/Theme/Theme.hpp>
#include <algorithm>

namespace SzpontUI {

Slider::Slider(float min_val, float max_val, float current)
    : Widget("Slider"), min_val_(min_val), max_val_(max_val), value_(current) {
    min_size_ = Size{120, 24};
}

void Slider::set_range(float min_val, float max_val) {
    min_val_ = min_val;
    max_val_ = max_val;
    set_value(value_);
}

void Slider::set_value(float val) {
    float clamped = std::max(min_val_, std::min(max_val_, val));
    if (value_ == clamped) return;
    value_ = clamped;
    on_value_changed(value_);
    update();
}

Size Slider::measure(Size) {
    return Size{140, 24};
}

void Slider::on_paint(Painter &painter) {
    int track_h = 6;
    int track_y = (bounds_.height - track_h) / 2;
    int knob_radius = 8;
    int knob_diam = knob_radius * 2;

    int usable_w = bounds_.width - knob_diam;
    float norm = (max_val_ > min_val_) ? (value_ - min_val_) / (max_val_ - min_val_) : 0.0f;
    int knob_x = knob_radius + static_cast<int>(usable_w * norm);

    // Track background
    Rect track_rect{knob_radius, track_y, usable_w, track_h};
    painter.fill_rounded_rect(track_rect, 3, current_theme().control_bg());
    painter.draw_rounded_rect(track_rect, 3, current_theme().control_border(), 1);

    // Active track fill
    if (knob_x > knob_radius) {
        Rect active_rect{knob_radius, track_y, knob_x - knob_radius, track_h};
        painter.fill_rounded_rect(active_rect, 3, current_theme().accent());
    }

    // Draggable Knob (circle)
    Rect knob_rect{knob_x - knob_radius, (bounds_.height - knob_diam) / 2, knob_diam, knob_diam};
    painter.draw_shadow(knob_rect, 3, Color(0, 0, 0, 80));
    painter.fill_rounded_rect(knob_rect, knob_radius, Color::white());
    painter.draw_rounded_rect(knob_rect, knob_radius, current_theme().accent(), 2);
}

void Slider::update_value_from_x(int x) {
    int knob_radius = 8;
    int usable_w = bounds_.width - knob_radius * 2;
    if (usable_w <= 0) return;

    int rel_x = x - knob_radius;
    float norm = std::max(0.0f, std::min(1.0f, static_cast<float>(rel_x) / usable_w));
    set_value(min_val_ + norm * (max_val_ - min_val_));
}

void Slider::on_mouse_down(MouseEvent &event) {
    if (event.button() == MouseButton::Left && is_enabled()) {
        dragging_ = true;
        update_value_from_x(event.x());
    }
}

void Slider::on_mouse_move(MouseEvent &event) {
    if (dragging_) {
        update_value_from_x(event.x());
    }
}

void Slider::on_mouse_up(MouseEvent &event) {
    if (dragging_ && event.button() == MouseButton::Left) {
        dragging_ = false;
        update();
    }
}

} // namespace SzpontUI
