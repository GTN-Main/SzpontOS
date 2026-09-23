#pragma once

#include <SzpontUI/Widgets/Widget.hpp>

namespace SzpontUI {

class ProgressBar : public Widget {
public:
    explicit ProgressBar(float value = 0.0f);

    float value() const { return value_; }
    void set_value(float val);

    Size measure(Size available) override;

protected:
    void on_paint(Painter &painter) override;

private:
    float value_{0.0f}; // 0.0 to 1.0
};

} // namespace SzpontUI
