#pragma once

#include <SzpontUI/Platform/PlatformBackend.hpp>
#include <SzpontUI/App/Timer.hpp>
#include <vector>
#include <memory>
#include <chrono>

namespace SzpontUI {

class Window;

class Application {
public:
    Application(int &argc, char **argv);
    virtual ~Application();

    static Application *instance() { return instance_; }

    int exec();
    void quit(int return_code = 0);

    void register_window(Window *win);
    void unregister_window(Window *win);

    TimerId set_timeout(int ms, std::function<void()> callback);
    TimerId set_interval(int ms, std::function<void()> callback);
    void cancel_timer(TimerId id);

    IPlatformBackend &backend() { return *backend_; }

private:
    struct ActiveTimer {
        TimerId id;
        int interval_ms;
        bool single_shot;
        std::chrono::steady_clock::time_point next_fire;
        std::function<void()> callback;
    };

    static Application *instance_;
    std::unique_ptr<IPlatformBackend> backend_;
    std::vector<Window*> windows_;
    std::vector<ActiveTimer> timers_;
    TimerId next_timer_id_{0};

    bool running_{false};
    int exit_code_{0};
};

} // namespace SzpontUI
