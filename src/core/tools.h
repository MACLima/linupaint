#pragma once

#include "core/document.h"
#include "core/selection.h"
#include "raster/draw.h"

#include <memory>
#include <optional>
#include <random>
#include <vector>

namespace lp {

// Toolbox order of the classic Paint (two columns, top to bottom, left to right).
enum class ToolId {
    FreeSelect,
    RectSelect,
    Eraser,
    Fill,
    PickColor,
    Magnifier,
    Pencil,
    Brush,
    Airbrush,
    Text,
    Line,
    Curve,
    Rectangle,
    Polygon,
    Ellipse,
    RoundedRect,
};
constexpr int kToolCount = 16;

struct ToolOptions {
    int eraserSize = 8;       // 4, 6, 8 or 10
    int brushIndex = 1;       // 0..11, see classicBrush()
    int airbrushSize = 9;     // 9, 16 or 24
    int lineWidth = 1;        // 1..5
    FillStyle fillStyle = FillStyle::Outline;
    int magnifierZoom = 4;    // 1, 2, 6 or 8 (the classic offers 1x, 2x, 6x, 8x)
    bool transparentSelection = false;
};

struct PointerEvent {
    Point pos;      // image coordinates
    int button = 0; // 0 = left (primary color), 1 = right (secondary color)
    bool shift = false;
    bool ctrl = false;
};

class ToolHost {
public:
    virtual ~ToolHost() = default;
    virtual Document& document() = 0;
    virtual SelectionController& selection() = 0;
    virtual Rgba color(int index) const = 0; // 0 = primary, 1 = secondary
    virtual void setColor(int index, Rgba c) = 0;
    virtual const ToolOptions& options() const = 0;
    virtual void repaint(Rect imageRect) = 0;
    virtual int zoom() const = 0;
    virtual void zoomAt(int level, Point imagePoint) = 0;
    // Size of the image area visible in the viewport at the given zoom.
    virtual Point visibleImageSize(int zoom) const = 0;
    virtual void restorePreviousTool() = 0;
    virtual void showSize(int w, int h) = 0;
};

class Tool {
public:
    virtual ~Tool() = default;
    virtual void press(ToolHost&, const PointerEvent&) {}
    virtual void move(ToolHost&, const PointerEvent&) {}
    virtual void release(ToolHost&, const PointerEvent&) {}
    virtual void doubleClick(ToolHost&, const PointerEvent&) {}
    virtual void hover(ToolHost&, Point) {}
    virtual void tick(ToolHost&) {}
    virtual bool wantsTicks() const { return false; }
    // Esc or the opposite button: abandon the operation in progress.
    virtual void cancel(ToolHost&) {}
    // Tool switch / document action: complete any multi-step operation.
    virtual void finish(ToolHost&) {}

    // Overlays the canvas draws on top of the image.
    virtual std::optional<Rect> rubberBand() const { return std::nullopt; }
    virtual const std::vector<Point>* lassoPath() const { return nullptr; }
    virtual std::optional<Rect> hoverFrame() const { return std::nullopt; }
};

// Returns nullptr for ToolId::Text, which lives in the UI layer (it needs font rendering).
std::unique_ptr<Tool> createTool(ToolId id);

} // namespace lp
