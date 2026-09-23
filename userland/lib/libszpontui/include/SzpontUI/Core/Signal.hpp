#pragma once

#include <functional>
#include <vector>
#include <utility>
#include <cstdint>

namespace SzpontUI {

using ConnectionId = uint64_t;

template <typename... Args>
class Signal {
public:
    using Callback = std::function<void(Args...)>;

    Signal() = default;
    ~Signal() = default;

    // Non-copyable, movable
    Signal(const Signal &) = delete;
    Signal &operator=(const Signal &) = delete;
    Signal(Signal &&) noexcept = default;
    Signal &operator=(Signal &&) noexcept = default;

    ConnectionId connect(Callback callback) {
        ConnectionId id = ++next_id_;
        slots_.push_back({id, std::move(callback)});
        return id;
    }

    void disconnect(ConnectionId id) {
        for (auto it = slots_.begin(); it != slots_.end(); ++it) {
            if (it->id == id) {
                slots_.erase(it);
                break;
            }
        }
    }

    void emit(Args... args) const {
        // Copy callbacks list to allow connection/disconnection during emission
        auto current_slots = slots_;
        for (const auto &slot : current_slots) {
            if (slot.callback) {
                slot.callback(args...);
            }
        }
    }

    void operator()(Args... args) const {
        emit(std::forward<Args>(args)...);
    }

    void clear() {
        slots_.clear();
    }

    bool is_empty() const {
        return slots_.empty();
    }

private:
    struct Slot {
        ConnectionId id;
        Callback callback;
    };

    std::vector<Slot> slots_;
    ConnectionId next_id_{0};
};

} // namespace SzpontUI
