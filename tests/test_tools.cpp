#include "core/tools.h"

#include <catch2/catch_test_macros.hpp>

using namespace lp;

namespace {

constexpr Rgba kRed = rgb(255, 0, 0);

class FakeHost : public ToolHost {
public:
    FakeHost() : doc(20, 20), sel(doc) {}
    Document& document() override { return doc; }
    SelectionController& selection() override { return sel; }
    Rgba color(int i) const override { return colors[i]; }
    void setColor(int i, Rgba c) override { colors[i] = c; }
    const ToolOptions& options() const override { return opts; }
    void repaint(Rect) override {}
    int zoom() const override { return zoomLevel; }
    void zoomAt(int level, Point) override { zoomLevel = level; }
    Point visibleImageSize(int z) const override { return {100 / z, 100 / z}; }
    void restorePreviousTool() override { restored = true; }
    void showSize(int, int) override {}

    Document doc;
    SelectionController sel;
    Rgba colors[2] = {kBlack, kWhite};
    ToolOptions opts;
    int zoomLevel = 1;
    bool restored = false;
};

PointerEvent at(int x, int y, int button = 0, bool shift = false)
{
    PointerEvent e;
    e.pos = {x, y};
    e.button = button;
    e.shift = shift;
    return e;
}

void drag(Tool& t, ToolHost& h, Point a, Point b, int button = 0, bool shift = false)
{
    t.press(h, at(a.x, a.y, button, shift));
    t.move(h, at(b.x, b.y, button, shift));
    t.release(h, at(b.x, b.y, button, shift));
}

} // namespace

TEST_CASE("every tool except text exists")
{
    for (int i = 0; i < kToolCount; ++i) {
        const auto id = ToolId(i);
        REQUIRE((createTool(id) != nullptr) == (id != ToolId::Text));
    }
}

TEST_CASE("pencil draws with the button color and undoes as one step")
{
    FakeHost h;
    auto t = createTool(ToolId::Pencil);
    drag(*t, h, {1, 1}, {10, 1});
    REQUIRE(h.doc.image().pixel(5, 1) == kBlack);
    drag(*t, h, {1, 3}, {10, 3}, 1);
    REQUIRE(h.doc.image().pixel(5, 3) == kWhite);
    h.doc.undo();
    h.doc.undo();
    REQUIRE(h.doc.image().pixel(5, 1) == kWhite);
}

TEST_CASE("line tool redraws from the snapshot while dragging")
{
    FakeHost h;
    auto t = createTool(ToolId::Line);
    t->press(h, at(0, 0));
    t->move(h, at(19, 0));
    t->move(h, at(0, 19));
    t->release(h, at(0, 19));
    REQUIRE(h.doc.image().pixel(10, 0) == kWhite);
    REQUIRE(h.doc.image().pixel(0, 10) == kBlack);
}

TEST_CASE("pressing the other button cancels a shape")
{
    FakeHost h;
    auto t = createTool(ToolId::Rectangle);
    t->press(h, at(2, 2));
    t->move(h, at(10, 10));
    t->press(h, at(10, 10, 1));
    REQUIRE(h.doc.image() == Image(20, 20));
}

TEST_CASE("filled rectangle uses the other color inside")
{
    FakeHost h;
    h.opts.fillStyle = FillStyle::OutlineAndFill;
    h.colors[1] = kRed;
    auto t = createTool(ToolId::Rectangle);
    drag(*t, h, {2, 2}, {10, 10});
    REQUIRE(h.doc.image().pixel(2, 2) == kBlack);
    REQUIRE(h.doc.image().pixel(6, 6) == kRed);
}

TEST_CASE("fill, pick color and magnifier")
{
    FakeHost h;
    h.colors[0] = kRed;
    createTool(ToolId::Fill)->press(h, at(5, 5));
    REQUIRE(h.doc.image().pixel(0, 0) == kRed);

    h.colors[1] = kBlack;
    auto pick = createTool(ToolId::PickColor);
    pick->press(h, at(3, 3, 1));
    pick->release(h, at(3, 3, 1));
    REQUIRE(h.colors[1] == kRed);
    REQUIRE(h.restored);

    auto mag = createTool(ToolId::Magnifier);
    mag->press(h, at(5, 5));
    REQUIRE(h.zoomLevel == 4);
    mag->press(h, at(5, 5));
    REQUIRE(h.zoomLevel == 1);
}

TEST_CASE("eraser and color eraser")
{
    FakeHost h;
    h.doc.image().fill(kBlack);
    h.colors[1] = kRed;
    auto t = createTool(ToolId::Eraser);
    drag(*t, h, {5, 5}, {5, 5});
    REQUIRE(h.doc.image().pixel(5, 5) == kRed);

    FakeHost h2;
    h2.doc.image().fillRect({0, 0, 20, 10}, kBlack);
    h2.colors[1] = kRed;
    drag(*createTool(ToolId::Eraser), h2, {5, 9}, {5, 10}, 1);
    REQUIRE(h2.doc.image().pixel(5, 9) == kRed);
    REQUIRE(h2.doc.image().pixel(5, 11) == kWhite);
}

TEST_CASE("curve takes a line and two control points")
{
    FakeHost h;
    auto t = createTool(ToolId::Curve);
    drag(*t, h, {0, 10}, {19, 10});
    REQUIRE(h.doc.image().pixel(10, 10) == kBlack);
    drag(*t, h, {10, 0}, {10, 0});
    drag(*t, h, {10, 0}, {10, 0});
    REQUIRE(h.doc.image().pixel(10, 10) == kWhite);
    REQUIRE(h.doc.history().undoCount() == 1);
}

TEST_CASE("polygon closes on double click")
{
    FakeHost h;
    h.opts.fillStyle = FillStyle::Fill;
    auto t = createTool(ToolId::Polygon);
    drag(*t, h, {0, 0}, {19, 0});
    t->press(h, at(19, 19));
    t->release(h, at(19, 19));
    t->doubleClick(h, at(19, 19));
    REQUIRE(h.doc.image().pixel(15, 5) == kBlack);
    REQUIRE(h.doc.image().pixel(2, 15) == kWhite);
    REQUIRE(h.doc.history().undoCount() == 1);
}

TEST_CASE("rectangle selection drag moves pixels")
{
    FakeHost h;
    h.doc.image().fillRect({2, 2, 3, 3}, kRed);
    auto t = createTool(ToolId::RectSelect);
    drag(*t, h, {2, 2}, {4, 4});
    REQUIRE(h.sel.hasSelection());
    REQUIRE(h.sel.rect() == Rect{2, 2, 3, 3});
    drag(*t, h, {3, 3}, {13, 13});
    t->finish(h);
    REQUIRE(h.doc.image().pixel(13, 13) == kRed);
    REQUIRE(h.doc.image().pixel(3, 3) == kWhite);
}

TEST_CASE("airbrush keeps spraying on ticks")
{
    FakeHost h;
    auto t = createTool(ToolId::Airbrush);
    REQUIRE(t->wantsTicks());
    t->press(h, at(10, 10));
    for (int i = 0; i < 50; ++i)
        t->tick(h);
    t->release(h, at(10, 10));
    int n = 0;
    for (int y = 0; y < 20; ++y)
        for (int x = 0; x < 20; ++x)
            n += h.doc.image().pixel(x, y) == kBlack;
    REQUIRE(n > 30);
}
