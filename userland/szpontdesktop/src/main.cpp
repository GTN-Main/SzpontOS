#include <SzpontUI/SzpontUI.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/reboot.h>

#include "Config.hpp"
#include "XdgApp.hpp"
#include "Wallpaper.hpp"
#include "WindowManager.hpp"
#include "Panel.hpp"
#include "AppMenu.hpp"
#include "Dock.hpp"

using namespace SzpontUI;
using namespace SzpontDesktop;

static void launch_app_by_id(const std::vector<XdgApp> &apps, const std::string &id, const std::string &term) {
    printf("[szpontdesktop] launch_app_by_id: '%s'\n", id.c_str());
    fflush(stdout);
    const XdgApp *app = XdgRegistry::find_by_id(apps, id);
    if (app) {
        printf("[szpontdesktop] Found XdgApp: name='%s' exec='%s'\n", app->name.c_str(), app->exec.c_str());
        fflush(stdout);
        app->launch(term);
        return;
    }
    printf("[szpontdesktop] Falling back to direct exec for '%s'\n", id.c_str());
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        char *const argv[] = {(char*)id.c_str(), nullptr};
        execvp(id.c_str(), argv);
        _exit(127);
    }
    if (pid > 0) setpgid(pid, pid);
}

int main(int argc, char *argv[]) {
    setpgid(0, 0);
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    const char *disp_name = (argc > 1) ? argv[1] : getenv("DISPLAY");
    if (!disp_name || !*disp_name) disp_name = ":0";

    printf("[szpontdesktop] Initializing Szpont Experience (C++ SzpontUI) on '%s'...\n", disp_name);
    setenv("DISPLAY", disp_name, 1);
    setenv("XDG_CURRENT_DESKTOP", "SzpontOS", 0);
    setenv("XDG_SESSION_TYPE", "x11", 0);
    setenv("XDG_RUNTIME_DIR", "/tmp", 0);

    Application app(argc, argv);
    auto &backend = dynamic_cast<X11Backend&>(app.backend());
    Display *dpy = backend.display();
    int screen = backend.screen();
    ::Window root = backend.root_window();

    if (!dpy) {
        fprintf(stderr, "[szpontdesktop] Fatal: Cannot connect to X server '%s'!\n", disp_name);
        return 1;
    }

    Size screen_size = backend.screen_size();
    int screen_w = screen_size.width;
    int screen_h = screen_size.height;

    // 1. Load Configuration (/etc/szpontdesktop.conf)
    DesktopConfig config = DesktopConfig::load("/etc/szpontdesktop.conf");
    printf("[szpontdesktop] Config loaded: wallpaper='%s', dock_enabled=%d, terminal='%s'\n",
           config.wallpaper.c_str(), config.dock_enabled ? 1 : 0, config.terminal.c_str());

    // 2. Discover XDG Applications from /usr/share/applications
    std::vector<XdgApp> apps = XdgRegistry::scan_applications();
    printf("[szpontdesktop] Discovered %zu applications from XDG desktop entries.\n", apps.size());

    // 3. Apply Root Wallpaper
    WallpaperManager::apply_wallpaper(dpy, root, screen, screen_w, screen_h, config.wallpaper);

    // 4. Initialize Window Manager
    auto wm = std::make_unique<WindowManager>(dpy, screen, root, config);
    wm->initialize();

    // 5. Register native event filter to intercept WM events before SzpontUI window lookup
    backend.set_raw_event_filter([&wm](const void *native_ev) -> bool {
        const XEvent *xev = static_cast<const XEvent*>(native_ev);
        return wm->handle_event(*xev);
    });

    // 6. Create TopBar Panel Window
    auto panel = std::make_shared<PanelWindow>(screen_w, config.panel_height, *wm, config);
    panel->show();

    // 7. Create Start Application Menu Window (hidden initially)
    auto app_menu = std::make_shared<AppMenuWindow>(apps, config);

    // 8. Create Floating Bottom Dock Window
    std::shared_ptr<DockWindow> dock;
    if (config.dock_enabled) {
        dock = std::make_shared<DockWindow>(screen_w, screen_h, apps, *wm, config);
        dock->show();
    }

    // Connect signals & actions
    panel->on_start_clicked = [&app_menu]() {
        app_menu->toggle();
    };

    panel->on_exit_clicked = [&app]() {
        printf("[szpontdesktop] Exit clicked from panel. Exiting session...\n");
        app.quit(0);
    };

    app_menu->on_launch_app = [&config](const XdgApp &a) {
        printf("[szpontdesktop] Launching application '%s' (%s)...\n", a.name.c_str(), a.exec.c_str());
        a.launch(config.terminal);
    };

    app_menu->on_reboot_clicked = []() {
        sync();
        reboot(RB_AUTOBOOT);
    };

    app_menu->on_poweroff_clicked = []() {
        sync();
        reboot(RB_POWER_OFF);
    };

    app_menu->on_logout_clicked = [&app]() {
        printf("[szpontdesktop] Logout clicked from app menu. Exiting session...\n");
        app.quit(0);
    };

    if (dock) {
        dock->on_launch_app = [&config](const XdgApp &a) {
            printf("[szpontdesktop] Dock launching '%s' (%s)...\n", a.name.c_str(), a.exec.c_str());
            a.launch(config.terminal);
        };
    }

    wm->on_session_exit_requested = [&app]() {
        printf("[szpontdesktop] Session exit requested via shortcut. Exiting...\n");
        app.quit(0);
    };

    wm->on_toggle_app_menu = [&app_menu]() {
        app_menu->toggle();
    };

    wm->on_quick_launch = [&apps, &config](int index) {
        printf("[szpontdesktop] on_quick_launch index=%d\n", index);
        fflush(stdout);
        if (index == 0) {
            launch_app_by_id(apps, "szponter", config.terminal);
        } else if (index == 1) {
            XdgApp term;
            term.id = "terminal";
            term.exec = config.terminal;
            term.terminal = false;
            term.launch();
        } else if (index == 2) {
            launch_app_by_id(apps, "szpontview", config.terminal);
        } else if (index == 3) {
            launch_app_by_id(apps, "szpontmon", config.terminal);
        }
    };

    // 1-second interval timer for live clock & non-blocking child process reaping
    app.set_interval(1000, [&panel]() {
        panel->update_time();
        pid_t reaped;
        while ((reaped = waitpid(-1, nullptr, WNOHANG)) > 0) {
            printf("[szpontdesktop] Reaped child process pid=%d\n", reaped);
            fflush(stdout);
        }
    });

    // Run autostart applications
    for (const auto &auto_cmd : config.autostart) {
        if (!auto_cmd.empty()) {
            printf("[szpontdesktop] Autostarting: '%s'\n", auto_cmd.c_str());
            launch_app_by_id(apps, auto_cmd, config.terminal);
        }
    }

    printf("[szpontdesktop] Szpont Experience Desktop Environment ready.\n");
    fflush(stdout);

    int ret = app.exec();

    printf("[szpontdesktop] Shutting down desktop session...\n");
    kill(0, SIGTERM);
    usleep(25000);
    kill(0, SIGKILL);
    while (waitpid(-1, nullptr, WNOHANG) > 0) {}

    return ret;
}
