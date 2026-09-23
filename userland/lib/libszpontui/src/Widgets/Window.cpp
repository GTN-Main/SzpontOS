#include <SzpontUI/Widgets/Window.hpp>
#include <SzpontUI/App/Application.hpp>
#include <SzpontUI/Theme/Theme.hpp>

namespace SzpontUI {

Window::Window(Size size, std::string title)
    : Widget("Window"), title_(std::move(title)), backbuffer_(size.width, size.height) {
    bounds_ = Rect{0, 0, size.width, size.height};
    dirty_rect_ = bounds_;

    if (auto app = Application::instance()) {
        backend_handle_ = app->backend().create_window(this, size, title_);
        app->register_window(this);
    }
}

Window::~Window() {
    if (auto app = Application::instance()) {
        app->unregister_window(this);
        if (backend_handle_) {
            app->backend().destroy_window(backend_handle_);
            backend_handle_ = nullptr;
        }
    }
}

void Window::set_title(const std::string &title) {
    title_ = title;
    if (auto app = Application::instance()) {
        if (backend_handle_) {
            app->backend().set_title(backend_handle_, title_);
        }
    }
}

void Window::show() {
    set_visible(true);
    if (auto app = Application::instance()) {
        if (backend_handle_) {
            app->backend().show_window(backend_handle_);
        }
    }
    invalidate_window(Rect{0, 0, bounds_.width, bounds_.height});
    render_and_present();
}

void Window::hide() {
    set_visible(false);
    if (auto app = Application::instance()) {
        if (backend_handle_) {
            app->backend().hide_window(backend_handle_);
        }
    }
}

void Window::close() {
    on_close();
    hide();
    if (auto app = Application::instance()) {
        app->unregister_window(this);
    }
}

void Window::set_position(Point pos) {
    bounds_.x = pos.x;
    bounds_.y = pos.y;
    if (auto app = Application::instance()) {
        if (backend_handle_) {
            app->backend().set_position(backend_handle_, pos);
        }
    }
}

void Window::resize(Size size) {
    bounds_.width = size.width;
    bounds_.height = size.height;
    if (auto app = Application::instance()) {
        if (backend_handle_) {
            app->backend().resize_window(backend_handle_, size);
        }
    }
    backbuffer_ = BitmapSurface(size.width, size.height);
    invalidate_window(Rect{0, 0, bounds_.width, bounds_.height});
}

void Window::set_override_redirect(bool val) {
    if (auto app = Application::instance()) {
        if (backend_handle_) {
            app->backend().set_override_redirect(backend_handle_, val);
        }
    }
}

void Window::set_root_widget(std::shared_ptr<Widget> root) {
    if (root_widget_) {
        remove_child(root_widget_);
    }
    root_widget_ = root;
    if (root_widget_) {
        add_child(root_widget_);
        root_widget_->arrange(Rect{0, 0, bounds_.width, bounds_.height});
    }
    invalidate_window(bounds_);
}

void Window::invalidate_window(Rect dirty) {
    if (dirty.is_empty()) return;
    if (dirty_rect_.is_empty()) {
        dirty_rect_ = dirty;
    } else {
        dirty_rect_ = dirty_rect_.united(dirty);
    }
    dirty_rect_ = dirty_rect_.intersected(Rect{0, 0, bounds_.width, bounds_.height});
    needs_redraw_ = true;
}

void Window::render_and_present() {
    if (!needs_redraw_ || dirty_rect_.is_empty() || !is_visible()) return;

    Painter painter(backbuffer_);

    // Clip to dirty region
    painter.push_clip(dirty_rect_);

    // Paint window background or custom window drawing
    on_paint(painter);

    // Paint root layout and children
    if (root_widget_ && root_widget_->is_visible()) {
        root_widget_->paint(painter);
    } else {
        for (const auto &child_obj : children()) {
            if (auto child = std::dynamic_pointer_cast<Widget>(child_obj)) {
                child->paint(painter);
            }
        }
    }

    painter.pop_clip();

    // Present buffer to X11 display
    if (auto app = Application::instance()) {
        if (backend_handle_) {
            app->backend().present(backend_handle_, backbuffer_, dirty_rect_);
        }
    }

    dirty_rect_ = Rect{0, 0, 0, 0};
    needs_redraw_ = false;
}

void Window::set_focused_widget(Widget *w) {
    if (focused_widget_ == w) return;
    if (focused_widget_) {
        Event fo(EventType::LostFocus);
        focused_widget_->handle_event(fo);
    }
    focused_widget_ = w;
    if (focused_widget_) {
        Event fi(EventType::GainedFocus);
        focused_widget_->handle_event(fi);
    }
}

void Window::on_paint(Painter &painter) {
    painter.fill_rect(Rect{0, 0, bounds_.width, bounds_.height}, current_theme().background());
}

void Window::on_resize(ResizeEvent &event) {
    bounds_ = Rect{0, 0, event.width(), event.height()};
    backbuffer_.resize(event.width(), event.height());

    if (root_widget_) {
        root_widget_->arrange(bounds_);
    } else if (layout_) {
        layout_->arrange(bounds_);
    }

    invalidate_window(bounds_);
}

void Window::handle_backend_event(Event &event) {
    switch (event.type()) {
        case EventType::Paint: {
            auto &pe = static_cast<PaintEvent&>(event);
            invalidate_window(pe.dirty_rect());
            break;
        }
        case EventType::Resize: {
            auto &re = static_cast<ResizeEvent&>(event);
            on_resize(re);
            break;
        }
        case EventType::MouseMove: {
            auto &me = static_cast<MouseEvent&>(event);
            Point pt = me.pos();

            Widget *target = captured_mouse_widget_ ? captured_mouse_widget_ : child_at(pt);

            if (target != hovered_widget_) {
                if (hovered_widget_) {
                    MouseEvent le(EventType::MouseLeave, hovered_widget_->from_screen(pt));
                    hovered_widget_->handle_event(le);
                }
                hovered_widget_ = target;
                if (hovered_widget_) {
                    MouseEvent ee(EventType::MouseEnter, hovered_widget_->from_screen(pt));
                    hovered_widget_->handle_event(ee);
                }
            }

            if (target) {
                MouseEvent local_me(EventType::MouseMove, target->from_screen(pt), me.button(), me.modifiers());
                target->handle_event(local_me);
            } else {
                on_mouse_move(me);
            }
            break;
        }
        case EventType::MouseDown: {
            auto &me = static_cast<MouseEvent&>(event);
            Point pt = me.pos();
            Widget *target = child_at(pt);
            if (target) {
                set_focused_widget(target);
                captured_mouse_widget_ = target;
                MouseEvent local_me(EventType::MouseDown, target->from_screen(pt), me.button(), me.modifiers());
                target->handle_event(local_me);
            } else {
                set_focused_widget(nullptr);
                on_mouse_down(me);
            }
            break;
        }
        case EventType::MouseUp: {
            auto &me = static_cast<MouseEvent&>(event);
            Point pt = me.pos();
            Widget *target = captured_mouse_widget_ ? captured_mouse_widget_ : child_at(pt);
            if (target) {
                MouseEvent local_me(EventType::MouseUp, target->from_screen(pt), me.button(), me.modifiers());
                target->handle_event(local_me);
            } else {
                on_mouse_up(me);
            }
            captured_mouse_widget_ = nullptr;
            break;
        }
        case EventType::KeyDown: {
            if (focused_widget_) {
                focused_widget_->handle_event(event);
            }
            on_key_down(static_cast<KeyEvent&>(event));
            break;
        }
        case EventType::KeyUp: {
            if (focused_widget_) {
                focused_widget_->handle_event(event);
            }
            on_key_up(static_cast<KeyEvent&>(event));
            break;
        }
        case EventType::Close: {
            close();
            break;
        }
        default:
            break;
    }
}

void Window::on_key_down(KeyEvent &) {}
void Window::on_key_up(KeyEvent &) {}

} // namespace SzpontUI
