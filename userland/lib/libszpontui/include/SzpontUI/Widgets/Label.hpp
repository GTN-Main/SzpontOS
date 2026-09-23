#pragma once

#include <SzpontUI/Widgets/Widget.hpp>
#include <SzpontUI/Font/Font.hpp>
#include <string>

namespace SzpontUI {

class Label : public Widget {
public:
    explicit Label(std::string text = "", Font font = Font(), Color color = Color(255, 255, 255));

    const std::string &text() const { return text_; }
    void set_text(std::string text);

    const Font &font() const { return font_; }
    void set_font(Font font);

    Color color() const { return color_; }
    void set_color(Color c);

    TextAlignment text_alignment() const { return text_align_; }
    void set_text_alignment(TextAlignment align);

    Size measure(Size available) override;

protected:
    void on_paint(Painter &painter) override;

private:
    std::string text_;
    Font font_;
    Color color_;
    bool has_custom_color_{false};
    TextAlignment text_align_{TextAlignment::Left};
};

} // namespace SzpontUI
