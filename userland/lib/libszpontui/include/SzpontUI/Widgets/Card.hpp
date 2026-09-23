#pragma once

#include <SzpontUI/Widgets/Widget.hpp>

namespace SzpontUI {

class Card : public Widget {
public:
    explicit Card();

    Size measure(Size available) override;

protected:
    void on_paint(Painter &painter) override;
};

} // namespace SzpontUI
