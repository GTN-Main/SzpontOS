#include <SzpontUI/SzpontUI.hpp>
#include "SystemMonitorWindow.hpp"
#include <cstdio>

using namespace SzpontUI;
using namespace SzpontMon;

int main(int argc, char *argv[]) {
    Application app(argc, argv);

    auto window = std::make_shared<SystemMonitorWindow>(860, 580);
    window->show();

    // 1-second periodic data refresh
    app.set_interval(1000, [&window]() {
        window->refresh_data();
    });

    return app.exec();
}
