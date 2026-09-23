#pragma once

#include <SzpontUI/Theme/Theme.hpp>

namespace SzpontUI {

class SzpontTheme : public Theme {
public:
    Color background() const override { return Color::from_hex("#101726"); }
    Color surface() const override { return Color::from_hex("#172033"); }
    Color surface_hover() const override { return Color::from_hex("#1e2a42"); }
    Color surface_active() const override { return Color::from_hex("#253554"); }
    Color surface_border() const override { return Color::from_hex("#28344e"); }

    Color accent() const override { return Color::from_hex("#5b9cf8"); }
    Color accent_hover() const override { return Color::from_hex("#7aaef9"); }
    Color accent_active() const override { return Color::from_hex("#4182e4"); }
    Color accent_text() const override { return Color::from_hex("#ffffff"); }

    Color text_primary() const override { return Color::from_hex("#ffffff"); }
    Color text_secondary() const override { return Color::from_hex("#94a3b8"); }
    Color text_muted() const override { return Color::from_hex("#64748b"); }

    Color control_bg() const override { return Color::from_hex("#1b2436"); }
    Color control_border() const override { return Color::from_hex("#28344e"); }
    Color control_hover() const override { return Color::from_hex("#27334b"); }

    Color success() const override { return Color::from_hex("#28c840"); }
    Color warning() const override { return Color::from_hex("#febc2e"); }
    Color danger() const override { return Color::from_hex("#ff5f57"); }

    int corner_radius_card() const override { return 10; }
    int corner_radius_button() const override { return 6; }
    int corner_radius_input() const override { return 6; }

    Font default_font() const override {
        return Font("Inter", 13, FontWeight::Regular);
    }

    Font title_font() const override {
        return Font("Inter", 16, FontWeight::Bold);
    }

    Font caption_font() const override {
        return Font("Inter", 11, FontWeight::Regular);
    }

    Font mono_font() const override {
        return Font("DejaVu Sans Mono", 12, FontWeight::Regular);
    }
};

} // namespace SzpontUI
