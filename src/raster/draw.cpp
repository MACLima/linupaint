#include "raster/draw.h"

#include <cmath>
#include <cstdlib>
#include <optional>

namespace lp {

namespace {

struct Span {
    int l;
    int r; // inclusive
};

Rect footprintBounds(const std::vector<Point>& fp)
{
    if (fp.empty())
        return {};
    int x0 = fp[0].x, y0 = fp[0].y, x1 = x0, y1 = y0;
    for (const Point& p : fp) {
        x0 = std::min(x0, p.x);
        y0 = std::min(y0, p.y);
        x1 = std::max(x1, p.x);
        y1 = std::max(y1, p.y);
    }
    return {x0, y0, x1 - x0 + 1, y1 - y0 + 1};
}

void hline(Image& img, int y, int x0, int x1, Rgba c)
{
    if (y < 0 || y >= img.height())
        return;
    x0 = std::max(x0, 0);
    x1 = std::min(x1, img.width() - 1);
    if (x0 > x1)
        return;
    std::fill(img.scanLine(y) + x0, img.scanLine(y) + x1 + 1, c);
}

std::optional<Span> ellipseSpan(const Rect& r, int y)
{
    if (r.empty() || y < r.y || y >= r.bottom())
        return std::nullopt;
    const double a = r.w / 2.0;
    const double b = r.h / 2.0;
    const double cx = r.x + a;
    const double cy = r.y + b;
    const double py = (y + 0.5 - cy) / b;
    const double t = std::max(0.0, 1.0 - py * py);
    const double dx = a * std::sqrt(t);
    int l = int(std::ceil(cx - dx - 0.5));
    int rr = int(std::floor(cx + dx - 0.5));
    if (l > rr) {
        // Very flat rows still get the pixel(s) nearest the axis so the outline stays closed.
        l = int(std::floor(cx - 0.5));
        rr = int(std::ceil(cx - 0.5));
    }
    return Span{std::max(l, r.x), std::min(rr, r.right() - 1)};
}

std::optional<Span> rectSpan(const Rect& r, int y)
{
    if (r.empty() || y < r.y || y >= r.bottom())
        return std::nullopt;
    return Span{r.x, r.right() - 1};
}

int roundedRadius(const Rect& r)
{
    return std::max(0, std::min({8, r.w / 2, r.h / 2}));
}

std::optional<Span> roundedSpan(const Rect& r, int y, int radius)
{
    if (r.empty() || y < r.y || y >= r.bottom())
        return std::nullopt;
    if (radius <= 0)
        return Span{r.x, r.right() - 1};
    const int d = radius * 2;
    std::optional<Span> corner;
    if (y < r.y + radius)
        corner = ellipseSpan({r.x, r.y, d, d}, y);
    else if (y >= r.bottom() - radius)
        corner = ellipseSpan({r.x, r.bottom() - d, d, d}, y);
    if (!corner)
        return Span{r.x, r.right() - 1};
    const int inset = corner->l - r.x;
    return Span{r.x + inset, r.right() - 1 - inset};
}

template <typename OuterFn, typename InnerFn>
Rect drawSpanShape(Image& img, Rect r, const ShapeStyle& s, OuterFn outerSpan, InnerFn innerSpan)
{
    if (r.empty())
        return {};
    const int t = std::max(1, s.lineWidth);
    const Rect inner = r.adjusted(-t);
    std::vector<std::optional<Span>> outer(std::size_t(r.h));
    for (int y = r.y; y < r.bottom(); ++y)
        outer[std::size_t(y - r.y)] = outerSpan(r, y);
    auto outerAt = [&](int y) -> std::optional<Span> {
        if (y < r.y || y >= r.bottom())
            return std::nullopt;
        return outer[std::size_t(y - r.y)];
    };
    for (int y = r.y; y < r.bottom(); ++y) {
        const auto o = outerAt(y);
        if (!o)
            continue;
        if (s.fill == FillStyle::Fill) {
            hline(img, y, o->l, o->r, s.interior);
            continue;
        }
        const auto i = inner.empty() ? std::nullopt : innerSpan(inner, y);
        if (!i || i->l > i->r) {
            hline(img, y, o->l, o->r, s.outline);
            continue;
        }
        if (s.fill == FillStyle::OutlineAndFill)
            hline(img, y, i->l, i->r, s.interior);
        // Reach the neighbouring rows' edges so the outline stays 8-connected on flat curves.
        int leftEnd = i->l - 1;
        int rightStart = i->r + 1;
        for (const int ny : {y - 1, y + 1}) {
            if (const auto n = outerAt(ny)) {
                leftEnd = std::max(leftEnd, n->l - 1);
                rightStart = std::min(rightStart, n->r + 1);
            }
        }
        leftEnd = std::min(leftEnd, o->r);
        rightStart = std::max(rightStart, o->l);
        hline(img, y, o->l, leftEnd, s.outline);
        hline(img, y, rightStart, o->r, s.outline);
    }
    return r.intersected(img.bounds());
}

int sgn(int v) { return v < 0 ? -1 : 1; }

} // namespace

std::vector<Point> penFootprint(int width)
{
    width = std::clamp(width, 1, 64);
    if (width == 1)
        return {{0, 0}};
    if (width == 2)
        return {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    const int lo = -((width - 1) / 2);
    const double c = lo + (width - 1) / 2.0;
    const int k = width / 2;
    const double limit = (width % 2) ? double(k * k + k) : double(k * k);
    std::vector<Point> fp;
    for (int dy = lo; dy < lo + width; ++dy)
        for (int dx = lo; dx < lo + width; ++dx) {
            const double ex = dx - c;
            const double ey = dy - c;
            if (ex * ex + ey * ey <= limit)
                fp.push_back({dx, dy});
        }
    return fp;
}

Brush classicBrush(int index)
{
    index = std::clamp(index, 0, 11);
    static constexpr BrushShape shapes[] = {BrushShape::Round, BrushShape::Square, BrushShape::Slash,
                                            BrushShape::Backslash};
    static constexpr int roundSizes[] = {7, 4, 1};
    static constexpr int otherSizes[] = {8, 5, 2};
    const BrushShape shape = shapes[index / 3];
    const int size = shape == BrushShape::Round ? roundSizes[index % 3] : otherSizes[index % 3];
    return {shape, size};
}

std::vector<Point> brushFootprint(const Brush& brush)
{
    const int n = std::max(1, brush.size);
    std::vector<Point> fp;
    switch (brush.shape) {
    case BrushShape::Round:
        return penFootprint(n);
    case BrushShape::Square:
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x)
                fp.push_back({x - n / 2, y - n / 2});
        break;
    case BrushShape::Slash:
        for (int k = 0; k < n; ++k)
            fp.push_back({k - n / 2, (n - 1 - k) - n / 2});
        break;
    case BrushShape::Backslash:
        for (int k = 0; k < n; ++k)
            fp.push_back({k - n / 2, k - n / 2});
        break;
    }
    return fp;
}

Rect stamp(Image& img, Point center, const std::vector<Point>& footprint, Rgba c)
{
    for (const Point& p : footprint)
        img.setPixelSafe(center.x + p.x, center.y + p.y, c);
    return footprintBounds(footprint).translated(center).intersected(img.bounds());
}

Rect drawFootprintLine(Image& img, Point a, Point b, const std::vector<Point>& footprint, Rgba c)
{
    if (footprint.size() == 1 && footprint[0] == Point{0, 0}) {
        forEachLinePoint(a, b, [&](Point p) { img.setPixelSafe(p.x, p.y, c); });
    } else {
        forEachLinePoint(a, b, [&](Point p) {
            for (const Point& o : footprint)
                img.setPixelSafe(p.x + o.x, p.y + o.y, c);
        });
    }
    const Rect fb = footprintBounds(footprint);
    const Rect span = Rect::fromPoints(a, b);
    return Rect{span.x + fb.x, span.y + fb.y, span.w + fb.w - 1, span.h + fb.h - 1}.intersected(img.bounds());
}

Rect drawLine(Image& img, Point a, Point b, Rgba c, int width)
{
    return drawFootprintLine(img, a, b, penFootprint(width), c);
}

Point constrainTo45(Point from, Point to)
{
    const int dx = to.x - from.x;
    const int dy = to.y - from.y;
    const int adx = std::abs(dx);
    const int ady = std::abs(dy);
    constexpr double tan22 = 0.41421356;
    if (ady <= adx * tan22)
        return {to.x, from.y};
    if (adx <= ady * tan22)
        return {from.x, to.y};
    const int d = std::max(adx, ady);
    return {from.x + sgn(dx) * d, from.y + sgn(dy) * d};
}

Point constrainSquare(Point from, Point to)
{
    const int dx = to.x - from.x;
    const int dy = to.y - from.y;
    const int d = std::max(std::abs(dx), std::abs(dy));
    return {from.x + sgn(dx) * d, from.y + sgn(dy) * d};
}

Rect drawRectangle(Image& img, Rect r, const ShapeStyle& s)
{
    return drawSpanShape(img, r, s, rectSpan, rectSpan);
}

Rect drawEllipse(Image& img, Rect r, const ShapeStyle& s)
{
    return drawSpanShape(img, r, s, ellipseSpan, ellipseSpan);
}

Rect drawRoundedRect(Image& img, Rect r, const ShapeStyle& s)
{
    const int outerR = roundedRadius(r);
    const int innerR = std::max(0, outerR - std::max(1, s.lineWidth));
    return drawSpanShape(
        img, r, s, [outerR](const Rect& rr, int y) { return roundedSpan(rr, y, outerR); },
        [innerR](const Rect& rr, int y) { return roundedSpan(rr, y, innerR); });
}

Rect drawPolyline(Image& img, const std::vector<Point>& pts, Rgba c, int width)
{
    if (pts.empty())
        return {};
    const auto fp = penFootprint(width);
    Rect dirty = stamp(img, pts[0], fp, c);
    for (std::size_t i = 1; i < pts.size(); ++i)
        dirty = dirty.united(drawFootprintLine(img, pts[i - 1], pts[i], fp, c));
    return dirty;
}

Rect drawPolygon(Image& img, const std::vector<Point>& pts, const ShapeStyle& s)
{
    if (pts.empty())
        return {};
    int x0 = pts[0].x, y0 = pts[0].y, x1 = x0, y1 = y0;
    for (const Point& p : pts) {
        x0 = std::min(x0, p.x);
        y0 = std::min(y0, p.y);
        x1 = std::max(x1, p.x);
        y1 = std::max(y1, p.y);
    }
    const Rect box{x0, y0, x1 - x0 + 1, y1 - y0 + 1};
    Rect dirty;
    if (s.fill != FillStyle::Outline && pts.size() >= 3) {
        std::vector<Point> local;
        local.reserve(pts.size());
        for (const Point& p : pts)
            local.push_back({p.x - x0, p.y - y0});
        img.fillMasked(box, Mask::fromPolygon(box.w, box.h, local), s.interior);
        dirty = box.intersected(img.bounds());
    }
    if (s.fill != FillStyle::Fill) {
        std::vector<Point> closed = pts;
        closed.push_back(pts.front());
        dirty = dirty.united(drawPolyline(img, closed, s.outline, s.lineWidth));
    }
    return dirty;
}

Rect drawBezier(Image& img, Point p0, Point c1, Point c2, Point p3, Rgba c, int width)
{
    auto dist = [](Point a, Point b) { return std::hypot(double(b.x - a.x), double(b.y - a.y)); };
    const double len = dist(p0, c1) + dist(c1, c2) + dist(c2, p3);
    const int steps = std::max(8, int(len / 2.0));
    std::vector<Point> pts;
    pts.reserve(steps + 1);
    for (int i = 0; i <= steps; ++i) {
        const double t = double(i) / steps;
        const double u = 1.0 - t;
        const double b0 = u * u * u, b1 = 3 * u * u * t, b2 = 3 * u * t * t, b3 = t * t * t;
        const Point p{int(std::lround(b0 * p0.x + b1 * c1.x + b2 * c2.x + b3 * p3.x)),
                      int(std::lround(b0 * p0.y + b1 * c1.y + b2 * c2.y + b3 * p3.y))};
        if (pts.empty() || !(pts.back() == p))
            pts.push_back(p);
    }
    return drawPolyline(img, pts, c, width);
}

Rect replaceColor(Image& img, Point center, const std::vector<Point>& footprint, Rgba from, Rgba to)
{
    for (const Point& p : footprint) {
        const int x = center.x + p.x, y = center.y + p.y;
        if (img.contains(x, y) && img.pixel(x, y) == from)
            img.setPixel(x, y, to);
    }
    return footprintBounds(footprint).translated(center).intersected(img.bounds());
}

Rect spray(Image& img, Point center, int diameter, int count, Rgba c, std::mt19937& rng)
{
    const int r = std::max(1, diameter / 2);
    std::uniform_int_distribution<int> d(-r, r);
    for (int i = 0; i < count; ++i) {
        int dx, dy;
        do {
            dx = d(rng);
            dy = d(rng);
        } while (dx * dx + dy * dy > r * r);
        img.setPixelSafe(center.x + dx, center.y + dy, c);
    }
    return Rect{center.x - r, center.y - r, 2 * r + 1, 2 * r + 1}.intersected(img.bounds());
}

} // namespace lp
