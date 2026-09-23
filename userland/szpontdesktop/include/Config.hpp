#pragma once

#include <string>
#include <vector>

namespace SzpontDesktop {

struct DesktopConfig {
    std::string wallpaper{"/usr/share/artwork/wallpaper.jpg"};
    std::string terminal{"/usr/bin/szponterm"};
    bool dock_enabled{true};
    std::string panel_position{"top"};
    int panel_height{38};
    std::vector<std::string> dock_pinned{"szponterm", "makaljer", "szpontdetected", "fastfetch"};
    std::vector<std::string> autostart;
    int window_radius{8};
    int border_width{1};

    static DesktopConfig load(const std::string &path = "/etc/szpontdesktop.conf");
};

} // namespace SzpontDesktop
