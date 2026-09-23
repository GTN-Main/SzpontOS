#pragma once

#include <SzpontUI/Widgets/Widget.hpp>

namespace SzpontUI {

class Slider : public Widget {
public:
    explicit Slider(float min_val = 0.0f, float max_val = 1.0f, float current = 0.5f);

    float value() const { return value_; }
    void set_value(float val);

    float min_value() const { return min_val_; }
    float max_value() const { return max_val_; }
    void set_range(float min_val, float max_val);

    Size measure(Size available) override;

    Signal<float> on_value_changed;

protected:
    void on_paint(Painter &painter) override;
    void on_mouse_down(MouseEvent &event) override;
    void on_mouse_move(MouseEvent &event) override;
    void on_mouse_up(MouseEvent &event) override;

private:
    void update_value_from_x(int x);

    float min_val_{0.0f};
    float max_val_{1.0f};
    float value_{0.5f};
    bool dragging_{false};
};

} // namespace SzpontUI
