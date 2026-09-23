#pragma once

#include <SzpontUI/Core/Color.hpp>
#include <SzpontUI/Font/Font.hpp>
#include <memory>

namespace SzpontUI {

class Theme {
public:
    virtual ~Theme() = default;

    virtual Color background() const = 0;
    virtual Color surface() const = 0;
    virtual Color surface_hover() const = 0;
    virtual Color surface_active() const = 0;
    virtual Color surface_border() const = 0;

    virtual Color accent() const = 0;
    virtual Color accent_hover() const = 0;
    virtual Color accent_active() const = 0;
    virtual Color accent_text() const = 0;

    virtual Color text_primary() const = 0;
    virtual Color text_secondary() const = 0;
    virtual Color text_muted() const = 0;

    virtual Color control_bg() const = 0;
    virtual Color control_border() const = 0;
    virtual Color control_hover() const = 0;

    virtual Color success() const = 0;
    virtual Color warning() const = 0;
    virtual Color danger() const = 0;

    virtual int corner_radius_card() const = 0;
    virtual int corner_radius_button() const = 0;
    virtual int corner_radius_input() const = 0;

    virtual Font default_font() const = 0;
    virtual Font title_font() const = 0;
    virtual Font caption_font() const = 0;
    virtual Font mono_font() const = 0;
};

class ThemeManager {
public:
    static ThemeManager &instance();

    Theme &current() { return *theme_; }
    void set_theme(std::shared_ptr<Theme> theme) { theme_ = theme; }

private:
    ThemeManager();
    std::shared_ptr<Theme> theme_;
};

inline Theme &current_theme() {
    return ThemeManager::instance().current();
}

} // namespace SzpontUI
