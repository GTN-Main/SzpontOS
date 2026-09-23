#pragma once

#include <SzpontUI/Font/Font.hpp>
#include <cstdint>
#include <vector>
#include <memory>
#include <map>

namespace SzpontUI {

struct GlyphBitmap {
    int width{0};
    int height{0};
    int stride{0}; // 4-byte aligned rowstride in bytes for Pixman
    int bearing_x{0};
    int bearing_y{0};
    std::vector<uint32_t> buffer; // 32-bit aligned buffer
};

class GlyphCache {
public:
    static GlyphCache &instance();

    std::shared_ptr<GlyphBitmap> get_glyph(const Font &font, uint32_t glyph_index);

    void clear();

private:
    GlyphCache() = default;

    struct GlyphKey {
        std::string family;
        int size{0};
        FontWeight weight{FontWeight::Regular};
        uint32_t glyph_index{0};

        bool operator<(const GlyphKey &o) const {
            if (glyph_index != o.glyph_index) return glyph_index < o.glyph_index;
            if (size != o.size) return size < o.size;
            if (weight != o.weight) return weight < o.weight;
            return family < o.family;
        }
    };

    std::map<GlyphKey, std::shared_ptr<GlyphBitmap>> cache_;
};

} // namespace SzpontUI
