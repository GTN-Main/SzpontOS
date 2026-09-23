#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <algorithm>

namespace SzpontUI {

class Color {
public:
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
    uint8_t a{255};

    constexpr Color() = default;
    constexpr Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}

    // Packed 32-bit ARGB (native Pixman & X11 standard: 0xAARRGGBB)
    constexpr uint32_t to_argb() const {
        return (static_cast<uint32_t>(a) << 24) |
               (static_cast<uint32_t>(r) << 16) |
               (static_cast<uint32_t>(g) << 8)  |
               (static_cast<uint32_t>(b));
    }

    constexpr uint32_t to_rgba() const {
        return (static_cast<uint32_t>(r) << 24) |
               (static_cast<uint32_t>(g) << 16) |
               (static_cast<uint32_t>(b) << 8)  |
               (static_cast<uint32_t>(a));
    }

    static constexpr Color from_argb(uint32_t argb) {
        return Color(
            static_cast<uint8_t>((argb >> 16) & 0xFF),
            static_cast<uint8_t>((argb >> 8) & 0xFF),
            static_cast<uint8_t>(argb & 0xFF),
            static_cast<uint8_t>((argb >> 24) & 0xFF)
        );
    }

    static Color from_hex(std::string_view hex) {
        if (hex.empty()) return Color(0, 0, 0, 255);
        if (hex[0] == '#') hex.remove_prefix(1);

        auto parse_hex_byte = [](std::string_view s) -> uint8_t {
            unsigned int v = 0;
            for (char c : s) {
                v <<= 4;
                if (c >= '0' && c <= '9') v |= (c - '0');
                else if (c >= 'a' && c <= 'f') v |= (c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') v |= (c - 'A' + 10);
            }
            return static_cast<uint8_t>(v);
        };

        if (hex.size() == 6) { // RRGGBB
            uint8_t r = parse_hex_byte(hex.substr(0, 2));
            uint8_t g = parse_hex_byte(hex.substr(2, 2));
            uint8_t b = parse_hex_byte(hex.substr(4, 2));
            return Color(r, g, b, 255);
        } else if (hex.size() == 8) { // RRGGBBAA
            uint8_t r = parse_hex_byte(hex.substr(0, 2));
            uint8_t g = parse_hex_byte(hex.substr(2, 2));
            uint8_t b = parse_hex_byte(hex.substr(4, 2));
            uint8_t a = parse_hex_byte(hex.substr(6, 2));
            return Color(r, g, b, a);
        } else if (hex.size() == 3) { // RGB
            char r_s[2] = {hex[0], hex[0]};
            char g_s[2] = {hex[1], hex[1]};
            char b_s[2] = {hex[2], hex[2]};
            return Color(parse_hex_byte(std::string_view(r_s, 2)),
                         parse_hex_byte(std::string_view(g_s, 2)),
                         parse_hex_byte(std::string_view(b_s, 2)), 255);
        }
        return Color(0, 0, 0, 255);
    }

    constexpr Color with_alpha(uint8_t new_alpha) const {
        return Color(r, g, b, new_alpha);
    }

    Color with_opacity(float opacity) const {
        float clamped = std::max(0.0f, std::min(1.0f, opacity));
        return Color(r, g, b, static_cast<uint8_t>(a * clamped));
    }

    static Color lerp(const Color &c1, const Color &c2, float t) {
        float f = std::max(0.0f, std::min(1.0f, t));
        uint8_t nr = static_cast<uint8_t>(c1.r + (c2.r - c1.r) * f);
        uint8_t ng = static_cast<uint8_t>(c1.g + (c2.g - c1.g) * f);
        uint8_t nb = static_cast<uint8_t>(c1.b + (c2.b - c1.b) * f);
        uint8_t na = static_cast<uint8_t>(c1.a + (c2.a - c1.a) * f);
        return Color(nr, ng, nb, na);
    }

    constexpr bool operator==(const Color &o) const {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }
    constexpr bool operator!=(const Color &o) const { return !(*this == o); }

    // Color presets
    static constexpr Color transparent() { return Color(0, 0, 0, 0); }
    static constexpr Color black()       { return Color(0, 0, 0, 255); }
    static constexpr Color white()       { return Color(255, 255, 255, 255); }
    static constexpr Color red()         { return Color(239, 68, 68, 255); }
    static constexpr Color green()       { return Color(34, 197, 94, 255); }
    static constexpr Color blue()        { return Color(59, 130, 246, 255); }
    static constexpr Color amber()       { return Color(245, 158, 11, 255); }
};

} // namespace SzpontUI
