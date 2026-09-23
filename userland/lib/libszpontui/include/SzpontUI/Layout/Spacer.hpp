#pragma once

#include <SzpontUI/Layout/Layout.hpp>

namespace SzpontUI {

class Spacer : public LayoutItem {
public:
    explicit Spacer(int stretch = 1) : stretch_(stretch) {}

    Size measure(Size) override {
        return Size{0, 0};
    }

    void arrange(Rect bounds) override {
        bounds_ = bounds;
    }

    SizePolicy horizontal_policy() const override { return SizePolicy::Expanding; }
    SizePolicy vertical_policy() const override { return SizePolicy::Expanding; }
    int stretch_factor() const override { return stretch_; }

private:
    int stretch_{1};
    Rect bounds_;
};

} // namespace SzpontUI
