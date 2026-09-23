#pragma once

#include <SzpontUI/Core/Geometry.hpp>
#include <vector>
#include <memory>

namespace SzpontUI {

class Widget;

enum class Alignment {
    Start,
    Center,
    End,
    Stretch
};

enum class SizePolicy {
    Fixed,
    Preferred,
    Expanding
};

class LayoutItem {
public:
    virtual ~LayoutItem() = default;

    virtual Size measure(Size available) = 0;
    virtual void arrange(Rect bounds) = 0;
    virtual SizePolicy horizontal_policy() const = 0;
    virtual SizePolicy vertical_policy() const = 0;
    virtual int stretch_factor() const { return 0; }
};

class Layout : public LayoutItem {
public:
    virtual ~Layout() = default;

    void set_padding(Insets insets) { padding_ = insets; }
    void set_padding(int all) { padding_ = Insets(all); }
    Insets padding() const { return padding_; }

    void set_spacing(int spacing) { spacing_ = spacing; }
    int spacing() const { return spacing_; }

    void set_owner(Widget *owner) {
        owner_ = owner;
        on_owner_changed();
    }
    Widget *owner() const { return owner_; }
    virtual void on_owner_changed() {}

    virtual void add_widget(std::shared_ptr<Widget> widget, int stretch = 0) = 0;
    virtual void add_item(std::shared_ptr<LayoutItem> item) = 0;
    virtual void add_spacer(int stretch = 1) = 0;

    SizePolicy horizontal_policy() const override { return SizePolicy::Preferred; }
    SizePolicy vertical_policy() const override { return SizePolicy::Preferred; }

protected:
    Widget *owner_{nullptr};
    Insets padding_{8};
    int spacing_{6};
};

} // namespace SzpontUI
