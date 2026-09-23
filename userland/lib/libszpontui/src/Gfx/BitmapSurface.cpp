#include <SzpontUI/Gfx/BitmapSurface.hpp>
#include <pixman.h>
#include <cstring>

namespace SzpontUI {

BitmapSurface::BitmapSurface() = default;

BitmapSurface::BitmapSurface(int width, int height) {
    resize(width, height);
}

BitmapSurface::~BitmapSurface() {
    release_pixman_image();
}

BitmapSurface::BitmapSurface(BitmapSurface &&o) noexcept
    : width_(o.width_), height_(o.height_), stride_(o.stride_),
      pixels_(std::move(o.pixels_)), pixman_image_(o.pixman_image_) {
    o.width_ = 0;
    o.height_ = 0;
    o.stride_ = 0;
    o.pixman_image_ = nullptr;
}

BitmapSurface &BitmapSurface::operator=(BitmapSurface &&o) noexcept {
    if (this != &o) {
        release_pixman_image();
        width_ = o.width_;
        height_ = o.height_;
        stride_ = o.stride_;
        pixels_ = std::move(o.pixels_);
        pixman_image_ = o.pixman_image_;

        o.width_ = 0;
        o.height_ = 0;
        o.stride_ = 0;
        o.pixman_image_ = nullptr;
    }
    return *this;
}

void BitmapSurface::resize(int width, int height) {
    if (width_ == width && height_ == height && !pixels_.empty()) return;

    release_pixman_image();
    width_ = std::max(0, width);
    height_ = std::max(0, height);
    stride_ = width_ * sizeof(uint32_t);

    if (width_ > 0 && height_ > 0) {
        pixels_.assign(width_ * height_, 0);
        init_pixman_image();
    } else {
        pixels_.clear();
    }
}

void BitmapSurface::clear(Color color) {
    if (pixels_.empty()) return;
    uint32_t val = color.to_argb();
    std::fill(pixels_.begin(), pixels_.end(), val);
}

void BitmapSurface::set_pixel(int x, int y, Color color) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        pixels_[y * width_ + x] = color.to_argb();
    }
}

Color BitmapSurface::get_pixel(int x, int y) const {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        return Color::from_argb(pixels_[y * width_ + x]);
    }
    return Color::transparent();
}

void BitmapSurface::init_pixman_image() {
    if (width_ > 0 && height_ > 0 && !pixels_.empty()) {
        pixman_image_ = pixman_image_create_bits(
            PIXMAN_a8r8g8b8,
            width_,
            height_,
            pixels_.data(),
            stride_
        );
    }
}

void BitmapSurface::release_pixman_image() {
    if (pixman_image_) {
        pixman_image_unref(pixman_image_);
        pixman_image_ = nullptr;
    }
}

} // namespace SzpontUI
