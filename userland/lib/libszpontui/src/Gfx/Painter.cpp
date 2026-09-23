#include <SzpontUI/Gfx/Painter.hpp>
#include <SzpontUI/Font/TextShaper.hpp>
#include <SzpontUI/Font/GlyphCache.hpp>
#include <pixman.h>
#include <cmath>
#include <cstring>

namespace SzpontUI {

Painter::Painter(BitmapSurface &target) : target_(target) {
    clip_stack_.push_back(target_.rect());
}

Painter::~Painter() = default;

void Painter::push_clip(const Rect &rect) {
    Rect transformed = apply_transform(rect);
    Rect current = current_clip();
    clip_stack_.push_back(current.intersected(transformed));
}

void Painter::pop_clip() {
    if (clip_stack_.size() > 1) {
        clip_stack_.pop_back();
    }
}

Rect Painter::current_clip() const {
    if (clip_stack_.empty()) return target_.rect();
    return clip_stack_.back();
}

void Painter::translate(int dx, int dy) {
    origin_.x += dx;
    origin_.y += dy;
}

void Painter::translate(Point delta) {
    translate(delta.x, delta.y);
}

Rect Painter::apply_transform(const Rect &r) const {
    return r.translated(origin_);
}

Point Painter::apply_transform(const Point &p) const {
    return p + origin_;
}

static inline pixman_color_t to_pixman_color(Color color) {
    uint32_t a = (color.a * 65535) / 255;
    pixman_color_t pcolor;
    pcolor.alpha = a;
    pcolor.red   = (color.r * a) / 255;
    pcolor.green = (color.g * a) / 255;
    pcolor.blue  = (color.b * a) / 255;
    return pcolor;
}

void Painter::fill_rect(const Rect &rect, Color color) {
    if (color.a == 0) return;

    Rect r = apply_transform(rect).intersected(current_clip());
    if (r.is_empty()) return;

    if (color.a == 255) {
        // Fast path for 100% opaque rectangle: direct scanline memory fill (no pixman heap allocation)
        uint32_t argb = color.to_argb();
        for (int y = r.y; y < r.bottom(); ++y) {
            std::fill_n(target_.scanline(y) + r.x, r.width, argb);
        }
        return;
    }

    pixman_color_t pcolor = to_pixman_color(color);
    pixman_image_t *solid = pixman_image_create_solid_fill(&pcolor);
    if (!solid) return;

    pixman_image_composite32(
        PIXMAN_OP_OVER,
        solid,
        nullptr,
        target_.pixman_image(),
        0, 0,
        0, 0,
        r.x, r.y,
        r.width, r.height
    );

    pixman_image_unref(solid);
}

void Painter::draw_rect(const Rect &rect, Color color, int thickness) {
    if (thickness <= 0 || color.a == 0) return;

    // Top
    fill_rect(Rect{rect.x, rect.y, rect.width, thickness}, color);
    // Bottom
    fill_rect(Rect{rect.x, rect.bottom() - thickness, rect.width, thickness}, color);
    // Left
    fill_rect(Rect{rect.x, rect.y + thickness, thickness, rect.height - 2 * thickness}, color);
    // Right
    fill_rect(Rect{rect.right() - thickness, rect.y + thickness, thickness, rect.height - 2 * thickness}, color);
}

void Painter::fill_rounded_rect(const Rect &rect, int radius, Color color) {
    if (color.a == 0) return;

    Rect r = apply_transform(rect).intersected(current_clip());
    if (r.is_empty()) return;

    if (radius <= 0) {
        fill_rect(rect, color);
        return;
    }

    int max_r = std::min(rect.width, rect.height) / 2;
    radius = std::min(radius, max_r);

    // Inner cross rectangles
    fill_rect(Rect{rect.x + radius, rect.y, rect.width - 2 * radius, rect.height}, color);
    fill_rect(Rect{rect.x, rect.y + radius, radius, rect.height - 2 * radius}, color);
    fill_rect(Rect{rect.right() - radius, rect.y + radius, radius, rect.height - 2 * radius}, color);

    // Corner culling: check which corners intersect current_clip()
    Rect clip = current_clip();
    bool do_tl = clip.intersects(apply_transform(Rect{rect.x, rect.y, radius, radius}));
    bool do_tr = clip.intersects(apply_transform(Rect{rect.right() - radius, rect.y, radius, radius}));
    bool do_bl = clip.intersects(apply_transform(Rect{rect.x, rect.bottom() - radius, radius, radius}));
    bool do_br = clip.intersects(apply_transform(Rect{rect.right() - radius, rect.bottom() - radius, radius, radius}));

    if (!do_tl && !do_tr && !do_bl && !do_br) return;

    Point centers[4] = {
        {rect.x + radius, rect.y + radius},                         // Top-Left
        {rect.right() - radius - 1, rect.y + radius},                 // Top-Right
        {rect.x + radius, rect.bottom() - radius - 1},                // Bottom-Left
        {rect.right() - radius - 1, rect.bottom() - radius - 1}       // Bottom-Right
    };

    int r_sq = radius * radius;
    int inner_r = radius - 1;
    int inner_sq = inner_r * inner_r;

    for (int cy = 0; cy < radius; ++cy) {
        int dy = radius - 1 - cy;
        int dy_sq = dy * dy;
        for (int cx = 0; cx < radius; ++cx) {
            int dx = radius - 1 - cx;
            int dist_sq = dx * dx + dy_sq;
            if (dist_sq > r_sq) continue;

            float alpha_factor = (dist_sq > inner_sq) ? (radius - std::sqrt(static_cast<float>(dist_sq))) : 1.0f;
            Color pixel_color = color.with_opacity(alpha_factor);

            if (do_tl) {
                Point p0 = apply_transform(Point{centers[0].x - radius + cx, centers[0].y - radius + cy});
                if (clip.contains(p0)) target_.set_pixel(p0.x, p0.y, Color::lerp(target_.get_pixel(p0.x, p0.y), color, pixel_color.a / 255.0f));
            }
            if (do_tr) {
                Point p1 = apply_transform(Point{centers[1].x + radius - 1 - cx, centers[1].y - radius + cy});
                if (clip.contains(p1)) target_.set_pixel(p1.x, p1.y, Color::lerp(target_.get_pixel(p1.x, p1.y), color, pixel_color.a / 255.0f));
            }
            if (do_bl) {
                Point p2 = apply_transform(Point{centers[2].x - radius + cx, centers[2].y + radius - 1 - cy});
                if (clip.contains(p2)) target_.set_pixel(p2.x, p2.y, Color::lerp(target_.get_pixel(p2.x, p2.y), color, pixel_color.a / 255.0f));
            }
            if (do_br) {
                Point p3 = apply_transform(Point{centers[3].x + radius - 1 - cx, centers[3].y + radius - 1 - cy});
                if (clip.contains(p3)) target_.set_pixel(p3.x, p3.y, Color::lerp(target_.get_pixel(p3.x, p3.y), color, pixel_color.a / 255.0f));
            }
        }
    }
}

void Painter::draw_rounded_rect(const Rect &rect, int radius, Color color, int thickness) {
    if (color.a == 0 || thickness <= 0) return;

    Rect r = apply_transform(rect).intersected(current_clip());
    if (r.is_empty()) return;

    if (radius <= 0) {
        draw_rect(rect, color, thickness);
        return;
    }

    // Top & Bottom segments
    fill_rect(Rect{rect.x + radius, rect.y, rect.width - 2 * radius, thickness}, color);
    fill_rect(Rect{rect.x + radius, rect.bottom() - thickness, rect.width - 2 * radius, thickness}, color);

    // Left & Right segments
    fill_rect(Rect{rect.x, rect.y + radius, thickness, rect.height - 2 * radius}, color);
    fill_rect(Rect{rect.right() - thickness, rect.y + radius, thickness, rect.height - 2 * radius}, color);

    // Corner culling
    Rect clip = current_clip();
    bool do_tl = clip.intersects(apply_transform(Rect{rect.x, rect.y, radius, radius}));
    bool do_tr = clip.intersects(apply_transform(Rect{rect.right() - radius, rect.y, radius, radius}));
    bool do_bl = clip.intersects(apply_transform(Rect{rect.x, rect.bottom() - radius, radius, radius}));
    bool do_br = clip.intersects(apply_transform(Rect{rect.right() - radius, rect.bottom() - radius, radius, radius}));

    if (!do_tl && !do_tr && !do_bl && !do_br) return;

    Point centers[4] = {
        {rect.x + radius, rect.y + radius},
        {rect.right() - radius - 1, rect.y + radius},
        {rect.x + radius, rect.bottom() - radius - 1},
        {rect.right() - radius - 1, rect.bottom() - radius - 1}
    };

    float min_dist = static_cast<float>(radius - thickness);
    for (int cy = 0; cy < radius; ++cy) {
        for (int cx = 0; cx < radius; ++cx) {
            float dist = std::sqrt(static_cast<float>((radius - 1 - cx) * (radius - 1 - cx) + (radius - 1 - cy) * (radius - 1 - cy)));
            if (dist <= radius && dist >= radius - thickness - 0.5f) {
                float alpha_factor = 1.0f;
                if (dist > radius - 1.0f) alpha_factor = radius - dist;
                else if (dist < min_dist) alpha_factor = dist - min_dist;

                alpha_factor = std::max(0.0f, std::min(1.0f, alpha_factor));
                Color pixel_color = color.with_opacity(alpha_factor);

                if (do_tl) {
                    Point p0 = apply_transform(Point{centers[0].x - radius + cx, centers[0].y - radius + cy});
                    if (clip.contains(p0)) target_.set_pixel(p0.x, p0.y, Color::lerp(target_.get_pixel(p0.x, p0.y), color, pixel_color.a / 255.0f));
                }
                if (do_tr) {
                    Point p1 = apply_transform(Point{centers[1].x + radius - 1 - cx, centers[1].y - radius + cy});
                    if (clip.contains(p1)) target_.set_pixel(p1.x, p1.y, Color::lerp(target_.get_pixel(p1.x, p1.y), color, pixel_color.a / 255.0f));
                }
                if (do_bl) {
                    Point p2 = apply_transform(Point{centers[2].x - radius + cx, centers[2].y + radius - 1 - cy});
                    if (clip.contains(p2)) target_.set_pixel(p2.x, p2.y, Color::lerp(target_.get_pixel(p2.x, p2.y), color, pixel_color.a / 255.0f));
                }
                if (do_br) {
                    Point p3 = apply_transform(Point{centers[3].x + radius - 1 - cx, centers[3].y + radius - 1 - cy});
                    if (clip.contains(p3)) target_.set_pixel(p3.x, p3.y, Color::lerp(target_.get_pixel(p3.x, p3.y), color, pixel_color.a / 255.0f));
                }
            }
        }
    }
}

void Painter::fill_gradient(const Rect &rect, Color start, Color end, bool vertical) {
    Rect r = apply_transform(rect).intersected(current_clip());
    if (r.is_empty()) return;

    if (vertical) {
        int h = rect.height;
        if (h <= 0) return;
        int base_y = rect.y + origin_.y;

        for (int y = r.y; y < r.bottom(); ++y) {
            float t = static_cast<float>(y - base_y) / static_cast<float>(h);
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            Color line_col = Color::lerp(start, end, t);
            uint32_t pixel_val = line_col.to_argb();

            uint32_t *row_ptr = target_.scanline(y) + r.x;
            std::fill_n(row_ptr, r.width, pixel_val);
        }
        return;
    }

    pixman_point_fixed_t p1, p2;
    p1.x = pixman_int_to_fixed(rect.x);
    p1.y = pixman_int_to_fixed(rect.y);

    p2.x = pixman_int_to_fixed(rect.right());
    p2.y = pixman_int_to_fixed(rect.y);

    pixman_gradient_stop_t stops[2];
    stops[0].x = 0;
    stops[0].color.red = (start.r * 65535) / 255;
    stops[0].color.green = (start.g * 65535) / 255;
    stops[0].color.blue = (start.b * 65535) / 255;
    stops[0].color.alpha = (start.a * 65535) / 255;

    stops[1].x = pixman_int_to_fixed(1);
    stops[1].color.red = (end.r * 65535) / 255;
    stops[1].color.green = (end.g * 65535) / 255;
    stops[1].color.blue = (end.b * 65535) / 255;
    stops[1].color.alpha = (end.a * 65535) / 255;

    pixman_image_t *gradient = pixman_image_create_linear_gradient(&p1, &p2, stops, 2);
    if (!gradient) return;

    pixman_image_composite32(
        PIXMAN_OP_OVER,
        gradient,
        nullptr,
        target_.pixman_image(),
        r.x - origin_.x, r.y - origin_.y,
        0, 0,
        r.x, r.y,
        r.width, r.height
    );

    pixman_image_unref(gradient);
}

void Painter::draw_shadow(const Rect &rect, int blur_radius, Color color) {
    if (blur_radius <= 0 || color.a == 0) return;

    // Soft drop shadow simulation using layered transparent rounded boxes
    for (int i = blur_radius; i > 0; --i) {
        float factor = static_cast<float>(blur_radius - i + 1) / (blur_radius * blur_radius);
        Color c = color.with_opacity(factor * (color.a / 255.0f));
        fill_rounded_rect(
            Rect{rect.x - i, rect.y - i + 2, rect.width + 2 * i, rect.height + 2 * i},
            8 + i,
            c
        );
    }
}

void Painter::draw_line(Point p1, Point p2, Color color, int thickness) {
    if (color.a == 0) return;
    if (thickness <= 0) thickness = 1;

    // Fast-path: horizontal lines
    if (p1.y == p2.y) {
        int x_min = std::min(p1.x, p2.x);
        int w = std::abs(p2.x - p1.x);
        fill_rect(Rect{x_min, p1.y, w, thickness}, color);
        return;
    }

    // Fast-path: vertical lines
    if (p1.x == p2.x) {
        int y_min = std::min(p1.y, p2.y);
        int h = std::abs(p2.y - p1.y);
        fill_rect(Rect{p1.x, y_min, thickness, h}, color);
        return;
    }

    p1 = apply_transform(p1);
    p2 = apply_transform(p2);
    Rect clip = current_clip();

    int dx = std::abs(p2.x - p1.x);
    int dy = std::abs(p2.y - p1.y);
    int sx = p1.x < p2.x ? 1 : -1;
    int sy = p1.y < p2.y ? 1 : -1;
    int err = dx - dy;

    int x = p1.x;
    int y = p1.y;

    while (true) {
        if (clip.contains(Point{x, y})) {
            target_.set_pixel(x, y, color);
        }
        if (x == p2.x && y == p2.y) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

void Painter::draw_bitmap(Point pos, const BitmapSurface &src) {
    draw_bitmap(pos, src, Rect{0, 0, src.width(), src.height()});
}

void Painter::draw_bitmap(Point pos, const BitmapSurface &src, const Rect &src_rect) {
    Point p = apply_transform(pos);
    Rect dst_rect{p.x, p.y, src_rect.width, src_rect.height};
    Rect visible = dst_rect.intersected(current_clip());
    if (visible.is_empty()) return;

    int src_x_offset = src_rect.x + (visible.x - dst_rect.x);
    int src_y_offset = src_rect.y + (visible.y - dst_rect.y);

    if (src.pixman_image() && target_.pixman_image()) {
        pixman_image_composite(
            PIXMAN_OP_OVER,
            src.pixman_image(),
            nullptr,
            target_.pixman_image(),
            (int16_t)src_x_offset, (int16_t)src_y_offset,
            0, 0,
            (int16_t)visible.x, (int16_t)visible.y,
            (uint16_t)visible.width, (uint16_t)visible.height
        );
    } else {
        for (int y = 0; y < visible.height; ++y) {
            const uint32_t *src_line = src.scanline(src_y_offset + y) + src_x_offset;
            uint32_t *dst_line = target_.scanline(visible.y + y) + visible.x;
            std::memcpy(dst_line, src_line, visible.width * sizeof(uint32_t));
        }
    }
}

void Painter::draw_text(Point pos, std::string_view text, const Font &font, Color color) {
    if (text.empty() || color.a == 0) return;
    ShapedRun run = TextShaper::shape_text(text, font);
    draw_shaped_run(pos, run, font, color);
}

void Painter::draw_shaped_run(Point pos, const ShapedRun &run, const Font &font, Color color) {
    if (run.glyphs.empty() || color.a == 0) return;

    Point base_pt = apply_transform(pos);
    Rect clip = current_clip();

    // Quick bounding box culling
    Rect run_bounds{base_pt.x - 4, base_pt.y - 4, run.total_width + 8, run.metrics.height + 8};
    if (!run_bounds.intersects(clip)) {
        return;
    }

    pixman_color_t pcolor = to_pixman_color(color);
    pixman_image_t *solid = pixman_image_create_solid_fill(&pcolor);
    if (!solid) return;

    int pen_x = base_pt.x;
    int pen_y = base_pt.y + run.metrics.ascent;

    for (const auto &glyph : run.glyphs) {
        auto bitmap = GlyphCache::instance().get_glyph(font, glyph.glyph_index);
        if (bitmap && bitmap->width > 0 && bitmap->height > 0) {
            int gx = pen_x + glyph.x_offset + bitmap->bearing_x;
            int gy = pen_y - glyph.y_offset - bitmap->bearing_y;

            Rect glyph_rect = Rect{gx, gy, bitmap->width, bitmap->height}.intersected(clip);
            if (!glyph_rect.is_empty()) {
                pixman_image_t *mask = pixman_image_create_bits(
                    PIXMAN_a8,
                    bitmap->width,
                    bitmap->height,
                    bitmap->buffer.data(),
                    bitmap->stride
                );

                if (mask) {
                    pixman_image_composite32(
                        PIXMAN_OP_OVER,
                        solid,
                        mask,
                        target_.pixman_image(),
                        0, 0,
                        glyph_rect.x - gx, glyph_rect.y - gy,
                        glyph_rect.x, glyph_rect.y,
                        glyph_rect.width, glyph_rect.height
                    );
                    pixman_image_unref(mask);
                }
            }
        }
        pen_x += glyph.x_advance;
        pen_y += glyph.y_advance;
    }

    pixman_image_unref(solid);
}

void Painter::draw_text(const Rect &rect, std::string_view text, const Font &font, Color color,
                        TextAlignment align, VerticalAlignment valig) {
    if (text.empty() || color.a == 0) return;

    Rect transformed_rect = apply_transform(rect);
    if (!transformed_rect.intersects(current_clip())) {
        return;
    }

    ShapedRun run = TextShaper::shape_text(text, font);
    if (run.glyphs.empty()) return;

    Point p{rect.x, rect.y};

    // Horizontal alignment
    switch (align) {
        case TextAlignment::Center:
            p.x = rect.x + (rect.width - run.total_width) / 2;
            break;
        case TextAlignment::Right:
            p.x = rect.right() - run.total_width;
            break;
        default:
            p.x = rect.x;
            break;
    }

    // Vertical alignment
    switch (valig) {
        case VerticalAlignment::Center:
            p.y = rect.y + (rect.height - run.metrics.height) / 2;
            break;
        case VerticalAlignment::Bottom:
            p.y = rect.bottom() - run.metrics.height;
            break;
        default:
            p.y = rect.y;
            break;
    }

    draw_shaped_run(p, run, font, color);
}

Size Painter::measure_text(std::string_view text, const Font &font) {
    if (text.empty()) return Size{0, 0};
    ShapedRun run = TextShaper::shape_text(text, font);
    return Size{run.total_width, run.metrics.height};
}

} // namespace SzpontUI

