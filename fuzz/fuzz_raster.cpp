#include "raster/codecs.h"
#include "raster/draw.h"
#include "raster/transform.h"

#include <cstdint>

namespace {

struct Reader {
    const std::uint8_t* p;
    std::size_t n;
    int next(int lo, int hi)
    {
        if (n == 0)
            return lo;
        const int v = *p++;
        --n;
        return lo + v % (hi - lo + 1);
    }
};

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    Reader r{data, size};
    lp::Image img(r.next(1, 64), r.next(1, 64));
    while (r.n > 0) {
        const lp::Point a{r.next(-80, 80), r.next(-80, 80)};
        const lp::Point b{r.next(-80, 80), r.next(-80, 80)};
        const lp::Rgba c = lp::rgb(r.next(0, 255), r.next(0, 255), r.next(0, 255));
        lp::ShapeStyle s;
        s.fill = lp::FillStyle(r.next(0, 2));
        s.lineWidth = r.next(1, 5);
        switch (r.next(0, 9)) {
        case 0: lp::drawLine(img, a, b, c, s.lineWidth); break;
        case 1: lp::drawEllipse(img, lp::Rect::fromPoints(a, b), s); break;
        case 2: lp::drawRoundedRect(img, lp::Rect::fromPoints(a, b), s); break;
        case 3: lp::drawPolygon(img, {a, b, {b.x, a.y}}, s); break;
        case 4: lp::drawBezier(img, a, b, {a.x, b.y}, {b.x, a.y}, c, s.lineWidth); break;
        case 5: lp::floodFill(img, a, c); break;
        case 6: img = lp::skew(img, r.next(-60, 60), r.next(-60, 60), c); break;
        case 7: img = lp::rotate(img, 90 * r.next(1, 3)); break;
        case 8: img = lp::stretch(img, r.next(1, 200), r.next(1, 200)); break;
        default: lp::drawFootprintLine(img, a, b, lp::brushFootprint(lp::classicBrush(r.next(0, 11))), c); break;
        }
        if (img.width() > 512 || img.height() > 512)
            img = lp::scaled(img, 64, 64);
    }
    (void)lp::encodeGif(img);
    (void)lp::encodeBmp(img, 4);
    (void)lp::encodeBmp(img, 8);
    return 0;
}
