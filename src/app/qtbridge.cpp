#include "qtbridge.h"

namespace app {

lp::Image fromQImage(const QImage& src, lp::Rgba background)
{
    const QImage q = src.convertToFormat(QImage::Format_ARGB32);
    lp::Image out(q.width(), q.height(), background);
    const int br = lp::redOf(background), bg = lp::greenOf(background), bb = lp::blueOf(background);
    for (int y = 0; y < q.height(); ++y) {
        const auto* s = reinterpret_cast<const QRgb*>(q.constScanLine(y));
        lp::Rgba* d = out.scanLine(y);
        for (int x = 0; x < q.width(); ++x) {
            const QRgb c = s[x];
            const int a = qAlpha(c);
            if (a == 255) {
                d[x] = c;
            } else if (a > 0) {
                d[x] = lp::rgb((qRed(c) * a + br * (255 - a)) / 255, (qGreen(c) * a + bg * (255 - a)) / 255,
                               (qBlue(c) * a + bb * (255 - a)) / 255);
            }
        }
    }
    return out;
}

} // namespace app
