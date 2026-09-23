#pragma once

#include <SzpontUI/Layout/Layout.hpp>
#include <SzpontUI/Layout/Spacer.hpp>
#include <vector>
#include <memory>

namespace SzpontUI {

enum class Orientation {
    Horizontal,
    Vertical
};

class BoxLayout : public Layout {
public:
    explicit BoxLayout(Orientation orientation);

    void add_widget(std::shared_ptr<Widget> widget, int stretch = 0) override;
    void add_item(std::shared_ptr<LayoutItem> item) override;
    void add_spacer(int stretch = 1) override;

    Size measure(Size available) override;
    void arrange(Rect bounds) override;
    void on_owner_changed() override;

    void set_alignment(Alignment align) { alignment_ = align; }
    Alignment alignment() const { return alignment_; }

private:
    struct Entry {
        std::shared_ptr<LayoutItem> item;
        int stretch{0};
    };

    Orientation orientation_;
    Alignment alignment_{Alignment::Stretch};
    std::vector<Entry> entries_;
};

class VBoxLayout : public BoxLayout {
public:
    VBoxLayout() : BoxLayout(Orientation::Vertical) {}
};

class HBoxLayout : public BoxLayout {
public:
    HBoxLayout() : BoxLayout(Orientation::Horizontal) {}
};

} // namespace SzpontUI
