#include <SzpontUI/Widgets/Widget.hpp>
#include <SzpontUI/Widgets/Window.hpp>

namespace SzpontUI {

Widget::Widget(std::string name) : Object(std::move(name)) {}

Widget::~Widget() = default;

Window *Widget::window() {
    Object *cur = this;
    while (cur) {
        if (auto w = dynamic_cast<Window*>(cur)) {
            return w;
        }
        cur = cur->parent();
    }
    return nullptr;
}

const Window *Widget::window() const {
    const Object *cur = this;
    while (cur) {
        if (auto w = dynamic_cast<const Window*>(cur)) {
            return w;
        }
        cur = cur->parent();
    }
    return nullptr;
}

void Widget::set_bounds(Rect bounds) {
    if (bounds_ == bounds) return;
    Rect old_bounds = bounds_;
    bounds_ = bounds;

    if (layout_) {
        layout_->arrange(Rect{0, 0, bounds_.width, bounds_.height});
    }

    update(Rect{0, 0, old_bounds.width, old_bounds.height});
    update(Rect{0, 0, bounds_.width, bounds_.height});
}

void Widget::set_layout(std::shared_ptr<Layout> layout) {
    layout_ = layout;
    if (layout_) {
        layout_->set_owner(this);
    }
    invalidate_layout();
}

void Widget::invalidate_layout() {
    if (layout_) {
        layout_->arrange(Rect{0, 0, bounds_.width, bounds_.height});
    }
    update();
}

Size Widget::measure(Size available) {
    if (h_policy_ == SizePolicy::Fixed && v_policy_ == SizePolicy::Fixed) {
        return min_size_;
    }
    if (layout_) {
        Size layout_sz = layout_->measure(available);
        return Size{
            std::max(min_size_.width, layout_sz.width),
            std::max(min_size_.height, layout_sz.height)
        };
    }
    return Size{std::max(min_size_.width, bounds_.width), std::max(min_size_.height, bounds_.height)};
}

void Widget::arrange(Rect bounds) {
    set_bounds(bounds);
}

void Widget::set_visible(bool v) {
    if (visible_ == v) return;
    visible_ = v;
    update();
}

void Widget::set_enabled(bool e) {
    if (enabled_ == e) return;
    enabled_ = e;
    update();
}

void Widget::set_focus() {
    if (auto win = window()) {
        win->set_focused_widget(this);
    }
}

void Widget::update(Rect dirty_rect) {
    if (!visible_) return;
    if (auto win = window()) {
        Point screen_p = to_screen(dirty_rect.pos());
        win->invalidate_window(Rect{screen_p, dirty_rect.size()});
    }
}

void Widget::update() {
    update(Rect{0, 0, bounds_.width, bounds_.height});
}

Point Widget::map_to_parent(Point p) const {
    return p + bounds_.pos();
}

Point Widget::map_from_parent(Point p) const {
    return p - bounds_.pos();
}

Point Widget::to_screen(Point p) const {
    if (dynamic_cast<const Window*>(this)) return p;
    Point cur = p + bounds_.pos();
    const Object *par = parent();
    while (par) {
        if (auto w = dynamic_cast<const Widget*>(par)) {
            // Stop at window root
            if (dynamic_cast<const Window*>(par)) break;
            cur += w->pos();
        }
        par = par->parent();
    }
    return cur;
}

Point Widget::from_screen(Point p) const {
    if (dynamic_cast<const Window*>(this)) return p;
    Point cur = p;
    const Object *par = parent();
    while (par) {
        if (auto w = dynamic_cast<const Widget*>(par)) {
            if (dynamic_cast<const Window*>(par)) break;
            cur -= w->pos();
        }
        par = par->parent();
    }
    return cur - bounds_.pos();
}

Widget *Widget::child_at(Point local_pos) {
    for (auto it = children().rbegin(); it != children().rend(); ++it) {
        if (auto child = std::dynamic_pointer_cast<Widget>(*it)) {
            if (child->is_visible() && child->bounds().contains(local_pos)) {
                Point child_local = child->map_from_parent(local_pos);
                Widget *hit = child->child_at(child_local);
                return hit ? hit : child.get();
            }
        }
    }
    return nullptr;
}

void Widget::paint(Painter &painter) {
    if (!visible_) return;

    painter.translate(bounds_.pos());
    painter.push_clip(Rect{0, 0, bounds_.width, bounds_.height});

    on_paint(painter);

    Rect clip = painter.current_clip();
    for (const auto &child_obj : children()) {
        if (auto child = std::dynamic_pointer_cast<Widget>(child_obj)) {
            if (child->is_visible() && clip.intersects(painter.apply_transform(child->bounds()))) {
                child->paint(painter);
            }
        }
    }

    painter.pop_clip();
    painter.translate(-bounds_.x, -bounds_.y);
}

void Widget::handle_event(Event &event) {
    switch (event.type()) {
        case EventType::MouseMove:
            on_mouse_move(static_cast<MouseEvent&>(event));
            break;
        case EventType::MouseDown:
            on_mouse_down(static_cast<MouseEvent&>(event));
            break;
        case EventType::MouseUp:
            on_mouse_up(static_cast<MouseEvent&>(event));
            break;
        case EventType::MouseEnter:
            hovered_ = true;
            on_mouse_enter(static_cast<MouseEvent&>(event));
            update();
            break;
        case EventType::MouseLeave:
            hovered_ = false;
            pressed_ = false;
            on_mouse_leave(static_cast<MouseEvent&>(event));
            update();
            break;
        case EventType::KeyDown:
            on_key_down(static_cast<KeyEvent&>(event));
            break;
        case EventType::KeyUp:
            on_key_up(static_cast<KeyEvent&>(event));
            break;
        case EventType::GainedFocus:
            focused_ = true;
            on_focus_in(event);
            update();
            break;
        case EventType::LostFocus:
            focused_ = false;
            on_focus_out(event);
            update();
            break;
        case EventType::Resize:
            on_resize(static_cast<ResizeEvent&>(event));
            break;
        default:
            break;
    }
}

void Widget::on_paint(Painter &) {}
void Widget::on_mouse_move(MouseEvent &) {}
void Widget::on_mouse_down(MouseEvent &) {}
void Widget::on_mouse_up(MouseEvent &) {}
void Widget::on_mouse_enter(MouseEvent &) {}
void Widget::on_mouse_leave(MouseEvent &) {}
void Widget::on_key_down(KeyEvent &) {}
void Widget::on_key_up(KeyEvent &) {}
void Widget::on_focus_in(Event &) {}
void Widget::on_focus_out(Event &) {}
void Widget::on_resize(ResizeEvent &) {}

} // namespace SzpontUI
