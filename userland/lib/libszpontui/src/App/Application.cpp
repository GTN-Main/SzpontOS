#include <SzpontUI/App/Application.hpp>
#include <SzpontUI/Platform/X11Backend.hpp>
#include <SzpontUI/Font/FontDatabase.hpp>
#include <SzpontUI/Widgets/Window.hpp>
#include <poll.h>
#include <algorithm>

namespace SzpontUI {

Application *Application::instance_ = nullptr;

Application::Application(int &, char **) {
    instance_ = this;
    backend_ = std::unique_ptr<IPlatformBackend>(&X11Backend::instance());
    FontDatabase::instance().initialize();
}

Application::~Application() {
    FontDatabase::instance().shutdown();
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

void Application::register_window(Window *win) {
    if (!win) return;
    if (std::find(windows_.begin(), windows_.end(), win) == windows_.end()) {
        windows_.push_back(win);
    }
}

void Application::unregister_window(Window *win) {
    auto it = std::find(windows_.begin(), windows_.end(), win);
    if (it != windows_.end()) {
        windows_.erase(it);
    }
    if (windows_.empty() && running_) {
        quit(0);
    }
}

TimerId Application::set_timeout(int ms, std::function<void()> callback) {
    TimerId id = ++next_timer_id_;
    auto now = std::chrono::steady_clock::now();
    timers_.push_back({id, ms, true, now + std::chrono::milliseconds(ms), std::move(callback)});
    return id;
}

TimerId Application::set_interval(int ms, std::function<void()> callback) {
    TimerId id = ++next_timer_id_;
    auto now = std::chrono::steady_clock::now();
    timers_.push_back({id, ms, false, now + std::chrono::milliseconds(ms), std::move(callback)});
    return id;
}

void Application::cancel_timer(TimerId id) {
    for (auto it = timers_.begin(); it != timers_.end(); ++it) {
        if (it->id == id) {
            timers_.erase(it);
            break;
        }
    }
}

void Application::quit(int return_code) {
    running_ = false;
    exit_code_ = return_code;
}

int Application::exec() {
    running_ = true;
    exit_code_ = 0;

    int fd = backend_->connection_fd();

    while (running_) {
        // 1. Process pending backend events first
        backend_->pump_events();

        if (!running_ || windows_.empty()) {
            break;
        }

        // 2. Check timers and calculate next timeout
        auto now = std::chrono::steady_clock::now();
        int timeout_ms = -1;

        for (auto it = timers_.begin(); it != timers_.end();) {
            if (now >= it->next_fire) {
                if (it->callback) {
                    it->callback();
                }
                if (it->single_shot) {
                    it = timers_.erase(it);
                    continue;
                } else {
                    it->next_fire = now + std::chrono::milliseconds(it->interval_ms);
                }
            }
            int diff = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(it->next_fire - now).count());
            if (diff < 0) diff = 0;
            if (timeout_ms < 0 || diff < timeout_ms) {
                timeout_ms = diff;
            }
            ++it;
        }

        // 3. Render and present all windows that need redraw
        for (auto *win : windows_) {
            if (win && win->needs_redraw()) {
                win->render_and_present();
            }
        }

        if (windows_.empty()) {
            break;
        }

        // 4. Wait for X11 events or timer expiration via poll()
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        poll(&pfd, 1, timeout_ms);
    }

    return exit_code_;
}

} // namespace SzpontUI
