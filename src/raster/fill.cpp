#include "raster/draw.h"

namespace lp {

Rect floodFill(Image& img, Point seed, Rgba c)
{
    if (!img.contains(seed.x, seed.y))
        return {};
    const Rgba target = img.pixel(seed.x, seed.y);
    if (target == c)
        return {};

    const int w = img.width();
    const int h = img.height();
    int minX = seed.x, maxX = seed.x, minY = seed.y, maxY = seed.y;
    std::vector<Point> stack;
    stack.reserve(1024);
    stack.push_back(seed);

    while (!stack.empty()) {
        const Point p = stack.back();
        stack.pop_back();
        Rgba* line = img.scanLine(p.y);
        if (line[p.x] != target)
            continue;
        int xl = p.x;
        while (xl > 0 && line[xl - 1] == target)
            --xl;
        int xr = p.x;
        while (xr < w - 1 && line[xr + 1] == target)
            ++xr;
        std::fill(line + xl, line + xr + 1, c);
        minX = std::min(minX, xl);
        maxX = std::max(maxX, xr);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);

        for (const int ny : {p.y - 1, p.y + 1}) {
            if (ny < 0 || ny >= h)
                continue;
            const Rgba* nl = img.scanLine(ny);
            bool inRun = false;
            for (int x = xl; x <= xr; ++x) {
                if (nl[x] == target) {
                    if (!inRun) {
                        stack.push_back({x, ny});
                        inRun = true;
                    }
                } else {
                    inRun = false;
                }
            }
        }
    }
    return {minX, minY, maxX - minX + 1, maxY - minY + 1};
}

} // namespace lp
