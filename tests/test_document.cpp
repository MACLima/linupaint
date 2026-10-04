#include "core/document.h"
#include "raster/draw.h"

#include <catch2/catch_test_macros.hpp>

using namespace lp;

TEST_CASE("edit session commits one undo step")
{
    Document doc(10, 10);
    doc.beginEdit();
    doc.addDirty(drawLine(doc.image(), {0, 0}, {9, 0}, kBlack));
    doc.commitEdit();
    REQUIRE(doc.isModified());
    REQUIRE(doc.canUndo());
    REQUIRE(doc.image().pixel(5, 0) == kBlack);

    doc.undo();
    REQUIRE(doc.image().pixel(5, 0) == kWhite);
    REQUIRE(doc.canRedo());
    doc.redo();
    REQUIRE(doc.image().pixel(5, 0) == kBlack);
}

TEST_CASE("cancel restores the snapshot")
{
    Document doc(10, 10);
    doc.beginEdit();
    doc.addDirty(drawLine(doc.image(), {0, 5}, {9, 5}, kBlack));
    doc.cancelEdit();
    REQUIRE(doc.image() == Image(10, 10));
    REQUIRE_FALSE(doc.canUndo());
}

TEST_CASE("no-op edits do not create history")
{
    Document doc(4, 4);
    doc.beginEdit();
    doc.addDirty({0, 0, 4, 4});
    doc.commitEdit();
    REQUIRE_FALSE(doc.canUndo());
}

TEST_CASE("whole image replacement is undoable")
{
    Document doc(4, 4);
    doc.replaceImage(Image(8, 2, kBlack));
    REQUIRE(doc.image().width() == 8);
    doc.undo();
    REQUIRE(doc.image().width() == 4);
    REQUIRE(doc.image().pixel(0, 0) == kWhite);
    doc.redo();
    REQUIRE(doc.image().height() == 2);
}

TEST_CASE("at least 50 undo levels")
{
    Document doc(64, 64);
    for (int i = 0; i < 60; ++i) {
        doc.beginEdit();
        doc.image().setPixel(i, 0, kBlack);
        doc.addDirty({i, 0, 1, 1});
        doc.commitEdit();
    }
    REQUIRE(doc.history().undoCount() >= 50);
    for (int i = 0; i < 50; ++i)
        doc.undo();
    REQUIRE(doc.image().pixel(10, 0) == kWhite);
    REQUIRE(doc.image().pixel(9, 0) == kBlack);
}

TEST_CASE("reset clears history and the modified flag")
{
    Document doc(4, 4);
    doc.replaceImage(Image(2, 2));
    doc.reset(Image(3, 3));
    REQUIRE_FALSE(doc.canUndo());
    REQUIRE_FALSE(doc.isModified());
}
