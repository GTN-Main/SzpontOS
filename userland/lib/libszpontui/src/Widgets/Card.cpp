#include <SzpontUI/Widgets/Card.hpp>
#include <SzpontUI/Theme/Theme.hpp>

namespace SzpontUI {

Card::Card() : Widget("Card") {
    min_size_ = Size{100, 60};
}

Size Card::measure(Size available) {
    if (layout_) {
        return layout_->measure(available);
    }
    return min_size_;
}

void Card::on_paint(Painter &painter) {
    Rect r{0, 0, bounds_.width, bounds_.height};
    int radius = current_theme().corner_radius_card();

    // Card background
    painter.fill_rounded_rect(r, radius, current_theme().surface());

    // 1px Border
    painter.draw_rounded_rect(r, radius, current_theme().surface_border(), 1);
}

} // namespace SzpontUI
