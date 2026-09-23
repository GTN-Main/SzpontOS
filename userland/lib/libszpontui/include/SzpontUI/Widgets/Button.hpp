#pragma once

#include <SzpontUI/Widgets/Widget.hpp>
#include <SzpontUI/Font/Font.hpp>
#include <string>

namespace SzpontUI {

enum class ButtonStyle {
    Default,
    Primary,
    Danger,
    Ghost
};

class Button : public Widget {
public:
    explicit Button(std::string text = "");

    const std::string &text() const { return text_; }
    void set_text(std::string text);

    ButtonStyle style() const { return style_; }
    void set_style(ButtonStyle style);

    void set_accent(bool accent) {
        set_style(accent ? ButtonStyle::Primary : ButtonStyle::Default);
    }

    Size measure(Size available) override;

    Signal<> on_click;

protected:
    void on_paint(Painter &painter) override;
    void on_mouse_down(MouseEvent &event) override;
    void on_mouse_up(MouseEvent &event) override;
    void on_mouse_enter(MouseEvent &event) override;
    void on_mouse_leave(MouseEvent &event) override;

private:
    std::string text_;
    ButtonStyle style_{ButtonStyle::Default};
    Font font_;
};

} // namespace SzpontUI
