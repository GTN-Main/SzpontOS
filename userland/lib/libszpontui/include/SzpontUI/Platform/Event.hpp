#pragma once

#include <SzpontUI/Core/Geometry.hpp>
#include <string>
#include <cstdint>

namespace SzpontUI {

enum class EventType {
    None,
    MouseMove,
    MouseDown,
    MouseUp,
    MouseEnter,
    MouseLeave,
    MouseWheel,
    KeyDown,
    KeyUp,
    Paint,
    Resize,
    GainedFocus,
    LostFocus,
    Close
};

enum class MouseButton {
    None = 0,
    Left = 1,
    Middle = 2,
    Right = 3
};

struct KeyModifiers {
    bool shift{false};
    bool ctrl{false};
    bool alt{false};
    bool meta{false};
};

class Event {
public:
    explicit Event(EventType type) : type_(type) {}
    virtual ~Event() = default;

    EventType type() const { return type_; }
    bool is_accepted() const { return accepted_; }
    void accept() { accepted_ = true; }
    void ignore() { accepted_ = false; }

private:
    EventType type_{EventType::None};
    bool accepted_{false};
};

class MouseEvent : public Event {
public:
    MouseEvent(EventType type, Point pos, MouseButton button = MouseButton::None, KeyModifiers mods = {})
        : Event(type), pos_(pos), button_(button), modifiers_(mods) {}

    Point pos() const { return pos_; }
    int x() const { return pos_.x; }
    int y() const { return pos_.y; }
    MouseButton button() const { return button_; }
    KeyModifiers modifiers() const { return modifiers_; }

private:
    Point pos_{0, 0};
    MouseButton button_{MouseButton::None};
    KeyModifiers modifiers_{};
};

class KeyEvent : public Event {
public:
    KeyEvent(EventType type, uint32_t key_code, uint32_t keysym, std::string text = "", KeyModifiers mods = {})
        : Event(type), key_code_(key_code), keysym_(keysym), text_(std::move(text)), modifiers_(mods) {}

    uint32_t key_code() const { return key_code_; }
    uint32_t keysym() const { return keysym_; }
    const std::string &text() const { return text_; }
    KeyModifiers modifiers() const { return modifiers_; }

private:
    uint32_t key_code_{0};
    uint32_t keysym_{0};
    std::string text_;
    KeyModifiers modifiers_{};
};

class PaintEvent : public Event {
public:
    explicit PaintEvent(Rect dirty_rect) : Event(EventType::Paint), dirty_rect_(dirty_rect) {}
    Rect dirty_rect() const { return dirty_rect_; }

private:
    Rect dirty_rect_;
};

class ResizeEvent : public Event {
public:
    explicit ResizeEvent(Size new_size) : Event(EventType::Resize), new_size_(new_size) {}
    Size size() const { return new_size_; }
    int width() const { return new_size_.width; }
    int height() const { return new_size_.height; }

private:
    Size new_size_;
};

} // namespace SzpontUI
