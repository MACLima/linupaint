#include "raster/draw.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

using namespace lp;

namespace {

int countColor(const Image& img, Rgba c)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            n += img.pixel(x, y) == c;
    return n;
}

bool symmetricH(const Image& img)
{
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (img.pixel(x, y) != img.pixel(img.width() - 1 - x, y))
                return false;
    return true;
}

bool symmetricV(const Image& img)
{
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (img.pixel(x, y) != img.pixel(x, img.height() - 1 - y))
                return false;
    return true;
}

} // namespace

TEST_CASE("one pixel lines have no antialiasing and exact length")
{
    Image img(10, 10);
    const Rect r = drawLine(img, {1, 1}, {8, 1}, kBlack);
    REQUIRE(r == Rect{1, 1, 8, 1});
    REQUIRE(countColor(img, kBlack) == 8);

    Image diag(5, 5);
    drawLine(diag, {0, 0}, {4, 4}, kBlack);
    for (int i = 0; i < 5; ++i)
        REQUIRE(diag.pixel(i, i) == kBlack);
    REQUIRE(countColor(diag, kBlack) == 5);
    // Only the two colors exist: no blended pixels.
    REQUIRE(countColor(diag, kBlack) + countColor(diag, kWhite) == 25);
}

TEST_CASE("lines clip at the image border")
{
    Image img(4, 4);
    const Rect r = drawLine(img, {-10, 2}, {10, 2}, kBlack, 3);
    REQUIRE(r == Rect{0, 1, 4, 3});
    REQUIRE(countColor(img, kBlack) == 12);
}

TEST_CASE("pen footprints")
{
    REQUIRE(penFootprint(1).size() == 1);
    REQUIRE(penFootprint(2).size() == 4);
    REQUIRE(penFootprint(3).size() == 9);
    REQUIRE(penFootprint(4).size() == 12);
    REQUIRE(penFootprint(5).size() == 21);
}

TEST_CASE("classic brushes")
{
    REQUIRE(classicBrush(0).shape == BrushShape::Round);
    REQUIRE(classicBrush(2).size == 1);
    REQUIRE(classicBrush(3).shape == BrushShape::Square);
    REQUIRE(brushFootprint(classicBrush(3)).size() == 64);
    REQUIRE(brushFootprint(classicBrush(6)).size() == 8);
    REQUIRE(classicBrush(11).shape == BrushShape::Backslash);
}

TEST_CASE("shift constraints")
{
    REQUIRE(constrainTo45({0, 0}, {10, 2}) == Point{10, 0});
    REQUIRE(constrainTo45({0, 0}, {1, 9}) == Point{0, 9});
    REQUIRE(constrainTo45({0, 0}, {7, -6}) == Point{7, -7});
    REQUIRE(constrainSquare({5, 5}, {1, 8}) == Point{1, 9});
}

TEST_CASE("rectangle outline, filled and both")
{
    Image img(6, 5);
    ShapeStyle s;
    drawRectangle(img, {0, 0, 6, 5}, s);
    REQUIRE(countColor(img, kBlack) == 2 * 6 + 2 * 3);

    Image f(6, 5, rgb(1, 1, 1));
    s.fill = FillStyle::OutlineAndFill;
    s.interior = rgb(255, 0, 0);
    drawRectangle(f, {0, 0, 6, 5}, s);
    REQUIRE(countColor(f, rgb(255, 0, 0)) == 4 * 3);

    Image g(6, 5);
    s.fill = FillStyle::Fill;
    drawRectangle(g, {1, 1, 3, 3}, s);
    REQUIRE(countColor(g, rgb(255, 0, 0)) == 9);
}

TEST_CASE("thick rectangle outline")
{
    Image img(10, 10);
    ShapeStyle s;
    s.lineWidth = 3;
    drawRectangle(img, {0, 0, 10, 10}, s);
    REQUIRE(countColor(img, kWhite) == 16);
}

TEST_CASE("ellipses are symmetric and closed")
{
    for (int w : {5, 8, 13, 20}) {
        for (int h : {5, 9, 14}) {
            Image img(w, h);
            ShapeStyle s;
            drawEllipse(img, {0, 0, w, h}, s);
            REQUIRE(symmetricH(img));
            REQUIRE(symmetricV(img));
            REQUIRE(img.pixel(w / 2, 0) == kBlack);
            REQUIRE(img.pixel(0, h / 2) == kBlack);
            REQUIRE(img.pixel(w / 2, h / 2) == kWhite);
            // Closed outline: filling the center must not leak to the corners.
            floodFill(img, {w / 2, h / 2}, rgb(255, 0, 0));
            REQUIRE(img.pixel(0, 0) == kWhite);
        }
    }
}

TEST_CASE("rounded rectangle keeps corners clear")
{
    Image img(40, 30);
    ShapeStyle s;
    drawRoundedRect(img, {0, 0, 40, 30}, s);
    REQUIRE(img.pixel(0, 0) == kWhite);
    REQUIRE(img.pixel(20, 0) == kBlack);
    REQUIRE(img.pixel(0, 15) == kBlack);
    REQUIRE(symmetricH(img));
}

TEST_CASE("polygon fill and outline")
{
    Image img(10, 10);
    ShapeStyle s;
    s.fill = FillStyle::OutlineAndFill;
    s.interior = rgb(0, 0, 255);
    drawPolygon(img, {{0, 0}, {9, 0}, {9, 9}, {0, 9}}, s);
    REQUIRE(img.pixel(0, 0) == kBlack);
    REQUIRE(img.pixel(5, 5) == rgb(0, 0, 255));
}

TEST_CASE("bezier passes through its end points")
{
    Image img(30, 30);
    drawBezier(img, {0, 0}, {0, 29}, {29, 29}, {29, 0}, kBlack, 1);
    REQUIRE(img.pixel(0, 0) == kBlack);
    REQUIRE(img.pixel(29, 0) == kBlack);
    REQUIRE(img.pixel(15, 15) == kWhite);
}

TEST_CASE("flood fill is 4-connected and exact")
{
    Image img(5, 5);
    // Diagonal wall: 4-connectivity must not leak through it.
    for (int i = 0; i < 5; ++i)
        img.setPixel(i, 4 - i, kBlack);
    const Rect r = floodFill(img, {0, 0}, rgb(255, 0, 0));
    REQUIRE(img.pixel(0, 0) == rgb(255, 0, 0));
    REQUIRE(img.pixel(4, 4) == kWhite);
    REQUIRE(r == Rect{0, 0, 4, 4});
    REQUIRE(floodFill(img, {0, 0}, rgb(255, 0, 0)).empty());
}

TEST_CASE("color eraser replaces only the primary color")
{
    Image img(3, 1);
    img.setPixel(0, 0, kBlack);
    img.setPixel(1, 0, rgb(9, 9, 9));
    replaceColor(img, {1, 0}, penFootprint(3), kBlack, rgb(255, 0, 0));
    REQUIRE(img.pixel(0, 0) == rgb(255, 0, 0));
    REQUIRE(img.pixel(1, 0) == rgb(9, 9, 9));
}

TEST_CASE("airbrush stays inside its disc")
{
    Image img(40, 40);
    std::mt19937 rng(1);
    spray(img, {20, 20}, 16, 500, kBlack, rng);
    for (int y = 0; y < 40; ++y)
        for (int x = 0; x < 40; ++x)
            if (img.pixel(x, y) == kBlack)
                REQUIRE((x - 20) * (x - 20) + (y - 20) * (y - 20) <= 64);
    REQUIRE(countColor(img, kBlack) > 50);
}

TEST_CASE("flood fill performance on 4000x4000", "[perf]")
{
    Image img(4000, 4000);
    const auto t0 = std::chrono::steady_clock::now();
    floodFill(img, {0, 0}, kBlack);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    WARN("flood fill 4000x4000: " << ms << " ms (PRD target < 200 ms)");
    REQUIRE(img.pixel(3999, 3999) == kBlack);
}
