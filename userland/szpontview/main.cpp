#include <SzpontUI/SzpontUI.hpp>
#include "ImageViewerWindow.hpp"
#include <cstdio>

using namespace SzpontUI;
using namespace SzpontView;

int main(int argc, char *argv[]) {
    Application app(argc, argv);

    std::string start_path;
    if (argc > 1 && argv[1] && argv[1][0]) {
        start_path = argv[1];
    }

    auto window = std::make_shared<ImageViewerWindow>(920, 620, start_path);
    window->show();

    return app.exec();
}
