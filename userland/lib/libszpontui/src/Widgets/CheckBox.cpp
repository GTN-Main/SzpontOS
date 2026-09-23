#include <SzpontUI/Widgets/CheckBox.hpp>
#include <SzpontUI/Theme/Theme.hpp>

namespace SzpontUI {

CheckBox::CheckBox(std::string text, bool checked)
    : Widget("CheckBox"), text_(std::move(text)), checked_(checked) {
    min_size_ = Size{100, 24};
}

void CheckBox::set_text(std::string text) {
    text_ = std::move(text);
    invalidate_layout();
}

void CheckBox::set_checked(bool c) {
    if (checked_ == c) return;
    checked_ = c;
    on_toggled(checked_);
    update();
}

Size CheckBox::measure(Size) {
    BitmapSurface dummy(1, 1);
    Painter painter(dummy);
    Font f = current_theme().default_font();
    Size text_sz = painter.measure_text(text_, f);
    return Size{18 + 8 + text_sz.width + 4, std::max(20, text_sz.height + 4)};
}

void CheckBox::on_paint(Painter &painter) {
    int box_size = 18;
    int box_y = (bounds_.height - box_size) / 2;
    Rect box_rect{0, box_y, box_size, box_size};

    if (checked_) {
        painter.fill_rounded_rect(box_rect, 4, current_theme().accent());
        // Checkmark tick lines
        painter.draw_line(Point{4, box_y + 9}, Point{7, box_y + 13}, Color::white(), 2);
        painter.draw_line(Point{7, box_y + 13}, Point{14, box_y + 5}, Color::white(), 2);
    } else {
        painter.fill_rounded_rect(box_rect, 4, current_theme().control_bg());
        painter.draw_rounded_rect(box_rect, 4, current_theme().control_border(), 1);
    }

    // Label text
    if (!text_.empty()) {
        Rect text_rect{box_size + 8, 0, bounds_.width - box_size - 8, bounds_.height};
        painter.draw_text(text_rect, text_, current_theme().default_font(),
                          current_theme().text_primary(), TextAlignment::Left, VerticalAlignment::Center);
    }
}

void CheckBox::on_mouse_down(MouseEvent &event) {
    if (event.button() == MouseButton::Left && is_enabled()) {
        set_checked(!checked_);
    }
}

} // namespace SzpontUI
