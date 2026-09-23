#pragma once

#include <SzpontUI/Widgets/Widget.hpp>
#include <string>

namespace SzpontUI {

class CheckBox : public Widget {
public:
    explicit CheckBox(std::string text = "", bool checked = false);

    const std::string &text() const { return text_; }
    void set_text(std::string text);

    bool is_checked() const { return checked_; }
    void set_checked(bool c);

    Size measure(Size available) override;

    Signal<bool> on_toggled;

protected:
    void on_paint(Painter &painter) override;
    void on_mouse_down(MouseEvent &event) override;

private:
    std::string text_;
    bool checked_{false};
};

} // namespace SzpontUI
