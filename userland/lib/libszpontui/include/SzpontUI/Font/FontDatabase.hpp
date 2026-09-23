#pragma once

#include <SzpontUI/Font/Font.hpp>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <cstdint>

// Forward declarations for FreeType & HarfBuzz handles
typedef struct FT_LibraryRec_* FT_Library;
typedef struct FT_FaceRec_* FT_Face;
typedef struct hb_font_t hb_font_t;

namespace SzpontUI {

struct FontFaceHandle {
    FT_Face ft_face{nullptr};
    hb_font_t *hb_font{nullptr};
    std::vector<uint8_t> file_data;
    FontMetrics metrics;
    int current_size{0};
};

class FontDatabase {
public:
    static FontDatabase &instance();

    bool initialize();
    void shutdown();

    std::shared_ptr<FontFaceHandle> get_face(const Font &font);
    FontMetrics get_metrics(const Font &font);

    void add_font_path(const std::string &path);

private:
    FontDatabase();
    ~FontDatabase();

    std::string resolve_font_path(const Font &font);

    FT_Library ft_library_{nullptr};
    std::vector<std::string> search_paths_;
    std::map<std::string, std::shared_ptr<FontFaceHandle>> face_cache_;
    bool initialized_{false};
};

} // namespace SzpontUI
