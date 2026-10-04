#include "raster/transform.h"

#include <cmath>

namespace lp {

namespace {

// Pixel access shared by Image and Mask so the geometric transforms are written once.
struct ImageAccess {
    static Image make(int w, int h) { return Image(w, h); }
    static Rgba get(const Image& i, int x, int y) { return i.pixel(x, y); }
    static void put(Image& i, int x, int y, Rgba v) { i.setPixel(x, y, v); }
};
struct MaskAccess {
    static Mask make(int w, int h) { return Mask(w, h); }
    static bool get(const Mask& m, int x, int y) { return m.test(x, y); }
    static void put(Mask& m, int x, int y, bool v) { m.set(x, y, v); }
};

template <typename A, typename T>
T flipH(const T& src)
{
    const int w = src.width(), h = src.height();
    T out = A::make(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            A::put(out, w - 1 - x, y, A::get(src, x, y));
    return out;
}

template <typename A, typename T>
T flipV(const T& src)
{
    const int w = src.width(), h = src.height();
    T out = A::make(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            A::put(out, x, h - 1 - y, A::get(src, x, y));
    return out;
}

template <typename A, typename T>
T rot(const T& src, int degrees)
{
    const int w = src.width(), h = src.height();
    degrees = ((degrees % 360) + 360) % 360;
    if (degrees == 180) {
        T out = A::make(w, h);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                A::put(out, w - 1 - x, h - 1 - y, A::get(src, x, y));
        return out;
    }
    if (degrees == 90 || degrees == 270) {
        T out = A::make(h, w);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                if (degrees == 90)
                    A::put(out, h - 1 - y, x, A::get(src, x, y));
                else
                    A::put(out, y, w - 1 - x, A::get(src, x, y));
            }
        return out;
    }
    return src;
}

template <typename A, typename T>
T scale(const T& src, int w, int h)
{
    w = std::max(1, w);
    h = std::max(1, h);
    T out = A::make(w, h);
    if (src.width() == 0 || src.height() == 0)
        return out;
    for (int y = 0; y < h; ++y) {
        const int sy = int((long long)y * src.height() / h);
        for (int x = 0; x < w; ++x) {
            const int sx = int((long long)x * src.width() / w);
            A::put(out, x, y, A::get(src, sx, sy));
        }
    }
    return out;
}

} // namespace

Image flipHorizontal(const Image& img)
{
    Image out = img;
    for (int y = 0; y < out.height(); ++y)
        std::reverse(out.scanLine(y), out.scanLine(y) + out.width());
    return out;
}

Image flipVertical(const Image& img)
{
    Image out(img.width(), img.height());
    for (int y = 0; y < img.height(); ++y)
        std::copy_n(img.scanLine(y), img.width(), out.scanLine(img.height() - 1 - y));
    return out;
}

Image rotate(const Image& img, int degrees) { return rot<ImageAccess>(img, degrees); }
Image scaled(const Image& img, int w, int h) { return scale<ImageAccess>(img, w, h); }

Image stretch(const Image& img, int percentX, int percentY)
{
    const int w = std::max(1, int(std::lround(img.width() * percentX / 100.0)));
    const int h = std::max(1, int(std::lround(img.height() * percentY / 100.0)));
    return scaled(img, w, h);
}

Image skew(const Image& img, int degreesX, int degreesY, Rgba background)
{
    Image cur = img;
    if (degreesX != 0) {
        const double t = std::tan(degreesX * 3.14159265358979323846 / 180.0);
        const int span = int(std::lround(std::abs(t) * (cur.height() - 1)));
        Image out(cur.width() + span, cur.height(), background);
        for (int y = 0; y < cur.height(); ++y) {
            int off = int(std::lround(t * y));
            if (t < 0)
                off += span;
            std::copy_n(cur.scanLine(y), cur.width(), out.scanLine(y) + off);
        }
        cur = std::move(out);
    }
    if (degreesY != 0) {
        const double t = std::tan(degreesY * 3.14159265358979323846 / 180.0);
        const int span = int(std::lround(std::abs(t) * (cur.width() - 1)));
        Image out(cur.width(), cur.height() + span, background);
        for (int x = 0; x < cur.width(); ++x) {
            int off = int(std::lround(t * x));
            if (t < 0)
                off += span;
            for (int y = 0; y < cur.height(); ++y)
                out.setPixel(x, y + off, cur.pixel(x, y));
        }
        cur = std::move(out);
    }
    return cur;
}

Image invertColors(const Image& img)
{
    Image out = img;
    Rgba* p = out.data();
    const std::size_t n = std::size_t(out.width()) * out.height();
    for (std::size_t i = 0; i < n; ++i)
        p[i] = (p[i] & 0xFF000000u) | (~p[i] & 0x00FFFFFFu);
    return out;
}

Image resizeCanvas(const Image& img, int w, int h, Rgba background)
{
    Image out(std::max(1, w), std::max(1, h), background);
    out.blit(img, {0, 0});
    return out;
}

Image toMonochrome(const Image& img)
{
    Image out = img;
    Rgba* p = out.data();
    const std::size_t n = std::size_t(out.width()) * out.height();
    for (std::size_t i = 0; i < n; ++i) {
        const int lum = (redOf(p[i]) * 299 + greenOf(p[i]) * 587 + blueOf(p[i]) * 114) / 1000;
        p[i] = lum >= 128 ? kWhite : kBlack;
    }
    return out;
}

Mask flipHorizontal(const Mask& m) { return flipH<MaskAccess>(m); }
Mask flipVertical(const Mask& m) { return flipV<MaskAccess>(m); }
Mask rotate(const Mask& m, int degrees) { return rot<MaskAccess>(m, degrees); }
Mask scaled(const Mask& m, int w, int h) { return scale<MaskAccess>(m, w, h); }

} // namespace lp
