#pragma once

#include <SzpontUI/Core/Object.hpp>
#include <SzpontUI/Core/Geometry.hpp>
#include <SzpontUI/Core/Color.hpp>
#include <SzpontUI/Core/Signal.hpp>
#include <SzpontUI/Gfx/Painter.hpp>
#include <SzpontUI/Platform/Event.hpp>
#include <SzpontUI/Layout/Layout.hpp>
#include <memory>
#include <vector>

namespace SzpontUI {

class Window;

class Widget : public Object, public LayoutItem {
public:
    explicit Widget(std::string name = "");
    ~Widget() override;

    // Hierarchy & Windows
    Window *window();
    const Window *window() const;

    // Geometry
    Rect bounds() const { return bounds_; }
    void set_bounds(Rect bounds);
    Point pos() const { return bounds_.pos(); }
    Size size() const { return bounds_.size(); }
    int width() const { return bounds_.width; }
    int height() const { return bounds_.height; }

    Size min_size() const { return min_size_; }
    void set_min_size(Size s) { min_size_ = s; invalidate_layout(); }

    Size max_size() const { return max_size_; }
    void set_max_size(Size s) { max_size_ = s; invalidate_layout(); }

    void set_fixed_size(Size s) {
        set_min_size(s);
        set_max_size(s);
        set_size_policy(SizePolicy::Fixed, SizePolicy::Fixed);
    }
    void set_fixed_size(int w, int h) { set_fixed_size(Size{w, h}); }

    void set_size_policy(SizePolicy h, SizePolicy v) {
        h_policy_ = h;
        v_policy_ = v;
        invalidate_layout();
    }
    SizePolicy horizontal_policy() const override { return h_policy_; }
    SizePolicy vertical_policy() const override { return v_policy_; }

    // Layout
    void set_layout(std::shared_ptr<Layout> layout);
    Layout *layout() const { return layout_.get(); }
    void invalidate_layout();

    Size measure(Size available) override;
    void arrange(Rect bounds) override;

    // Visibility & Interaction States
    bool is_visible() const { return visible_; }
    void set_visible(bool v);

    bool is_enabled() const { return enabled_; }
    void set_enabled(bool e);

    bool is_hovered() const { return hovered_; }
    bool is_pressed() const { return pressed_; }
    bool is_focused() const { return focused_; }

    void set_focus();

    // Redraw and Event Dispatch
    void update(Rect dirty_rect);
    void update();

    virtual void handle_event(Event &event);
    virtual void paint(Painter &painter);

    // Coordinate conversion
    Point to_screen(Point p) const;
    Point from_screen(Point p) const;
    Point map_to_parent(Point p) const;
    Point map_from_parent(Point p) const;

    // Widget tree finding
    Widget *child_at(Point local_pos);

    // Fluent method chaining
    template <typename T>
    T *as() { return dynamic_cast<T*>(this); }

protected:
    virtual void on_paint(Painter &painter);
    virtual void on_mouse_move(MouseEvent &event);
    virtual void on_mouse_down(MouseEvent &event);
    virtual void on_mouse_up(MouseEvent &event);
    virtual void on_mouse_enter(MouseEvent &event);
    virtual void on_mouse_leave(MouseEvent &event);
    virtual void on_key_down(KeyEvent &event);
    virtual void on_key_up(KeyEvent &event);
    virtual void on_focus_in(Event &event);
    virtual void on_focus_out(Event &event);
    virtual void on_resize(ResizeEvent &event);

    Rect bounds_{0, 0, 100, 30};
    Size min_size_{0, 0};
    Size max_size_{10000, 10000};
    SizePolicy h_policy_{SizePolicy::Preferred};
    SizePolicy v_policy_{SizePolicy::Preferred};

    bool visible_{true};
    bool enabled_{true};
    bool hovered_{false};
    bool pressed_{false};
    bool focused_{false};

    std::shared_ptr<Layout> layout_;
};

} // namespace SzpontUI
