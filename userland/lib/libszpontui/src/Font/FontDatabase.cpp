#include <SzpontUI/Font/FontDatabase.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
#include <cstdio>
#include <fstream>
#include <unistd.h>

namespace SzpontUI {

FontDatabase &FontDatabase::instance() {
    static FontDatabase instance;
    return instance;
}

FontDatabase::FontDatabase() {
    search_paths_ = {
        "/usr/share/fonts/truetype/szpont",
        "/usr/share/fonts/truetype",
        "/usr/share/fonts"
    };
    initialize();
}

FontDatabase::~FontDatabase() {
    shutdown();
}

bool FontDatabase::initialize() {
    if (initialized_) return true;

    FT_Error err = FT_Init_FreeType(&ft_library_);
    if (err) {
        fprintf(stderr, "[SzpontUI] Failed to initialize FreeType library (err=%d)\n", err);
        return false;
    }

    initialized_ = true;
    return true;
}

void FontDatabase::shutdown() {
    if (!initialized_) return;

    for (auto &pair : face_cache_) {
        if (pair.second) {
            if (pair.second->hb_font) {
                hb_font_destroy(pair.second->hb_font);
                pair.second->hb_font = nullptr;
            }
            if (pair.second->ft_face) {
                FT_Done_Face(pair.second->ft_face);
                pair.second->ft_face = nullptr;
            }
        }
    }
    face_cache_.clear();

    if (ft_library_) {
        FT_Done_FreeType(ft_library_);
        ft_library_ = nullptr;
    }

    initialized_ = false;
}

void FontDatabase::add_font_path(const std::string &path) {
    search_paths_.insert(search_paths_.begin(), path);
}

std::string FontDatabase::resolve_font_path(const Font &font) {
    std::vector<std::string> candidates;

    if (font.family() == "DejaVu Sans Mono" || font.family() == "monospace") {
        candidates.push_back("DejaVuSansMono.ttf");
    } else if (font.is_bold()) {
        candidates.push_back("Inter-Bold.ttf");
        candidates.push_back("DejaVuSansMono.ttf");
    } else {
        candidates.push_back("Inter-Regular.ttf");
        candidates.push_back("DejaVuSansMono.ttf");
    }

    for (const auto &dir : search_paths_) {
        for (const auto &file : candidates) {
            std::string full_path = dir + "/" + file;
            if (access(full_path.c_str(), R_OK) == 0) {
                return full_path;
            }
        }
    }

    return "";
}

std::shared_ptr<FontFaceHandle> FontDatabase::get_face(const Font &font) {
    if (!initialized_ && !initialize()) return nullptr;

    std::string cache_key = font.family() + (font.is_bold() ? ":bold:" : ":regular:") + std::to_string(font.size());
    auto it = face_cache_.find(cache_key);
    if (it != face_cache_.end()) {
        return it->second;
    }

    std::string path = resolve_font_path(font);
    if (path.empty()) {
        fprintf(stderr, "[SzpontUI] Font '%s' not found in search paths\n", font.family().c_str());
        return nullptr;
    }

    auto handle = std::make_shared<FontFaceHandle>();
    FT_Error err = FT_New_Face(ft_library_, path.c_str(), 0, &handle->ft_face);
    if (err) {
        fprintf(stderr, "[SzpontUI] FT_New_Face failed for '%s' (err=%d)\n", path.c_str(), err);
        return nullptr;
    }

    FT_Set_Pixel_Sizes(handle->ft_face, 0, font.size());
    handle->current_size = font.size();

    // Create HarfBuzz font wrapper around FreeType face
    handle->hb_font = hb_ft_font_create(handle->ft_face, nullptr);

    // Compute metrics
    FT_Size_Metrics sm = handle->ft_face->size->metrics;
    handle->metrics.ascent = sm.ascender >> 6;
    handle->metrics.descent = -(sm.descender >> 6);
    handle->metrics.height = sm.height >> 6;
    handle->metrics.line_gap = handle->metrics.height - (handle->metrics.ascent + handle->metrics.descent);

    face_cache_[cache_key] = handle;
    return handle;
}

FontMetrics FontDatabase::get_metrics(const Font &font) {
    auto handle = get_face(font);
    if (handle) {
        return handle->metrics;
    }
    // Fallback metrics
    FontMetrics m;
    m.ascent = font.size();
    m.descent = font.size() / 4;
    m.height = m.ascent + m.descent;
    m.line_gap = 2;
    return m;
}

} // namespace SzpontUI
