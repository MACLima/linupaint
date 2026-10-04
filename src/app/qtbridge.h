#pragma once

#include "raster/image.h"

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QRect>

namespace app {

inline QRect toQRect(const lp::Rect& r) { return {r.x, r.y, r.w, r.h}; }
inline lp::Rect toRect(const QRect& r) { return {r.x(), r.y(), r.width(), r.height()}; }
inline QPoint toQPoint(lp::Point p) { return {p.x, p.y}; }
inline lp::Point toPoint(QPoint p) { return {p.x(), p.y()}; }
inline QColor toQColor(lp::Rgba c) { return QColor::fromRgb(c); }
inline lp::Rgba toRgba(const QColor& c) { return lp::Rgba(c.rgb()) | 0xFF000000u; }

// Zero-copy view of the image; valid while the image is alive and not resized.
inline QImage wrap(const lp::Image& img)
{
    return QImage(reinterpret_cast<const uchar*>(img.data()), img.width(), img.height(), img.width() * 4,
                  QImage::Format_ARGB32);
}

inline QImage toQImage(const lp::Image& img) { return wrap(img).copy(); }

// Flattens transparency over `background` (Paint images are always opaque).
lp::Image fromQImage(const QImage& src, lp::Rgba background = lp::kWhite);

} // namespace app
