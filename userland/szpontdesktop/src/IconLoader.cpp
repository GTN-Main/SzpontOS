#include "IconLoader.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>
#include <cctype>

#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#include "stb_image.h"

namespace SzpontDesktop {

using namespace SzpontUI;

IconLoader &IconLoader::instance() {
    static IconLoader s_instance;
    return s_instance;
}

static unsigned char *read_file_to_buffer(const char *path, size_t *out_size) {
    if (!path || !path[0]) return nullptr;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return nullptr;

    off_t size = lseek(fd, 0, SEEK_END);
    if (size <= 0) {
        close(fd);
        return nullptr;
    }
    lseek(fd, 0, SEEK_SET);

    unsigned char *buf = (unsigned char *)malloc((size_t)size);
    if (!buf) {
        close(fd);
        return nullptr;
    }

    size_t total_read = 0;
    while (total_read < (size_t)size) {
        ssize_t n = read(fd, buf + total_read, (size_t)size - total_read);
        if (n <= 0) break;
        total_read += (size_t)n;
    }
    close(fd);

    if (total_read != (size_t)size) {
        free(buf);
        return nullptr;
    }

    *out_size = (size_t)size;
    return buf;
}

std::string IconLoader::resolve_icon_path(const std::string &icon_name, const std::string &app_id) {
    // 1. If absolute path and exists
    if (!icon_name.empty() && icon_name[0] == '/' && access(icon_name.c_str(), R_OK) == 0) {
        return icon_name;
    }

    std::vector<std::string> candidates;
    if (!icon_name.empty()) {
        candidates.push_back(icon_name);
        // Strip .png if already included
        if (icon_name.length() > 4 && icon_name.substr(icon_name.length() - 4) == ".png") {
            candidates.push_back(icon_name.substr(0, icon_name.length() - 4));
        }
    }
    if (!app_id.empty() && app_id != icon_name) {
        candidates.push_back(app_id);
    }

    // Standard XDG icon search locations
    static const char *s_search_dirs[] = {
        "/usr/share/icons/hicolor/48x48/apps",
        "/usr/share/icons/hicolor/64x64/apps",
        "/usr/share/icons/hicolor/32x32/apps",
        "/usr/share/icons",
        "/usr/share/pixmaps",
        "/usr/share/artwork"
    };

    for (const auto &cand : candidates) {
        for (const char *dir : s_search_dirs) {
            std::string p1 = std::string(dir) + "/" + cand + ".png";
            if (access(p1.c_str(), R_OK) == 0) return p1;

            std::string p2 = std::string(dir) + "/" + cand + ".jpg";
            if (access(p2.c_str(), R_OK) == 0) return p2;

            std::string p3 = std::string(dir) + "/" + cand;
            if (access(p3.c_str(), R_OK) == 0) return p3;
        }
    }

    return "";
}

bool IconLoader::load_and_scale(const std::string &path, int target_size, BitmapSurface &out_surface) {
    size_t file_size = 0;
    unsigned char *file_buf = read_file_to_buffer(path.c_str(), &file_size);
    if (!file_buf) return false;

    int src_w = 0, src_h = 0, channels = 0;
    unsigned char *img_data = stbi_load_from_memory(file_buf, (int)file_size, &src_w, &src_h, &channels, 4);
    free(file_buf);

    if (!img_data || src_w <= 0 || src_h <= 0) {
        return false;
    }

    out_surface.resize(target_size, target_size);
    out_surface.clear(Color::transparent());

    // Bilinear or nearest scaling into target_size x target_size ARGB surface
    const uint32_t *src32 = (const uint32_t *)img_data;

    for (int dy = 0; dy < target_size; ++dy) {
        int sy = (dy * src_h) / target_size;
        if (sy >= src_h) sy = src_h - 1;
        const uint32_t *src_row = src32 + sy * src_w;

        for (int dx = 0; dx < target_size; ++dx) {
            int sx = (dx * src_w) / target_size;
            if (sx >= src_w) sx = src_w - 1;

            uint32_t rgba = src_row[sx];
            uint32_t r = rgba & 0xFF;
            uint32_t g = (rgba >> 8) & 0xFF;
            uint32_t b = (rgba >> 16) & 0xFF;
            uint32_t a = (rgba >> 24) & 0xFF;

            // Premultiply RGB by alpha for PIXMAN_a8r8g8b8 OP_OVER
            if (a == 0) {
                r = g = b = 0;
            } else if (a < 255) {
                r = (r * a) / 255;
                g = (g * a) / 255;
                b = (b * a) / 255;
            }

            // ARGB for pixman / Painter
            out_surface.set_pixel(dx, dy, Color((uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a));
        }
    }

    stbi_image_free(img_data);
    return true;
}

void IconLoader::generate_fallback(const std::string &app_id, const std::string &name, int target_size,
                                  BitmapSurface &out_surface) {
    out_surface.resize(target_size, target_size);
    out_surface.clear(Color::transparent());

    Painter painter(out_surface);
    int r = target_size / 4;
    Rect bounds{0, 0, target_size, target_size};

    // Subdued obsidian / slate squircle
    painter.fill_rounded_rect(bounds, r, Color(28, 34, 46));
    painter.draw_rounded_rect(bounds, r, Color(255, 255, 255, 35), 1);

    // ASCII monogram
    std::string mono;
    if (app_id == "szponterm" || app_id == "terminal" || app_id == "sh") mono = ">_";
    else if (!name.empty() && name.length() >= 2) {
        char c1 = (char)toupper((unsigned char)name[0]);
        char c2 = (char)toupper((unsigned char)name[1]);
        mono = std::string{c1, c2};
    } else if (!app_id.empty() && app_id.length() >= 2) {
        char c1 = (char)toupper((unsigned char)app_id[0]);
        char c2 = (char)toupper((unsigned char)app_id[1]);
        mono = std::string{c1, c2};
    } else {
        mono = "APP";
    }

    int font_sz = (target_size >= 48) ? 14 : ((target_size >= 32) ? 11 : 9);
    painter.draw_text(bounds, mono, Font("Inter", font_sz, FontWeight::Bold),
                      Color(241, 245, 249), TextAlignment::Center, VerticalAlignment::Center);
}

const BitmapSurface *IconLoader::get_icon(const std::string &icon_name_or_path,
                                         const std::string &app_id,
                                         const std::string &app_name,
                                         int target_size) {
    char key_buf[128];
    snprintf(key_buf, sizeof(key_buf), "%s:%s:%d", icon_name_or_path.c_str(), app_id.c_str(), target_size);
    std::string cache_key = key_buf;

    auto it = cache_.find(cache_key);
    if (it != cache_.end()) {
        return &it->second;
    }

    std::string resolved = resolve_icon_path(icon_name_or_path, app_id);
    BitmapSurface surface;
    bool loaded = false;

    if (!resolved.empty()) {
        loaded = load_and_scale(resolved, target_size, surface);
    }

    if (!loaded) {
        generate_fallback(app_id, app_name, target_size, surface);
    }

    auto ins = cache_.emplace(cache_key, std::move(surface));
    return &ins.first->second;
}

} // namespace SzpontDesktop
