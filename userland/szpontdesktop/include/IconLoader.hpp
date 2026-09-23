#pragma once

#include <SzpontUI/SzpontUI.hpp>
#include <string>
#include <unordered_map>
#include <memory>

namespace SzpontDesktop {

class IconLoader {
public:
    static IconLoader &instance();

    // Returns a pointer to a cached BitmapSurface icon of target_size x target_size.
    // If not found, generates a modern fallback squircle icon.
    const SzpontUI::BitmapSurface *get_icon(const std::string &icon_name_or_path,
                                           const std::string &app_id,
                                           const std::string &app_name,
                                           int target_size = 48);

private:
    IconLoader() = default;

    std::unordered_map<std::string, SzpontUI::BitmapSurface> cache_;

    std::string resolve_icon_path(const std::string &icon_name, const std::string &app_id);
    bool load_and_scale(const std::string &path, int target_size, SzpontUI::BitmapSurface &out_surface);
    void generate_fallback(const std::string &app_id, const std::string &name, int target_size,
                           SzpontUI::BitmapSurface &out_surface);
};

} // namespace SzpontDesktop
