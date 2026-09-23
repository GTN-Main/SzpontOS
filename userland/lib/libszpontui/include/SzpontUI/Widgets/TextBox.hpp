#pragma once

#include <SzpontUI/Widgets/Widget.hpp>
#include <SzpontUI/Font/Font.hpp>
#include <string>

namespace SzpontUI {

class TextBox : public Widget {
public:
    explicit TextBox(std::string text = "");

    const std::string &text() const { return text_; }
    void set_text(std::string text);

    const std::string &placeholder() const { return placeholder_; }
    void set_placeholder(std::string ph);

    bool is_password() const { return is_password_; }
    void set_password(bool password);

    void clear() { set_text(""); }

    Size measure(Size available) override;

    Signal<const std::string&> on_text_changed;
    Signal<const std::string&> on_return_pressed;

protected:
    void on_paint(Painter &painter) override;
    void on_mouse_down(MouseEvent &event) override;
    void on_key_down(KeyEvent &event) override;
    void on_focus_in(Event &event) override;
    void on_focus_out(Event &event) override;

private:
    std::string text_;
    std::string placeholder_;
    bool is_password_{false};
    size_t cursor_pos_{0};
    Font font_;
    bool cursor_visible_{true};
};

} // namespace SzpontUI
