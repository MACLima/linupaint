#include "raster/codecs.h"

#include <QImage>

#include <catch2/catch_test_macros.hpp>

using namespace lp;

namespace {

Image gradient(int w, int h)
{
    Image img(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.setPixel(x, y, rgb((x * 37) & 0xFF, (y * 11) & 0xFF, (x * y) & 0xFF));
    return img;
}

Image fewColors(int w, int h)
{
    static const Rgba colors[] = {kBlack, kWhite, rgb(255, 0, 0), rgb(0, 128, 0), rgb(0, 0, 255), rgb(255, 255, 0)};
    Image img(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.setPixel(x, y, colors[(x / 3 + y / 2) % 6]);
    return img;
}

QImage decode(const std::vector<std::uint8_t>& bytes, const char* format)
{
    QImage q;
    REQUIRE(q.loadFromData(bytes.data(), int(bytes.size()), format));
    return q.convertToFormat(QImage::Format_ARGB32);
}

bool same(const Image& a, const QImage& q)
{
    if (q.width() != a.width() || q.height() != a.height())
        return false;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x)
            if (Rgba(q.pixel(x, y)) != a.pixel(x, y))
                return false;
    return true;
}

} // namespace

TEST_CASE("24-bit bmp round-trips through Qt")
{
    const Image img = gradient(37, 19);
    REQUIRE(same(img, decode(encodeBmp(img, 24), "BMP")));
}

TEST_CASE("indexed bmps round-trip when the colors fit")
{
    const Image img = fewColors(33, 17);
    REQUIRE(same(img, decode(encodeBmp(img, 8), "BMP")));
    REQUIRE(same(img, decode(encodeBmp(img, 4), "BMP")));
}

TEST_CASE("monochrome bmp decodes to black and white")
{
    const QImage q = decode(encodeBmp(gradient(20, 20), 1), "BMP");
    for (int y = 0; y < 20; ++y)
        for (int x = 0; x < 20; ++x) {
            const QRgb c = q.pixel(x, y);
            REQUIRE((c == 0xFF000000u || c == 0xFFFFFFFFu));
        }
}

TEST_CASE("gif round-trips exactly when the colors fit")
{
    const Image small = fewColors(51, 23);
    REQUIRE(same(small, decode(encodeGif(small), "GIF")));
    // Large enough to force several LZW dictionary resets.
    const Image big = fewColors(640, 480);
    REQUIRE(same(big, decode(encodeGif(big), "GIF")));
}

TEST_CASE("gif with more than 256 colors decodes at full size")
{
    const Image img = gradient(300, 200);
    const QImage q = decode(encodeGif(img), "GIF");
    REQUIRE(q.width() == 300);
    REQUIRE(q.height() == 200);
}
