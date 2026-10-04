#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lp {

// 0xAARRGGBB, the same in-memory layout as QImage::Format_ARGB32.
using Rgba = std::uint32_t;

constexpr Rgba rgb(int r, int g, int b, int a = 255)
{
    return (Rgba(a & 0xFF) << 24) | (Rgba(r & 0xFF) << 16) | (Rgba(g & 0xFF) << 8) | Rgba(b & 0xFF);
}
constexpr int alphaOf(Rgba c) { return int(c >> 24); }
constexpr int redOf(Rgba c) { return int((c >> 16) & 0xFF); }
constexpr int greenOf(Rgba c) { return int((c >> 8) & 0xFF); }
constexpr int blueOf(Rgba c) { return int(c & 0xFF); }

constexpr Rgba kBlack = rgb(0, 0, 0);
constexpr Rgba kWhite = rgb(255, 255, 255);

struct Point {
    int x = 0;
    int y = 0;
    friend bool operator==(const Point&, const Point&) = default;
    Point operator+(Point o) const { return {x + o.x, y + o.y}; }
    Point operator-(Point o) const { return {x - o.x, y - o.y}; }
};

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    friend bool operator==(const Rect&, const Rect&) = default;

    bool empty() const { return w <= 0 || h <= 0; }
    int right() const { return x + w; }   // exclusive
    int bottom() const { return y + h; }  // exclusive
    bool contains(Point p) const { return p.x >= x && p.y >= y && p.x < right() && p.y < bottom(); }

    // Smallest rect holding both points (inclusive).
    static Rect fromPoints(Point a, Point b)
    {
        const int x0 = std::min(a.x, b.x);
        const int y0 = std::min(a.y, b.y);
        return {x0, y0, std::max(a.x, b.x) - x0 + 1, std::max(a.y, b.y) - y0 + 1};
    }

    Rect united(const Rect& o) const
    {
        if (empty())
            return o;
        if (o.empty())
            return *this;
        const int x0 = std::min(x, o.x);
        const int y0 = std::min(y, o.y);
        return {x0, y0, std::max(right(), o.right()) - x0, std::max(bottom(), o.bottom()) - y0};
    }

    Rect intersected(const Rect& o) const
    {
        const int x0 = std::max(x, o.x);
        const int y0 = std::max(y, o.y);
        const int x1 = std::min(right(), o.right());
        const int y1 = std::min(bottom(), o.bottom());
        if (x1 <= x0 || y1 <= y0)
            return {};
        return {x0, y0, x1 - x0, y1 - y0};
    }

    Rect adjusted(int d) const { return {x - d, y - d, w + 2 * d, h + 2 * d}; }
    Rect translated(Point p) const { return {x + p.x, y + p.y, w, h}; }
};

// One byte per pixel; non-zero means "inside".
class Mask {
public:
    Mask() = default;
    Mask(int w, int h, bool value = false);

    int width() const { return w_; }
    int height() const { return h_; }
    bool isNull() const { return w_ == 0 || h_ == 0; }
    bool test(int x, int y) const
    {
        return x >= 0 && y >= 0 && x < w_ && y < h_ && bits_[std::size_t(y) * w_ + x] != 0;
    }
    void set(int x, int y, bool v)
    {
        if (x >= 0 && y >= 0 && x < w_ && y < h_)
            bits_[std::size_t(y) * w_ + x] = v ? 1 : 0;
    }

    // Rasterizes a closed polygon (even-odd, pixel centers) with coordinates relative to the mask.
    static Mask fromPolygon(int w, int h, const std::vector<Point>& poly);

    friend bool operator==(const Mask&, const Mask&) = default;

private:
    int w_ = 0;
    int h_ = 0;
    std::vector<std::uint8_t> bits_;
};

class Image {
public:
    Image() = default;
    Image(int w, int h, Rgba fill = kWhite);

    int width() const { return w_; }
    int height() const { return h_; }
    bool isNull() const { return w_ == 0 || h_ == 0; }
    Rect bounds() const { return {0, 0, w_, h_}; }
    bool contains(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }
    std::size_t byteSize() const { return px_.size() * sizeof(Rgba); }

    Rgba* data() { return px_.data(); }
    const Rgba* data() const { return px_.data(); }
    Rgba* scanLine(int y) { return px_.data() + std::size_t(y) * w_; }
    const Rgba* scanLine(int y) const { return px_.data() + std::size_t(y) * w_; }

    Rgba pixel(int x, int y) const { return px_[std::size_t(y) * w_ + x]; }
    void setPixel(int x, int y, Rgba c) { px_[std::size_t(y) * w_ + x] = c; }
    void setPixelSafe(int x, int y, Rgba c)
    {
        if (contains(x, y))
            setPixel(x, y, c);
    }

    void fill(Rgba c);
    void fillRect(Rect r, Rgba c);
    // Fills the pixels of `r` selected by `mask` (mask coordinates relative to r).
    void fillMasked(Rect r, const Mask& mask, Rgba c);

    // Pixels outside the image come back as `outside`.
    Image copy(Rect r, Rgba outside = kWhite) const;

    // Copies `src` to `at`, clipped to this image. Optional mask (src-sized) and transparent key.
    void blit(const Image& src, Point at);
    void blit(const Image& src, Point at, const Mask* mask, bool useKey, Rgba key);

    // Alpha-composites `src` over this image (used for pasted pictures with soft edges, e.g. emojis).
    void blendOver(const Image& src, Point at);

    friend bool operator==(const Image&, const Image&) = default;

private:
    int w_ = 0;
    int h_ = 0;
    std::vector<Rgba> px_;
};

} // namespace lp
