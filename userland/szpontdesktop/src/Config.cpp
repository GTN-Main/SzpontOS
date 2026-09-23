#include "Config.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace SzpontDesktop {

static void trim(std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        s.clear();
        return;
    }
    size_t end = s.find_last_not_of(" \t\r\n");
    s = s.substr(start, end - start + 1);
}

static std::vector<std::string> split_csv(const std::string &s) {
    std::vector<std::string> res;
    std::string cur;
    for (char c : s) {
        if (c == ',') {
            trim(cur);
            if (!cur.empty()) res.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    trim(cur);
    if (!cur.empty()) res.push_back(cur);
    return res;
}

DesktopConfig DesktopConfig::load(const std::string &path) {
    DesktopConfig cfg;
    FILE *f = fopen(path.c_str(), "r");
    if (!f) {
        // Fallback to ~/.config/szpontdesktop.conf
        const char *home = getenv("HOME");
        if (home) {
            std::string user_cfg = std::string(home) + "/.config/szpontdesktop.conf";
            f = fopen(user_cfg.c_str(), "r");
        }
    }
    if (!f) return cfg;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == ';' || *p == '[' || *p == '\n' || *p == '\r' || *p == '\0') {
            continue;
        }

        char *eq = strchr(p, '=');
        if (!eq) continue;

        *eq = '\0';
        std::string key = p;
        std::string val = eq + 1;
        trim(key);
        trim(val);

        if (key == "wallpaper") {
            if (!val.empty()) cfg.wallpaper = val;
        } else if (key == "terminal") {
            if (!val.empty()) cfg.terminal = val;
        } else if (key == "dock_enabled") {
            cfg.dock_enabled = (val == "true" || val == "1" || val == "yes");
        } else if (key == "panel_position") {
            if (!val.empty()) cfg.panel_position = val;
        } else if (key == "panel_height") {
            cfg.panel_height = atoi(val.c_str());
            if (cfg.panel_height < 24) cfg.panel_height = 24;
            if (cfg.panel_height > 64) cfg.panel_height = 64;
        } else if (key == "dock_pinned") {
            cfg.dock_pinned = split_csv(val);
        } else if (key == "autostart") {
            cfg.autostart = split_csv(val);
        } else if (key == "window_radius") {
            cfg.window_radius = atoi(val.c_str());
            if (cfg.window_radius < 0) cfg.window_radius = 0;
            if (cfg.window_radius > 24) cfg.window_radius = 24;
        } else if (key == "border_width") {
            cfg.border_width = atoi(val.c_str());
            if (cfg.border_width < 0) cfg.border_width = 0;
            if (cfg.border_width > 4) cfg.border_width = 4;
        }
    }

    fclose(f);
    return cfg;
}

} // namespace SzpontDesktop
