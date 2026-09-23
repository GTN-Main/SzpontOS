#include <SzpontUI/Font/GlyphCache.hpp>
#include <SzpontUI/Font/FontDatabase.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace SzpontUI {

GlyphCache &GlyphCache::instance() {
    static GlyphCache instance;
    return instance;
}

void GlyphCache::clear() {
    cache_.clear();
}

std::shared_ptr<GlyphBitmap> GlyphCache::get_glyph(const Font &font, uint32_t glyph_index) {
    GlyphKey key{font.family(), font.size(), font.weight(), glyph_index};
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        return it->second;
    }

    auto handle = FontDatabase::instance().get_face(font);
    if (!handle || !handle->ft_face) {
        return nullptr;
    }

    FT_Error err = FT_Load_Glyph(handle->ft_face, glyph_index, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL);
    if (err) {
        return nullptr;
    }

    FT_GlyphSlot slot = handle->ft_face->glyph;
    auto glyph = std::make_shared<GlyphBitmap>();
    glyph->width = slot->bitmap.width;
    glyph->height = slot->bitmap.rows;
    glyph->bearing_x = slot->bitmap_left;
    glyph->bearing_y = slot->bitmap_top;
    glyph->stride = (glyph->width + 3) & ~3; // Must be 4-byte aligned for Pixman

    if (glyph->width > 0 && glyph->height > 0 && slot->bitmap.buffer) {
        int words_count = (glyph->stride / 4) * glyph->height;
        glyph->buffer.assign(words_count, 0);
        uint8_t *dst = reinterpret_cast<uint8_t*>(glyph->buffer.data());
        const uint8_t *src = slot->bitmap.buffer;
        int src_pitch = slot->bitmap.pitch;

        for (int y = 0; y < glyph->height; ++y) {
            std::copy(src + y * src_pitch, src + y * src_pitch + glyph->width, dst + y * glyph->stride);
        }
    }

    cache_[key] = glyph;
    return glyph;
}

} // namespace SzpontUI
