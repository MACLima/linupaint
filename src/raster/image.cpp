#include "raster/image.h"

#include <cmath>

namespace lp {

Mask::Mask(int w, int h, bool value)
    : w_(std::max(0, w)), h_(std::max(0, h)), bits_(std::size_t(w_) * h_, value ? 1 : 0)
{
}

Mask Mask::fromPolygon(int w, int h, const std::vector<Point>& poly)
{
    Mask m(w, h, false);
    const std::size_t n = poly.size();
    if (n < 3)
        return m;
    std::vector<double> xs;
    for (int y = 0; y < h; ++y) {
        const double cy = y + 0.5;
        xs.clear();
        for (std::size_t i = 0; i < n; ++i) {
            const Point a = poly[i];
            const Point b = poly[(i + 1) % n];
            const double ay = a.y + 0.5, by = b.y + 0.5;
            if ((ay <= cy && by > cy) || (by <= cy && ay > cy)) {
                const double t = (cy - ay) / (by - ay);
                xs.push_back(a.x + 0.5 + t * (b.x - a.x));
            }
        }
        std::sort(xs.begin(), xs.end());
        for (std::size_t i = 0; i + 1 < xs.size(); i += 2) {
            const int x0 = std::max(0, int(std::ceil(xs[i] - 0.5)));
            const int x1 = std::min(w - 1, int(std::floor(xs[i + 1] - 0.5)));
            for (int x = x0; x <= x1; ++x)
                m.bits_[std::size_t(y) * w + x] = 1;
        }
    }
    // The outline itself belongs to the selection.
    for (std::size_t i = 0; i < n; ++i) {
        Point a = poly[i];
        const Point b = poly[(i + 1) % n];
        const int dx = std::abs(b.x - a.x), sx = a.x < b.x ? 1 : -1;
        const int dy = -std::abs(b.y - a.y), sy = a.y < b.y ? 1 : -1;
        int err = dx + dy;
        for (;;) {
            m.set(a.x, a.y, true);
            if (a == b)
                break;
            const int e2 = 2 * err;
            if (e2 >= dy) { err += dy; a.x += sx; }
            if (e2 <= dx) { err += dx; a.y += sy; }
        }
    }
    return m;
}

Image::Image(int w, int h, Rgba fill)
    : w_(std::max(0, w)), h_(std::max(0, h)), px_(std::size_t(w_) * h_, fill)
{
}

void Image::fill(Rgba c)
{
    std::fill(px_.begin(), px_.end(), c);
}

void Image::fillRect(Rect r, Rgba c)
{
    r = r.intersected(bounds());
    for (int y = r.y; y < r.bottom(); ++y)
        std::fill_n(scanLine(y) + r.x, r.w, c);
}

void Image::fillMasked(Rect r, const Mask& mask, Rgba c)
{
    const Rect clip = r.intersected(bounds());
    for (int y = clip.y; y < clip.bottom(); ++y) {
        Rgba* line = scanLine(y);
        for (int x = clip.x; x < clip.right(); ++x)
            if (mask.test(x - r.x, y - r.y))
                line[x] = c;
    }
}

Image Image::copy(Rect r, Rgba outside) const
{
    Image out(r.w, r.h, outside);
    const Rect clip = r.intersected(bounds());
    for (int y = clip.y; y < clip.bottom(); ++y)
        std::copy_n(scanLine(y) + clip.x, clip.w, out.scanLine(y - r.y) + (clip.x - r.x));
    return out;
}

void Image::blit(const Image& src, Point at)
{
    const Rect clip = Rect{at.x, at.y, src.width(), src.height()}.intersected(bounds());
    for (int y = clip.y; y < clip.bottom(); ++y)
        std::copy_n(src.scanLine(y - at.y) + (clip.x - at.x), clip.w, scanLine(y) + clip.x);
}

void Image::blit(const Image& src, Point at, const Mask* mask, bool useKey, Rgba key)
{
    if (!mask && !useKey) {
        blit(src, at);
        return;
    }
    const Rect clip = Rect{at.x, at.y, src.width(), src.height()}.intersected(bounds());
    for (int y = clip.y; y < clip.bottom(); ++y) {
        const int sy = y - at.y;
        const Rgba* s = src.scanLine(sy);
        Rgba* d = scanLine(y);
        for (int x = clip.x; x < clip.right(); ++x) {
            const int sx = x - at.x;
            if (mask && !mask->test(sx, sy))
                continue;
            const Rgba c = s[sx];
            if (useKey && c == key)
                continue;
            d[x] = c;
        }
    }
}

} // namespace lp
