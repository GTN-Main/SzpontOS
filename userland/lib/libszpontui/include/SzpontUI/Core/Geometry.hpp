#pragma once

#include <algorithm>
#include <cstdint>

namespace SzpontUI {

struct Point {
    int x{0};
    int y{0};

    constexpr Point() = default;
    constexpr Point(int px, int py) : x(px), y(py) {}

    constexpr Point operator+(const Point &o) const { return {x + o.x, y + o.y}; }
    constexpr Point operator-(const Point &o) const { return {x - o.x, y - o.y}; }
    Point &operator+=(const Point &o) { x += o.x; y += o.y; return *this; }
    Point &operator-=(const Point &o) { x -= o.x; y -= o.y; return *this; }
    constexpr bool operator==(const Point &o) const { return x == o.x && y == o.y; }
    constexpr bool operator!=(const Point &o) const { return !(*this == o); }
};

struct Size {
    int width{0};
    int height{0};

    constexpr Size() = default;
    constexpr Size(int w, int h) : width(std::max(0, w)), height(std::max(0, h)) {}

    constexpr bool is_empty() const { return width <= 0 || height <= 0; }
    constexpr bool operator==(const Size &o) const { return width == o.width && height == o.height; }
    constexpr bool operator!=(const Size &o) const { return !(*this == o); }
};

struct Insets {
    int top{0};
    int right{0};
    int bottom{0};
    int left{0};

    constexpr Insets() = default;
    constexpr Insets(int all) : top(all), right(all), bottom(all), left(all) {}
    constexpr Insets(int vert, int horiz) : top(vert), right(horiz), bottom(vert), left(horiz) {}
    constexpr Insets(int t, int r, int b, int l) : top(t), right(r), bottom(b), left(l) {}

    constexpr int horizontal() const { return left + right; }
    constexpr int vertical() const { return top + bottom; }
};

struct Rect {
    int x{0};
    int y{0};
    int width{0};
    int height{0};

    constexpr Rect() = default;
    constexpr Rect(int rx, int ry, int rw, int rh)
        : x(rx), y(ry), width(std::max(0, rw)), height(std::max(0, rh)) {}
    constexpr Rect(Point p, Size s)
        : x(p.x), y(p.y), width(s.width), height(s.height) {}

    constexpr Point pos() const { return {x, y}; }
    constexpr Size size() const { return {width, height}; }

    constexpr int left() const { return x; }
    constexpr int top() const { return y; }
    constexpr int right() const { return x + width; }
    constexpr int bottom() const { return y + height; }

    constexpr bool is_empty() const { return width <= 0 || height <= 0; }

    constexpr bool contains(Point p) const {
        return p.x >= x && p.x < x + width && p.y >= y && p.y < y + height;
    }

    constexpr bool contains(int px, int py) const {
        return contains(Point{px, py});
    }

    constexpr bool intersects(const Rect &o) const {
        return left() < o.right() && right() > o.left() &&
               top() < o.bottom() && bottom() > o.top();
    }

    Rect intersected(const Rect &o) const {
        int nx = std::max(x, o.x);
        int ny = std::max(y, o.y);
        int nr = std::min(right(), o.right());
        int nb = std::min(bottom(), o.bottom());
        if (nr < nx || nb < ny) return Rect{0, 0, 0, 0};
        return Rect{nx, ny, nr - nx, nb - ny};
    }

    Rect united(const Rect &o) const {
        if (is_empty()) return o;
        if (o.is_empty()) return *this;
        int nx = std::min(x, o.x);
        int ny = std::min(y, o.y);
        int nr = std::max(right(), o.right());
        int nb = std::max(bottom(), o.bottom());
        return Rect{nx, ny, nr - nx, nb - ny};
    }

    Rect translated(int dx, int dy) const {
        return Rect{x + dx, y + dy, width, height};
    }

    Rect translated(Point delta) const {
        return translated(delta.x, delta.y);
    }

    Rect shrunk_by(const Insets &in) const {
        return Rect{x + in.left, y + in.top,
                    std::max(0, width - in.horizontal()),
                    std::max(0, height - in.vertical())};
    }

    Rect expanded_by(const Insets &in) const {
        return Rect{x - in.left, y - in.top,
                    width + in.horizontal(),
                    height + in.vertical()};
    }

    constexpr bool operator==(const Rect &o) const {
        return x == o.x && y == o.y && width == o.width && height == o.height;
    }
    constexpr bool operator!=(const Rect &o) const { return !(*this == o); }
};

} // namespace SzpontUI
