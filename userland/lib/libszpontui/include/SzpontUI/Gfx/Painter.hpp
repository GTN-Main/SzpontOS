#pragma once

#include <SzpontUI/Core/Geometry.hpp>
#include <SzpontUI/Core/Color.hpp>
#include <SzpontUI/Font/Font.hpp>
#include <SzpontUI/Gfx/BitmapSurface.hpp>
#include <string_view>
#include <vector>

namespace SzpontUI {

enum class TextAlignment {
    Left,
    Center,
    Right
};

enum class VerticalAlignment {
    Top,
    Center,
    Bottom
};

class Painter {
public:
    explicit Painter(BitmapSurface &target);
    ~Painter();

    void push_clip(const Rect &rect);
    void pop_clip();
    Rect current_clip() const;

    void translate(int dx, int dy);
    void translate(Point delta);
    Point translation() const { return origin_; }

    void fill_rect(const Rect &rect, Color color);
    void draw_rect(const Rect &rect, Color color, int thickness = 1);

    void fill_rounded_rect(const Rect &rect, int radius, Color color);
    void draw_rounded_rect(const Rect &rect, int radius, Color color, int thickness = 1);

    void fill_gradient(const Rect &rect, Color start, Color end, bool vertical = true);

    void draw_shadow(const Rect &rect, int blur_radius, Color color = Color(0, 0, 0, 60));

    void draw_line(Point p1, Point p2, Color color, int thickness = 1);

    void draw_bitmap(Point pos, const BitmapSurface &src);
    void draw_bitmap(Point pos, const BitmapSurface &src, const Rect &src_rect);

    void draw_text(Point pos, std::string_view text, const Font &font, Color color);
    void draw_text(const Rect &rect, std::string_view text, const Font &font, Color color,
                   TextAlignment align = TextAlignment::Left,
                   VerticalAlignment valig = VerticalAlignment::Center);

    Size measure_text(std::string_view text, const Font &font);
    void draw_shaped_run(Point pos, const struct ShapedRun &run, const Font &font, Color color);

    Rect apply_transform(const Rect &r) const;
    Point apply_transform(const Point &p) const;

private:
    BitmapSurface &target_;
    Point origin_{0, 0};
    std::vector<Rect> clip_stack_;
};

} // namespace SzpontUI

