#include "raster/transform.h"

#include <catch2/catch_test_macros.hpp>

using namespace lp;

namespace {

Image numbered(int w, int h)
{
    Image img(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.setPixel(x, y, rgb(x, y, 0));
    return img;
}

} // namespace

TEST_CASE("flips")
{
    const Image img = numbered(3, 2);
    REQUIRE(flipHorizontal(img).pixel(0, 0) == img.pixel(2, 0));
    REQUIRE(flipVertical(img).pixel(0, 0) == img.pixel(0, 1));
    REQUIRE(flipHorizontal(flipHorizontal(img)) == img);
}

TEST_CASE("rotations")
{
    const Image img = numbered(3, 2);
    const Image r90 = rotate(img, 90);
    REQUIRE(r90.width() == 2);
    REQUIRE(r90.height() == 3);
    // Clockwise: the bottom-left pixel becomes the top-left.
    REQUIRE(r90.pixel(0, 0) == img.pixel(0, 1));
    REQUIRE(rotate(rotate(img, 90), 270) == img);
    REQUIRE(rotate(img, 180) == flipVertical(flipHorizontal(img)));
}

TEST_CASE("stretch and skew sizes")
{
    const Image img = numbered(10, 4);
    const Image s = stretch(img, 50, 200);
    REQUIRE(s.width() == 5);
    REQUIRE(s.height() == 8);
    const Image k = skew(img, 45, 0, kWhite);
    REQUIRE(k.width() == 13);
    REQUIRE(k.height() == 4);
    REQUIRE(k.pixel(0, 0) == img.pixel(0, 0));
    REQUIRE(k.pixel(3, 3) == img.pixel(0, 3));
    REQUIRE(skew(img, -45, 0, kWhite).pixel(3, 0) == img.pixel(0, 0));
}

TEST_CASE("invert, monochrome and canvas resize")
{
    Image img(2, 1);
    img.setPixel(0, 0, rgb(10, 20, 30));
    REQUIRE(invertColors(img).pixel(0, 0) == rgb(245, 235, 225));
    REQUIRE(toMonochrome(img).pixel(0, 0) == kBlack);
    REQUIRE(toMonochrome(img).pixel(1, 0) == kWhite);
    const Image big = resizeCanvas(img, 4, 3, rgb(1, 2, 3));
    REQUIRE(big.pixel(0, 0) == rgb(10, 20, 30));
    REQUIRE(big.pixel(3, 2) == rgb(1, 2, 3));
}

TEST_CASE("mask transforms follow image transforms")
{
    Mask m(3, 2);
    m.set(0, 0, true);
    REQUIRE(flipHorizontal(m).test(2, 0));
    REQUIRE(rotate(m, 90).test(1, 0));
    REQUIRE(scaled(m, 6, 4).test(1, 1));
}
