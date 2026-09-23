#include <SzpontUI/App/Timer.hpp>
#include <SzpontUI/App/Application.hpp>

namespace SzpontUI {

Timer::Timer(int interval_ms, Callback callback, bool single_shot)
    : interval_ms_(interval_ms), callback_(std::move(callback)), single_shot_(single_shot) {}

void Timer::start() {
    if (active_) stop();
    if (auto app = Application::instance()) {
        if (single_shot_) {
            id_ = app->set_timeout(interval_ms_, [this]() {
                trigger();
                active_ = false;
            });
        } else {
            id_ = app->set_interval(interval_ms_, [this]() {
                trigger();
            });
        }
        active_ = true;
    }
}

void Timer::stop() {
    if (!active_) return;
    if (auto app = Application::instance()) {
        app->cancel_timer(id_);
    }
    active_ = false;
    id_ = 0;
}

} // namespace SzpontUI
