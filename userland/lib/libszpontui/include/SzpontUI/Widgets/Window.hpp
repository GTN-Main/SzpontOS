#pragma once

#include <SzpontUI/Widgets/Widget.hpp>
#include <SzpontUI/Gfx/BitmapSurface.hpp>
#include <SzpontUI/Platform/PlatformBackend.hpp>
#include <string>

namespace SzpontUI {

class Window : public Widget {
public:
    Window(Size size, std::string title = "SzpontUI Application");
    ~Window() override;

    const std::string &title() const { return title_; }
    void set_title(const std::string &title);

    void show();
    void hide();
    void close();

    void set_position(Point pos);
    void resize(Size size);
    void set_override_redirect(bool val);
    void *backend_handle() const { return backend_handle_; }

    void set_root_widget(std::shared_ptr<Widget> root);
    Widget *root_widget() const { return root_widget_.get(); }

    void invalidate_window(Rect dirty);
    void render_and_present();
    bool needs_redraw() const { return needs_redraw_; }

    void set_focused_widget(Widget *w);
    Widget *focused_widget() const { return focused_widget_; }

    void handle_backend_event(Event &event);

    Signal<> on_close;

protected:
    void on_paint(Painter &painter) override;
    void on_resize(ResizeEvent &event) override;
    void on_key_down(KeyEvent &event) override;
    void on_key_up(KeyEvent &event) override;

private:
    std::string title_;
    void *backend_handle_{nullptr};
    BitmapSurface backbuffer_;
    Rect dirty_rect_;
    bool needs_redraw_{true};

    std::shared_ptr<Widget> root_widget_;
    Widget *focused_widget_{nullptr};
    Widget *hovered_widget_{nullptr};
    Widget *captured_mouse_widget_{nullptr};
};

} // namespace SzpontUI
