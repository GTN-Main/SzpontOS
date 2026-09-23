#pragma once

#include <SzpontUI/SzpontUI.hpp>
#include <string>
#include <memory>
#include <X11/Xlib.h>

namespace SzpontDesktop {

class WallpaperManager {
public:
    static Pixmap apply_wallpaper(Display *dpy, Window root, int screen, int screen_w, int screen_h,
                                 const std::string &image_path);

    // Retained scaled wallpaper surface for real frosted-glass sampling
    static const SzpontUI::BitmapSurface *wallpaper_surface();
};

} // namespace SzpontDesktop
