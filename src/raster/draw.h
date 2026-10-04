#pragma once

#include "raster/image.h"

#include <random>
#include <vector>

namespace lp {

// Calls f(Point) for every pixel of the Bresenham line from a to b (inclusive).
template <typename F>
void forEachLinePoint(Point a, Point b, F&& f)
{
    const int dx = a.x < b.x ? b.x - a.x : a.x - b.x;
    const int sx = a.x < b.x ? 1 : -1;
    const int dy = a.y < b.y ? a.y - b.y : b.y - a.y;
    const int sy = a.y < b.y ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        f(a);
        if (a == b)
            break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; a.x += sx; }
        if (e2 <= dx) { err += dx; a.y += sy; }
    }
}

enum class BrushShape { Round, Square, Slash, Backslash };

struct Brush {
    BrushShape shape = BrushShape::Round;
    int size = 1;
};

// The 12 brush tips of the classic Paint brush tool, in toolbox order (4 rows x 3 sizes).
Brush classicBrush(int index);

// Pixels covered by a brush stamped at the origin.
std::vector<Point> brushFootprint(const Brush& brush);

// Pen footprint used by lines and shape outlines of the given width (1..5).
std::vector<Point> penFootprint(int width);

// All functions below clip to the image and return the touched rect (in image coordinates).
Rect stamp(Image& img, Point center, const std::vector<Point>& footprint, Rgba c);
Rect drawLine(Image& img, Point a, Point b, Rgba c, int width = 1);
Rect drawFootprintLine(Image& img, Point a, Point b, const std::vector<Point>& footprint, Rgba c);

// Snaps `to` so that the segment from->to is horizontal, vertical or diagonal (Shift).
Point constrainTo45(Point from, Point to);
// Turns the rect spanned by from->to into a square keeping the drag direction (Shift).
Point constrainSquare(Point from, Point to);

enum class FillStyle { Outline, OutlineAndFill, Fill };

struct ShapeStyle {
    FillStyle fill = FillStyle::Outline;
    int lineWidth = 1;
    Rgba outline = kBlack;
    Rgba interior = kWhite;
};

Rect drawRectangle(Image& img, Rect r, const ShapeStyle& s);
Rect drawEllipse(Image& img, Rect r, const ShapeStyle& s);
Rect drawRoundedRect(Image& img, Rect r, const ShapeStyle& s);
Rect drawPolygon(Image& img, const std::vector<Point>& pts, const ShapeStyle& s);
Rect drawPolyline(Image& img, const std::vector<Point>& pts, Rgba c, int width);
Rect drawBezier(Image& img, Point p0, Point c1, Point c2, Point p3, Rgba c, int width);

// Exact-match, 4-connected flood fill.
Rect floodFill(Image& img, Point seed, Rgba c);

// Replaces `from` with `to` inside the footprint stamped at center (color eraser).
Rect replaceColor(Image& img, Point center, const std::vector<Point>& footprint, Rgba from, Rgba to);

// Classic airbrush: `count` random dots uniformly spread over a disc.
Rect spray(Image& img, Point center, int diameter, int count, Rgba c, std::mt19937& rng);

} // namespace lp
