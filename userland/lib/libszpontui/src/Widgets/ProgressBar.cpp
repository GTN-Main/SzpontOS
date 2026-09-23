#include <SzpontUI/Widgets/ProgressBar.hpp>
#include <SzpontUI/Theme/Theme.hpp>
#include <algorithm>

namespace SzpontUI {

ProgressBar::ProgressBar(float value)
    : Widget("ProgressBar"), value_(std::max(0.0f, std::min(1.0f, value))) {
    min_size_ = Size{100, 10};
}

void ProgressBar::set_value(float val) {
    float clamped = std::max(0.0f, std::min(1.0f, val));
    if (value_ == clamped) return;
    value_ = clamped;
    update();
}

Size ProgressBar::measure(Size) {
    return Size{120, 10};
}

void ProgressBar::on_paint(Painter &painter) {
    Rect r{0, 0, bounds_.width, bounds_.height};
    int radius = r.height / 2;

    // Track
    painter.fill_rounded_rect(r, radius, current_theme().control_bg());
    painter.draw_rounded_rect(r, radius, current_theme().control_border(), 1);

    // Progress fill
    int fill_w = static_cast<int>(r.width * value_);
    if (fill_w > 4) {
        Rect fill_rect{0, 0, fill_w, r.height};
        painter.fill_gradient(fill_rect, current_theme().accent(), current_theme().accent_hover(), false);
        painter.fill_rounded_rect(fill_rect, radius, Color::transparent()); // ensure proper clipping
    }
}

} // namespace SzpontUI
