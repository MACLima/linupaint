#include "raster/image.h"

#include <catch2/catch_test_macros.hpp>

using namespace lp;

TEST_CASE("color helpers pack ARGB")
{
    constexpr Rgba c = rgb(0x12, 0x34, 0x56);
    STATIC_REQUIRE(c == 0xFF123456u);
    STATIC_REQUIRE(redOf(c) == 0x12);
    STATIC_REQUIRE(greenOf(c) == 0x34);
    STATIC_REQUIRE(blueOf(c) == 0x56);
    STATIC_REQUIRE(alphaOf(c) == 0xFF);
}

TEST_CASE("rect geometry")
{
    const Rect a = Rect::fromPoints({5, 7}, {2, 3});
    REQUIRE(a == Rect{2, 3, 4, 5});
    REQUIRE(a.contains({2, 3}));
    REQUIRE(a.contains({5, 7}));
    REQUIRE_FALSE(a.contains({6, 7}));
    REQUIRE(a.united(Rect{}) == a);
    REQUIRE(a.intersected(Rect{4, 4, 10, 10}) == Rect{4, 4, 2, 4});
    REQUIRE(a.intersected(Rect{20, 20, 1, 1}).empty());
}

TEST_CASE("copy pads outside pixels and blit clips")
{
    Image img(4, 4, kBlack);
    const Image part = img.copy({-1, -1, 3, 3}, kWhite);
    REQUIRE(part.pixel(0, 0) == kWhite);
    REQUIRE(part.pixel(1, 1) == kBlack);

    Image dst(3, 3, kWhite);
    dst.blit(Image(2, 2, kBlack), {2, 2});
    REQUIRE(dst.pixel(2, 2) == kBlack);
    REQUIRE(dst.pixel(1, 1) == kWhite);
}

TEST_CASE("keyed and masked blit")
{
    Image src(2, 1, kWhite);
    src.setPixel(1, 0, kBlack);
    Image dst(2, 1, rgb(255, 0, 0));
    dst.blit(src, {0, 0}, nullptr, true, kWhite);
    REQUIRE(dst.pixel(0, 0) == rgb(255, 0, 0));
    REQUIRE(dst.pixel(1, 0) == kBlack);

    Mask m(2, 1);
    m.set(0, 0, true);
    Image dst2(2, 1, rgb(0, 255, 0));
    dst2.blit(src, {0, 0}, &m, false, 0);
    REQUIRE(dst2.pixel(0, 0) == kWhite);
    REQUIRE(dst2.pixel(1, 0) == rgb(0, 255, 0));
}

TEST_CASE("polygon mask covers interior and outline")
{
    const Mask m = Mask::fromPolygon(5, 5, {{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            REQUIRE(m.test(x, y));
    const Mask tri = Mask::fromPolygon(5, 5, {{0, 0}, {4, 0}, {0, 4}});
    REQUIRE(tri.test(0, 0));
    REQUIRE(tri.test(1, 1));
    REQUIRE_FALSE(tri.test(4, 4));
}
