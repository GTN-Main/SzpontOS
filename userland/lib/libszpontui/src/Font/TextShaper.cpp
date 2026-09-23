#include <SzpontUI/Font/TextShaper.hpp>
#include <SzpontUI/Font/FontDatabase.hpp>
#include <hb.h>
#include <unordered_map>
#include <string>

namespace SzpontUI {

struct ShapeCacheKey {
    std::string text;
    std::string family;
    int size{0};
    FontWeight weight{FontWeight::Regular};

    bool operator==(const ShapeCacheKey &o) const {
        return size == o.size && weight == o.weight && text == o.text && family == o.family;
    }
};

struct ShapeCacheHash {
    size_t operator()(const ShapeCacheKey &k) const {
        size_t h1 = std::hash<std::string>{}(k.text);
        size_t h2 = std::hash<std::string>{}(k.family);
        size_t h3 = std::hash<int>{}(k.size);
        size_t h4 = std::hash<int>{}(static_cast<int>(k.weight));
        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
    }
};

static std::unordered_map<ShapeCacheKey, ShapedRun, ShapeCacheHash> s_shape_cache;

ShapedRun TextShaper::shape_text(std::string_view text, const Font &font) {
    ShapedRun run;
    if (text.empty()) return run;

    ShapeCacheKey key{std::string(text), font.family(), font.size(), font.weight()};
    auto it = s_shape_cache.find(key);
    if (it != s_shape_cache.end()) {
        return it->second;
    }

    auto handle = FontDatabase::instance().get_face(font);
    if (!handle || !handle->hb_font) {
        // Fallback: simple character width estimate if no font is available
        run.metrics = FontDatabase::instance().get_metrics(font);
        int x_adv = font.size() * 3 / 5;
        for (size_t i = 0; i < text.size(); ++i) {
            ShapedGlyph g;
            g.glyph_index = static_cast<uint32_t>(text[i]);
            g.x_advance = x_adv;
            run.glyphs.push_back(g);
            run.total_width += x_adv;
        }
        if (s_shape_cache.size() > 1024) s_shape_cache.clear();
        s_shape_cache[key] = run;
        return run;
    }

    run.metrics = handle->metrics;

    hb_buffer_t *buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, text.data(), static_cast<int>(text.size()), 0, static_cast<int>(text.size()));
    hb_buffer_guess_segment_properties(buf);

    hb_shape(handle->hb_font, buf, nullptr, 0);

    unsigned int glyph_count = 0;
    hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

    run.glyphs.reserve(glyph_count);
    int current_x = 0;

    for (unsigned int i = 0; i < glyph_count; ++i) {
        ShapedGlyph sg;
        sg.glyph_index = glyph_info[i].codepoint;
        sg.cluster = glyph_info[i].cluster;
        // HarfBuzz FreeType positions are in 26.6 fractional pixels
        sg.x_offset = glyph_pos[i].x_offset >> 6;
        sg.y_offset = glyph_pos[i].y_offset >> 6;
        sg.x_advance = glyph_pos[i].x_advance >> 6;
        sg.y_advance = glyph_pos[i].y_advance >> 6;

        run.glyphs.push_back(sg);
        current_x += sg.x_advance;
    }

    run.total_width = current_x;

    hb_buffer_destroy(buf);

    if (s_shape_cache.size() > 1024) s_shape_cache.clear();
    s_shape_cache[key] = run;

    return run;
}

} // namespace SzpontUI

