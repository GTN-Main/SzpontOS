#pragma once

#include <SzpontUI/Font/Font.hpp>
#include <string_view>
#include <vector>
#include <cstdint>

namespace SzpontUI {

struct ShapedGlyph {
    uint32_t glyph_index{0};
    uint32_t cluster{0};
    int x_offset{0};
    int y_offset{0};
    int x_advance{0};
    int y_advance{0};
};

struct ShapedRun {
    std::vector<ShapedGlyph> glyphs;
    int total_width{0};
    FontMetrics metrics;
};

class TextShaper {
public:
    static ShapedRun shape_text(std::string_view text, const Font &font);
};

} // namespace SzpontUI
