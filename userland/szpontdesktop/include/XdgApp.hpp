#pragma once

#include <string>
#include <vector>
#include <sys/types.h>

namespace SzpontDesktop {

struct XdgApp {
    std::string id;            // e.g. "szponterm"
    std::string desktop_file;  // absolute path
    std::string name;          // Display Name
    std::string generic_name;
    std::string comment;       // Description
    std::string exec;          // Executable command line
    std::string icon;          // Icon name / path / monogram symbol
    std::string categories;
    bool terminal{false};
    bool no_display{false};

    std::string monogram() const;
    pid_t launch(const std::string &terminal_bin = "/usr/bin/szponterm") const;
};

class XdgRegistry {
public:
    static std::vector<XdgApp> scan_applications();
    static const XdgApp *find_by_id(const std::vector<XdgApp> &apps, const std::string &id);
};

} // namespace SzpontDesktop
