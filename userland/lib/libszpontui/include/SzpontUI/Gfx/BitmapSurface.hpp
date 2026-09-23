#pragma once

#include <SzpontUI/Core/Geometry.hpp>
#include <SzpontUI/Core/Color.hpp>
#include <vector>
#include <cstdint>
#include <memory>

// Forward declaration for pixman_image_t
typedef union pixman_image pixman_image_t;

namespace SzpontUI {

class BitmapSurface {
public:
    BitmapSurface();
    BitmapSurface(int width, int height);
    ~BitmapSurface();

    // Movable, non-copyable
    BitmapSurface(const BitmapSurface &) = delete;
    BitmapSurface &operator=(const BitmapSurface &) = delete;
    BitmapSurface(BitmapSurface &&) noexcept;
    BitmapSurface &operator=(BitmapSurface &&) noexcept;

    void resize(int width, int height);
    void clear(Color color = Color::transparent());

    int width() const { return width_; }
    int height() const { return height_; }
    int stride() const { return stride_; }
    Size size() const { return Size{width_, height_}; }
    Rect rect() const { return Rect{0, 0, width_, height_}; }

    uint32_t *pixels() { return pixels_.data(); }
    const uint32_t *pixels() const { return pixels_.data(); }
    uint32_t *scanline(int y) { return pixels_.data() + y * (stride_ / 4); }
    const uint32_t *scanline(int y) const { return pixels_.data() + y * (stride_ / 4); }

    pixman_image_t *pixman_image() const { return pixman_image_; }

    void set_pixel(int x, int y, Color color);
    Color get_pixel(int x, int y) const;

private:
    void init_pixman_image();
    void release_pixman_image();

    int width_{0};
    int height_{0};
    int stride_{0};
    std::vector<uint32_t> pixels_;
    pixman_image_t *pixman_image_{nullptr};
};

} // namespace SzpontUI
