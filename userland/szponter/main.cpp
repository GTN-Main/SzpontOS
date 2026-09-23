#include <SzpontUI/SzpontUI.hpp>
#include "SzponterWindow.hpp"
#include <cstdio>

using namespace SzpontUI;
using namespace Szponter;

int main(int argc, char *argv[]) {
    Application app(argc, argv);

    std::string start_path = "/";
    if (argc > 1 && argv[1] && argv[1][0]) {
        start_path = argv[1];
    } else {
        const char *home = getenv("HOME");
        if (home && access(home, R_OK) == 0) {
            start_path = home;
        }
    }

    auto window = std::make_shared<SzponterWindow>(880, 560, start_path);
    window->show();

    return app.exec();
}
