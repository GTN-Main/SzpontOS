#include <SzpontUI/Widgets/TextBox.hpp>
#include <SzpontUI/Theme/Theme.hpp>

namespace SzpontUI {

TextBox::TextBox(std::string text) : Widget("TextBox"), text_(std::move(text)) {
    min_size_ = Size{120, 32};
    font_ = Font("Inter", 13, FontWeight::Regular);
    cursor_pos_ = text_.size();
}

void TextBox::set_text(std::string text) {
    text_ = std::move(text);
    cursor_pos_ = std::min(cursor_pos_, text_.size());
    update();
}

void TextBox::set_placeholder(std::string ph) {
    placeholder_ = std::move(ph);
    update();
}

void TextBox::set_password(bool password) {
    is_password_ = password;
    update();
}

Size TextBox::measure(Size) {
    return Size{160, 32};
}

void TextBox::on_paint(Painter &painter) {
    Rect r{0, 0, bounds_.width, bounds_.height};
    int radius = current_theme().corner_radius_input();

    Color bg = current_theme().control_bg();
    Color border = is_focused() ? current_theme().accent() : current_theme().control_border();

    // Background & border
    painter.fill_rounded_rect(r, radius, bg);
    painter.draw_rounded_rect(r, radius, border, is_focused() ? 2 : 1);

    // Inner text area with padding
    Rect text_rect{8, 0, r.width - 16, r.height};

    if (text_.empty() && !placeholder_.empty() && !is_focused()) {
        painter.draw_text(text_rect, placeholder_, font_, current_theme().text_muted(),
                          TextAlignment::Left, VerticalAlignment::Center);
    } else {
        std::string display_text = is_password_ ? std::string(text_.size(), '*') : text_;
        painter.draw_text(text_rect, display_text, font_, current_theme().text_primary(),
                          TextAlignment::Left, VerticalAlignment::Center);

        // Draw cursor if focused
        if (is_focused() && cursor_visible_) {
            int cursor_x = text_rect.x;
            if (is_password_) {
                static int s_star_width = 0;
                if (s_star_width == 0) {
                    s_star_width = painter.measure_text("*", font_).width;
                }
                cursor_x += static_cast<int>(cursor_pos_) * s_star_width;
            } else {
                std::string prefix = text_.substr(0, cursor_pos_);
                Size prefix_size = painter.measure_text(prefix, font_);
                cursor_x += prefix_size.width;
            }
            int cursor_y = (r.height - 18) / 2;
            painter.fill_rect(Rect{cursor_x, cursor_y, 2, 18}, current_theme().accent());
        }
    }
}

void TextBox::on_mouse_down(MouseEvent &event) {
    if (event.button() == MouseButton::Left) {
        set_focus();
        cursor_pos_ = text_.size();
        cursor_visible_ = true;
        update();
    }
}

void TextBox::on_key_down(KeyEvent &event) {
    if (!is_enabled()) return;

    if (event.keysym() == 0xFF08) { // Backspace
        if (cursor_pos_ > 0 && !text_.empty()) {
            text_.erase(cursor_pos_ - 1, 1);
            cursor_pos_--;
            on_text_changed(text_);
            update();
        }
    } else if (event.keysym() == 0xFFFF) { // Delete
        if (cursor_pos_ < text_.size()) {
            text_.erase(cursor_pos_, 1);
            on_text_changed(text_);
            update();
        }
    } else if (event.keysym() == 0xFF51) { // Left arrow
        if (cursor_pos_ > 0) {
            cursor_pos_--;
            update();
        }
    } else if (event.keysym() == 0xFF53) { // Right arrow
        if (cursor_pos_ < text_.size()) {
            cursor_pos_++;
            update();
        }
    } else if (event.keysym() == 0xFF0D || event.keysym() == 0xFF8D) { // Return / Enter
        on_return_pressed(text_);
    } else if (!event.text().empty()) {
        char c = event.text()[0];
        if (static_cast<unsigned char>(c) >= 32 && c != 127) {
            text_.insert(cursor_pos_, event.text());
            cursor_pos_ += event.text().size();
            on_text_changed(text_);
            update();
        }
    }
}

void TextBox::on_focus_in(Event &) {
    cursor_visible_ = true;
    update();
}

void TextBox::on_focus_out(Event &) {
    cursor_visible_ = false;
    update();
}

} // namespace SzpontUI
