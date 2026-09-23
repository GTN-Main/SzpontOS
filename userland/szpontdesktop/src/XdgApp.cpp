#include "XdgApp.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <algorithm>
#include <cctype>

extern "C" char **environ;

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

std::string XdgApp::monogram() const {
    if (id == "szponter" || id == "file-manager") return "FM";
    if (id == "szpontview" || id == "image-viewer") return "IV";
    if (id == "szpontmon" || id == "system-monitor") return "SM";
    if (id == "szponterm" || id == "terminal" || id == "xterm") return ">_";
    if (id == "fastfetch") return "FF";
    if (id == "nano") return "ED";
    if (id == "top") return "TP";
    if (id == "sh") return "$_";

    if (name.empty()) return "??";
    if (name.length() == 1) return std::string(1, (char)toupper((unsigned char)name[0]));
    char c1 = (char)toupper((unsigned char)name[0]);
    char c2 = (char)toupper((unsigned char)name[1]);
    return std::string{c1, c2};
}

static std::string sanitize_exec(const std::string &exec_str) {
    std::string res;
    bool in_space = false;
    for (size_t i = 0; i < exec_str.length(); ++i) {
        if (exec_str[i] == '%' && i + 1 < exec_str.length() && isalpha(exec_str[i + 1])) {
            i++; // skip field code e.g. %f, %u, %F, %U
            continue;
        }
        char c = exec_str[i];
        if (c == ' ' || c == '\t') {
            if (!in_space && !res.empty()) {
                res += ' ';
                in_space = true;
            }
        } else {
            res += c;
            in_space = false;
        }
    }
    trim(res);
    return res;
}

static XdgApp parse_desktop_file(const std::string &filepath) {
    XdgApp app;
    app.desktop_file = filepath;

    size_t slash = filepath.find_last_of('/');
    std::string filename = (slash != std::string::npos) ? filepath.substr(slash + 1) : filepath;
    if (filename.length() > 8 && filename.substr(filename.length() - 8) == ".desktop") {
        app.id = filename.substr(0, filename.length() - 8);
    } else {
        app.id = filename;
    }

    FILE *f = fopen(filepath.c_str(), "r");
    if (!f) return app;

    char line[512];
    bool in_desktop_entry = false;

    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == ';' || *p == '\n' || *p == '\r' || *p == '\0') {
            continue;
        }

        if (*p == '[') {
            in_desktop_entry = (strncmp(p, "[Desktop Entry]", 15) == 0);
            continue;
        }

        if (!in_desktop_entry) continue;

        char *eq = strchr(p, '=');
        if (!eq) continue;

        *eq = '\0';
        std::string key = p;
        std::string val = eq + 1;
        trim(key);
        trim(val);

        if (key == "Name") {
            app.name = val;
        } else if (key == "GenericName") {
            app.generic_name = val;
        } else if (key == "Comment") {
            app.comment = val;
        } else if (key == "Exec") {
            app.exec = sanitize_exec(val);
        } else if (key == "Icon") {
            app.icon = val;
        } else if (key == "Categories") {
            app.categories = val;
        } else if (key == "Terminal") {
            app.terminal = (val == "true" || val == "1" || val == "yes");
        } else if (key == "NoDisplay") {
            app.no_display = (val == "true" || val == "1" || val == "yes");
        }
    }

    fclose(f);
    return app;
}

std::vector<XdgApp> XdgRegistry::scan_applications() {
    std::vector<XdgApp> apps;
    const char *dirs[] = {
        "/usr/share/applications",
        "/usr/local/share/applications"
    };

    for (const char *dir_path : dirs) {
        DIR *d = opendir(dir_path);
        if (!d) continue;

        struct dirent *ent;
        while ((ent = readdir(d)) != nullptr) {
            std::string name = ent->d_name;
            if (name.length() > 8 && name.substr(name.length() - 8) == ".desktop") {
                std::string full_path = std::string(dir_path) + "/" + name;
                XdgApp app = parse_desktop_file(full_path);
                if (!app.name.empty() && !app.exec.empty() && !app.no_display) {
                    // Check duplicates by id
                    bool exists = false;
                    for (const auto &existing : apps) {
                        if (existing.id == app.id) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        apps.push_back(app);
                    }
                }
            }
        }
        closedir(d);
    }

    // Sort alphabetically by name
    std::sort(apps.begin(), apps.end(), [](const XdgApp &a, const XdgApp &b) {
        return a.name < b.name;
    });

    return apps;
}

const XdgApp *XdgRegistry::find_by_id(const std::vector<XdgApp> &apps, const std::string &id) {
    for (const auto &app : apps) {
        if (app.id == id) return &app;
    }
    return nullptr;
}

pid_t XdgApp::launch(const std::string &terminal_bin) const {
    std::string cmd = exec;
    if (cmd.empty()) return -1;

    std::vector<std::string> args;

    if (terminal) {
        args.push_back(terminal_bin);
        args.push_back("-e");

        std::string cur;
        for (char c : cmd) {
            if (c == ' ' || c == '\t') {
                if (!cur.empty()) {
                    args.push_back(cur);
                    cur.clear();
                }
            } else {
                cur += c;
            }
        }
        if (!cur.empty()) args.push_back(cur);
    } else {
        std::string cur;
        for (char c : cmd) {
            if (c == ' ' || c == '\t') {
                if (!cur.empty()) {
                    args.push_back(cur);
                    cur.clear();
                }
            } else {
                cur += c;
            }
        }
        if (!cur.empty()) args.push_back(cur);
    }

    if (args.empty()) return -1;

    // Check executable access and fallbacks
    if (access(args[0].c_str(), X_OK) != 0) {
        size_t slash = args[0].find_last_of('/');
        std::string basename = (slash != std::string::npos) ? args[0].substr(slash + 1) : args[0];
        if (access(("/usr/bin/" + basename).c_str(), X_OK) == 0) {
            args[0] = "/usr/bin/" + basename;
        } else if (access(("/bin/" + basename).c_str(), X_OK) == 0) {
            args[0] = "/bin/" + basename;
        }
    }

    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        std::vector<char*> argv_ptrs;
        for (auto &a : args) {
            argv_ptrs.push_back(&a[0]);
        }
        argv_ptrs.push_back(nullptr);

        execve(argv_ptrs[0], argv_ptrs.data(), ::environ);
        _exit(127);
    }

    if (pid > 0) {
        setpgid(pid, pid);
    }

    return pid;
}

} // namespace SzpontDesktop
