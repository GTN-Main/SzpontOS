#include <SzpontUI/Layout/BoxLayout.hpp>
#include <SzpontUI/Widgets/Widget.hpp>
#include <numeric>

namespace SzpontUI {

BoxLayout::BoxLayout(Orientation orientation) : orientation_(orientation) {}

void BoxLayout::on_owner_changed() {
    if (!owner_) return;
    for (auto &entry : entries_) {
        if (auto w = std::dynamic_pointer_cast<Widget>(entry.item)) {
            owner_->add_child(w);
        } else if (auto sublayout = std::dynamic_pointer_cast<Layout>(entry.item)) {
            sublayout->set_owner(owner_);
        }
    }
}

void BoxLayout::add_widget(std::shared_ptr<Widget> widget, int stretch) {
    if (!widget) return;
    entries_.push_back({widget, stretch});
    if (owner_) {
        owner_->add_child(widget);
    }
}

void BoxLayout::add_item(std::shared_ptr<LayoutItem> item) {
    if (!item) return;
    entries_.push_back({item, item->stretch_factor()});
    if (owner_) {
        if (auto w = std::dynamic_pointer_cast<Widget>(item)) {
            owner_->add_child(w);
        } else if (auto sublayout = std::dynamic_pointer_cast<Layout>(item)) {
            sublayout->set_owner(owner_);
        }
    }
}

void BoxLayout::add_spacer(int stretch) {
    auto spacer = std::make_shared<Spacer>(stretch);
    entries_.push_back({spacer, stretch});
}

Size BoxLayout::measure(Size available) {
    if (entries_.empty()) {
        return Size{padding_.horizontal(), padding_.vertical()};
    }

    int major_total = 0;
    int minor_max = 0;
    int visible_count = 0;

    for (const auto &entry : entries_) {
        Size sz = entry.item->measure(available);
        if (orientation_ == Orientation::Vertical) {
            major_total += sz.height;
            minor_max = std::max(minor_max, sz.width);
        } else {
            major_total += sz.width;
            minor_max = std::max(minor_max, sz.height);
        }
        visible_count++;
    }

    if (visible_count > 1) {
        major_total += (visible_count - 1) * spacing_;
    }

    if (orientation_ == Orientation::Vertical) {
        return Size{minor_max + padding_.horizontal(), major_total + padding_.vertical()};
    } else {
        return Size{major_total + padding_.horizontal(), minor_max + padding_.vertical()};
    }
}

void BoxLayout::arrange(Rect bounds) {
    if (entries_.empty()) return;

    Rect inner = bounds.shrunk_by(padding_);
    if (inner.is_empty()) return;

    int total_stretch = 0;
    int fixed_major = 0;
    int count = static_cast<int>(entries_.size());

    std::vector<Size> measured_sizes;
    measured_sizes.reserve(count);

    for (const auto &entry : entries_) {
        Size sz = entry.item->measure(inner.size());
        measured_sizes.push_back(sz);

        int stretch = entry.stretch;
        if (stretch == 0 && entry.item->stretch_factor() > 0) {
            stretch = entry.item->stretch_factor();
        }

        if (stretch > 0) {
            total_stretch += stretch;
        } else {
            fixed_major += (orientation_ == Orientation::Vertical) ? sz.height : sz.width;
        }
    }

    int total_spacing = (count > 1) ? (count - 1) * spacing_ : 0;
    int available_major = (orientation_ == Orientation::Vertical ? inner.height : inner.width) - fixed_major - total_spacing;
    available_major = std::max(0, available_major);

    int current_major = (orientation_ == Orientation::Vertical) ? inner.y : inner.x;

    for (size_t i = 0; i < entries_.size(); ++i) {
        const auto &entry = entries_[i];
        Size sz = measured_sizes[i];

        int item_major = 0;
        int stretch = entry.stretch;
        if (stretch == 0 && entry.item->stretch_factor() > 0) {
            stretch = entry.item->stretch_factor();
        }

        if (total_stretch > 0 && stretch > 0) {
            item_major = (available_major * stretch) / total_stretch;
        } else {
            item_major = (orientation_ == Orientation::Vertical) ? sz.height : sz.width;
        }

        Rect item_rect;
        if (orientation_ == Orientation::Vertical) {
            int item_x = inner.x;
            int item_w = inner.width;
            if (alignment_ == Alignment::Center) {
                item_w = std::min(inner.width, sz.width);
                item_x = inner.x + (inner.width - item_w) / 2;
            } else if (alignment_ == Alignment::Start) {
                item_w = std::min(inner.width, sz.width);
            }
            item_rect = Rect{item_x, current_major, item_w, item_major};
            current_major += item_major + spacing_;
        } else {
            int item_y = inner.y;
            int item_h = inner.height;
            if (alignment_ == Alignment::Center) {
                item_h = std::min(inner.height, sz.height);
                item_y = inner.y + (inner.height - item_h) / 2;
            } else if (alignment_ == Alignment::Start) {
                item_h = std::min(inner.height, sz.height);
            }
            item_rect = Rect{current_major, item_y, item_major, item_h};
            current_major += item_major + spacing_;
        }

        entry.item->arrange(item_rect);
    }
}

} // namespace SzpontUI
