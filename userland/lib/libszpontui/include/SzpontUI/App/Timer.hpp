#pragma once

#include <functional>
#include <cstdint>
#include <chrono>

namespace SzpontUI {

using TimerId = uint64_t;

class Timer {
public:
    using Callback = std::function<void()>;

    Timer(int interval_ms, Callback callback, bool single_shot = false);
    ~Timer() = default;

    TimerId id() const { return id_; }
    int interval() const { return interval_ms_; }
    bool is_single_shot() const { return single_shot_; }
    bool is_active() const { return active_; }

    void start();
    void stop();

    void trigger() {
        if (callback_) callback_();
    }

private:
    TimerId id_{0};
    int interval_ms_{100};
    Callback callback_;
    bool single_shot_{false};
    bool active_{false};
};

} // namespace SzpontUI
