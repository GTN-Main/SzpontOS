#include <SzpontUI/Widgets/Button.hpp>
#include <SzpontUI/Theme/Theme.hpp>

namespace SzpontUI {

Button::Button(std::string text) : Widget("Button"), text_(std::move(text)) {
    min_size_ = Size{80, 32};
    font_ = Font("Inter", 13, FontWeight::Medium);
}

void Button::set_text(std::string text) {
    if (text_ == text) return;
    text_ = std::move(text);
    invalidate_layout();
}

void Button::set_style(ButtonStyle style) {
    style_ = style;
    update();
}

Size Button::measure(Size) {
    BitmapSurface dummy(1, 1);
    Painter painter(dummy);
    Size text_size = painter.measure_text(text_, font_);
    int w = std::max(min_size_.width, text_size.width + 24);
    int h = std::max(min_size_.height, text_size.height + 14);
    return Size{w, h};
}

void Button::on_paint(Painter &painter) {
    Rect r{0, 0, bounds_.width, bounds_.height};
    int radius = current_theme().corner_radius_button();

    Color bg;
    Color border;
    Color text_color;

    if (!is_enabled()) {
        bg = current_theme().control_bg().with_opacity(0.5f);
        border = current_theme().control_border().with_opacity(0.3f);
        text_color = current_theme().text_muted();
    } else if (style_ == ButtonStyle::Primary) {
        if (pressed_) bg = current_theme().accent_active();
        else if (hovered_) bg = current_theme().accent_hover();
        else bg = current_theme().accent();

        border = bg;
        text_color = current_theme().accent_text();
    } else if (style_ == ButtonStyle::Danger) {
        if (pressed_) bg = current_theme().danger().with_opacity(0.7f);
        else if (hovered_) bg = current_theme().danger();
        else bg = current_theme().danger().with_opacity(0.85f);

        border = bg;
        text_color = Color::white();
    } else { // Default / Secondary
        if (pressed_) bg = current_theme().surface_active();
        else if (hovered_) bg = current_theme().control_hover();
        else bg = current_theme().control_bg();

        border = hovered_ ? current_theme().accent().with_opacity(0.6f) : current_theme().control_border();
        text_color = current_theme().text_primary();
    }

    // Background fill & border
    painter.fill_rounded_rect(r, radius, bg);
    painter.draw_rounded_rect(r, radius, border, 1);

    // Button label
    Point text_offset{0, pressed_ ? 1 : 0};
    Rect text_rect = r.translated(text_offset);
    painter.draw_text(text_rect, text_, font_, text_color, TextAlignment::Center, VerticalAlignment::Center);
}

void Button::on_mouse_down(MouseEvent &event) {
    if (event.button() == MouseButton::Left && is_enabled()) {
        pressed_ = true;
        update();
    }
}

void Button::on_mouse_up(MouseEvent &event) {
    if (pressed_ && event.button() == MouseButton::Left && is_enabled()) {
        pressed_ = false;
        update();
        if (hovered_) {
            on_click();
        }
    }
}

void Button::on_mouse_enter(MouseEvent &) {
    hovered_ = true;
    update();
}

void Button::on_mouse_leave(MouseEvent &) {
    hovered_ = false;
    pressed_ = false;
    update();
}

} // namespace SzpontUI
