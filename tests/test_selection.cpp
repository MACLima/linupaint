#include "core/selection.h"
#include "raster/transform.h"

#include <catch2/catch_test_macros.hpp>

using namespace lp;

namespace {

constexpr Rgba kRed = rgb(255, 0, 0);

Document redSquareDoc()
{
    Document doc(10, 10);
    doc.image().fillRect({2, 2, 3, 3}, kRed);
    return doc;
}

} // namespace

TEST_CASE("moving a selection leaves the background color behind")
{
    Document doc = redSquareDoc();
    SelectionController sel(doc);
    sel.select({2, 2, 3, 3});
    sel.lift(false, kWhite);
    sel.moveTo({6, 6});
    sel.commit();
    REQUIRE(doc.image().pixel(3, 3) == kWhite);
    REQUIRE(doc.image().pixel(7, 7) == kRed);
    REQUIRE_FALSE(sel.hasSelection());
    doc.undo();
    REQUIRE(doc.image().pixel(3, 3) == kRed);
    REQUIRE(doc.image().pixel(7, 7) == kWhite);
}

TEST_CASE("ctrl-drag copies and shift-drag stamps")
{
    Document doc = redSquareDoc();
    SelectionController sel(doc);
    sel.select({2, 2, 3, 3});
    sel.lift(true, kWhite);
    sel.moveTo({6, 2});
    sel.commit();
    REQUIRE(doc.image().pixel(3, 3) == kRed);
    REQUIRE(doc.image().pixel(7, 3) == kRed);

    Document doc2 = redSquareDoc();
    SelectionController s2(doc2);
    s2.select({2, 2, 3, 3});
    s2.lift(false, kWhite);
    s2.moveTo({2, 6});
    s2.stamp();
    s2.moveTo({6, 6});
    s2.commit();
    REQUIRE(doc2.image().pixel(3, 7) == kRed);
    REQUIRE(doc2.image().pixel(7, 7) == kRed);
}

TEST_CASE("transparent selections skip the background color")
{
    Document doc(10, 10);
    doc.image().fill(kBlack);
    doc.image().fillRect({0, 0, 3, 3}, kWhite);
    doc.image().setPixel(1, 1, kRed);
    SelectionController sel(doc);
    sel.select({0, 0, 3, 3});
    sel.setTransparency(true, kWhite);
    sel.lift(true, kWhite);
    sel.moveTo({5, 5});
    sel.commit();
    REQUIRE(doc.image().pixel(5, 5) == kBlack);
    REQUIRE(doc.image().pixel(6, 6) == kRed);
}

TEST_CASE("free-form selection only moves the pixels inside the outline")
{
    Document doc(10, 10);
    doc.image().fill(kRed);
    SelectionController sel(doc);
    sel.selectPolygon({{0, 0}, {4, 0}, {0, 4}});
    REQUIRE(sel.isFreeForm());
    sel.lift(false, kWhite);
    REQUIRE(doc.image().pixel(0, 0) == kRed); // still drawn at the origin while floating
    sel.moveTo({5, 5});
    sel.commit();
    REQUIRE(doc.image().pixel(0, 0) == kWhite);
    REQUIRE(doc.image().pixel(4, 4) == kRed);
    REQUIRE(doc.image().pixel(5, 5) == kRed);
}

TEST_CASE("delete, extract and paste")
{
    Document doc = redSquareDoc();
    SelectionController sel(doc);
    sel.select({2, 2, 3, 3});
    const Image clip = sel.extract(kWhite);
    REQUIRE(clip.width() == 3);
    REQUIRE(clip.pixel(0, 0) == kRed);
    sel.deleteContents(kWhite);
    REQUIRE(doc.image().pixel(3, 3) == kWhite);

    sel.paste(clip, {0, 0});
    REQUIRE(sel.isFloating());
    sel.commit();
    REQUIRE(doc.image().pixel(0, 0) == kRed);
}

TEST_CASE("resizing and transforming a selection")
{
    Document doc = redSquareDoc();
    SelectionController sel(doc);
    sel.select({2, 2, 3, 3});
    sel.lift(false, kWhite);
    sel.resizeTo({0, 0, 6, 6});
    sel.commit();
    REQUIRE(doc.image().pixel(5, 5) == kRed);

    Document doc2(10, 10);
    doc2.image().fillRect({0, 0, 4, 2}, kRed);
    SelectionController s2(doc2);
    s2.select({0, 0, 4, 2});
    s2.transform([](const Image& i) { return rotate(i, 90); }, nullptr, kWhite);
    REQUIRE(s2.rect().w == 2);
    REQUIRE(s2.rect().h == 4);
    s2.commit();
    REQUIRE(doc2.image().pixel(1, 3) == kRed);
    REQUIRE(doc2.image().pixel(3, 0) == kWhite);
}
