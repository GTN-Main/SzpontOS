#pragma once

#include <string>
#include <cstdint>

namespace SzpontUI {

enum class FontWeight {
    Light,
    Regular,
    Medium,
    SemiBold,
    Bold,
    ExtraBold
};

enum class FontStyle {
    Normal,
    Italic
};

struct FontMetrics {
    int ascent{0};
    int descent{0};
    int line_gap{0};
    int height{0};
};

class Font {
public:
    Font() : family_("Inter"), size_(13), weight_(FontWeight::Regular), style_(FontStyle::Normal) {}
    Font(std::string family, int size, FontWeight weight = FontWeight::Regular, FontStyle style = FontStyle::Normal)
        : family_(std::move(family)), size_(size), weight_(weight), style_(style) {}

    const std::string &family() const { return family_; }
    int size() const { return size_; }
    FontWeight weight() const { return weight_; }
    FontStyle style() const { return style_; }

    Font with_size(int new_size) const {
        return Font(family_, new_size, weight_, style_);
    }

    Font with_weight(FontWeight new_weight) const {
        return Font(family_, size_, new_weight, style_);
    }

    bool is_bold() const {
        return weight_ == FontWeight::Bold || weight_ == FontWeight::ExtraBold;
    }

    bool is_italic() const {
        return style_ == FontStyle::Italic;
    }

    bool operator==(const Font &o) const {
        return size_ == o.size_ && weight_ == o.weight_ && style_ == o.style_ && family_ == o.family_;
    }
    bool operator!=(const Font &o) const { return !(*this == o); }

private:
    std::string family_;
    int size_{13};
    FontWeight weight_{FontWeight::Regular};
    FontStyle style_{FontStyle::Normal};
};

} // namespace SzpontUI
