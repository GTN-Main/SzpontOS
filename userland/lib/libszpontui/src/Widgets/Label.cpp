#include <SzpontUI/Widgets/Label.hpp>
#include <SzpontUI/Theme/Theme.hpp>

namespace SzpontUI {

Label::Label(std::string text, Font font, Color color)
    : Widget("Label"), text_(std::move(text)), font_(font), color_(color) {
    if (color != Color(255, 255, 255)) {
        has_custom_color_ = true;
    }
}

void Label::set_text(std::string text) {
    if (text_ == text) return;
    text_ = std::move(text);
    invalidate_layout();
}

void Label::set_font(Font font) {
    font_ = font;
    invalidate_layout();
}

void Label::set_color(Color c) {
    color_ = c;
    has_custom_color_ = true;
    update();
}

void Label::set_text_alignment(TextAlignment align) {
    text_align_ = align;
    update();
}

Size Label::measure(Size) {
    BitmapSurface dummy(1, 1);
    Painter painter(dummy);
    Size text_size = painter.measure_text(text_, font_);
    return Size{text_size.width + 4, text_size.height + 4};
}

void Label::on_paint(Painter &painter) {
    Color col = has_custom_color_ ? color_ : current_theme().text_primary();
    if (!is_enabled()) {
        col = current_theme().text_muted();
    }
    painter.draw_text(Rect{0, 0, bounds_.width, bounds_.height}, text_, font_, col, text_align_, VerticalAlignment::Center);
}

} // namespace SzpontUI
