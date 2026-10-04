#include "core/tools.h"

#include <cmath>

namespace lp {

namespace {

Rgba buttonColor(ToolHost& h, int button) { return h.color(button == 0 ? 0 : 1); }
Rgba otherColor(ToolHost& h, int button) { return h.color(button == 0 ? 1 : 0); }

ShapeStyle shapeStyle(ToolHost& h, int button)
{
    ShapeStyle s;
    s.fill = h.options().fillStyle;
    s.lineWidth = h.options().lineWidth;
    s.outline = buttonColor(h, button);
    s.interior = s.fill == FillStyle::Fill ? buttonColor(h, button) : otherColor(h, button);
    return s;
}

std::vector<Point> squareFootprint(int n)
{
    std::vector<Point> fp;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            fp.push_back({x - n / 2, y - n / 2});
    return fp;
}

// One drag, redrawn from the snapshot on every move (line and box shapes).
class DragShapeTool : public Tool {
public:
    void press(ToolHost& h, const PointerEvent& e) override
    {
        if (active_) {
            if (e.button != button_)
                cancel(h);
            return;
        }
        h.document().beginEdit();
        active_ = true;
        button_ = e.button;
        anchor_ = e.pos;
        last_ = {};
        redraw(h, e);
    }
    void move(ToolHost& h, const PointerEvent& e) override
    {
        if (active_)
            redraw(h, e);
    }
    void release(ToolHost& h, const PointerEvent& e) override
    {
        if (!active_ || e.button != button_)
            return;
        redraw(h, e);
        finish(h);
    }
    void cancel(ToolHost& h) override
    {
        if (!active_)
            return;
        h.document().cancelEdit();
        h.repaint(last_);
        active_ = false;
    }
    void finish(ToolHost& h) override
    {
        if (!active_)
            return;
        h.document().commitEdit();
        h.repaint(last_);
        active_ = false;
    }

protected:
    virtual Rect draw(ToolHost& h, Point a, Point b, const PointerEvent& e) = 0;

    int button_ = 0;

private:
    void redraw(ToolHost& h, const PointerEvent& e)
    {
        Document& doc = h.document();
        doc.restore(last_);
        const Rect r = draw(h, anchor_, e.pos, e);
        doc.addDirty(r);
        h.repaint(last_.united(r));
        last_ = r;
    }

    bool active_ = false;
    Point anchor_;
    Rect last_;
};

class LineTool : public DragShapeTool {
protected:
    Rect draw(ToolHost& h, Point a, Point b, const PointerEvent& e) override
    {
        if (e.shift)
            b = constrainTo45(a, b);
        return drawLine(h.document().image(), a, b, buttonColor(h, button_), h.options().lineWidth);
    }
};

enum class BoxShape { Rectangle, Ellipse, Rounded };

class BoxTool : public DragShapeTool {
public:
    explicit BoxTool(BoxShape shape) : shape_(shape) {}

protected:
    Rect draw(ToolHost& h, Point a, Point b, const PointerEvent& e) override
    {
        if (e.shift)
            b = constrainSquare(a, b);
        const Rect r = Rect::fromPoints(a, b);
        h.showSize(r.w, r.h);
        const ShapeStyle s = shapeStyle(h, button_);
        Image& img = h.document().image();
        switch (shape_) {
        case BoxShape::Rectangle:
            return drawRectangle(img, r, s);
        case BoxShape::Ellipse:
            return drawEllipse(img, r, s);
        case BoxShape::Rounded:
            return drawRoundedRect(img, r, s);
        }
        return {};
    }

private:
    BoxShape shape_;
};

// Pencil, brush, eraser and airbrush: segments drawn as the pointer moves.
class FreehandTool : public Tool {
public:
    void press(ToolHost& h, const PointerEvent& e) override
    {
        if (active_) {
            if (e.button != button_)
                cancel(h);
            return;
        }
        h.document().beginEdit();
        active_ = true;
        button_ = e.button;
        anchor_ = last_ = e.pos;
        shiftLine_ = e.shift && supportsShiftLine();
        apply(h, segment(h, e.pos, e.pos, button_));
    }
    void move(ToolHost& h, const PointerEvent& e) override
    {
        if (!active_)
            return;
        if (shiftLine_) {
            Document& doc = h.document();
            const Rect old = doc.dirty();
            doc.restore(old);
            const Rect r = segment(h, anchor_, constrainTo45(anchor_, e.pos), button_);
            apply(h, r.united(old));
            return;
        }
        apply(h, segment(h, last_, e.pos, button_));
        last_ = e.pos;
    }
    void release(ToolHost& h, const PointerEvent& e) override
    {
        if (!active_ || e.button != button_)
            return;
        finish(h);
    }
    void cancel(ToolHost& h) override
    {
        if (!active_)
            return;
        h.document().cancelEdit();
        active_ = false;
    }
    void finish(ToolHost& h) override
    {
        if (!active_)
            return;
        h.document().commitEdit();
        active_ = false;
    }

protected:
    virtual Rect segment(ToolHost& h, Point a, Point b, int button) = 0;
    virtual bool supportsShiftLine() const { return true; }
    void apply(ToolHost& h, Rect r)
    {
        h.document().addDirty(r);
        h.repaint(r);
    }

    bool active_ = false;
    int button_ = 0;
    Point anchor_;
    Point last_;
    bool shiftLine_ = false;
};

class PencilTool : public FreehandTool {
protected:
    Rect segment(ToolHost& h, Point a, Point b, int button) override
    {
        return drawLine(h.document().image(), a, b, buttonColor(h, button), 1);
    }
};

class BrushTool : public FreehandTool {
protected:
    Rect segment(ToolHost& h, Point a, Point b, int button) override
    {
        return drawFootprintLine(h.document().image(), a, b, brushFootprint(classicBrush(h.options().brushIndex)),
                                 buttonColor(h, button));
    }
};

class EraserTool : public FreehandTool {
protected:
    Rect segment(ToolHost& h, Point a, Point b, int button) override
    {
        Image& img = h.document().image();
        const auto fp = squareFootprint(h.options().eraserSize);
        if (button == 0)
            return drawFootprintLine(img, a, b, fp, h.color(1));
        // Color eraser: only pixels of the primary color become the secondary color.
        Rect dirty;
        forEachLinePoint(a, b, [&](Point p) { dirty = dirty.united(replaceColor(img, p, fp, h.color(0), h.color(1))); });
        return dirty;
    }
    bool supportsShiftLine() const override { return false; }
};

class AirbrushTool : public FreehandTool {
public:
    bool wantsTicks() const override { return true; }
    void tick(ToolHost& h) override
    {
        if (active_)
            apply(h, sprayOnce(h, last_, button_));
    }

protected:
    Rect segment(ToolHost& h, Point, Point b, int button) override { return sprayOnce(h, b, button); }
    bool supportsShiftLine() const override { return false; }

private:
    Rect sprayOnce(ToolHost& h, Point p, int button)
    {
        const int d = h.options().airbrushSize;
        const int count = d * d / 40 + 2;
        return spray(h.document().image(), p, d, count, buttonColor(h, button), rng_);
    }
    std::mt19937 rng_{0x5EED};
};

class FillTool : public Tool {
public:
    void press(ToolHost& h, const PointerEvent& e) override
    {
        Document& doc = h.document();
        doc.beginEdit();
        const Rect r = floodFill(doc.image(), e.pos, buttonColor(h, e.button));
        doc.addDirty(r);
        doc.commitEdit();
        h.repaint(r);
    }
};

class PickColorTool : public Tool {
public:
    void press(ToolHost& h, const PointerEvent& e) override
    {
        active_ = true;
        button_ = e.button;
        pick(h, e.pos);
    }
    void move(ToolHost& h, const PointerEvent& e) override
    {
        if (active_)
            pick(h, e.pos);
    }
    void release(ToolHost& h, const PointerEvent& e) override
    {
        if (!active_ || e.button != button_)
            return;
        active_ = false;
        h.restorePreviousTool();
    }

private:
    void pick(ToolHost& h, Point p)
    {
        const Image& img = h.document().image();
        if (img.contains(p.x, p.y))
            h.setColor(button_ == 0 ? 0 : 1, img.pixel(p.x, p.y));
    }
    bool active_ = false;
    int button_ = 0;
};

class MagnifierTool : public Tool {
public:
    void hover(ToolHost& h, Point p) override
    {
        const int target = h.options().magnifierZoom;
        if (h.zoom() != 1 || target <= 1) {
            frame_.reset();
            return;
        }
        const Point size = h.visibleImageSize(target);
        const Image& img = h.document().image();
        Rect r{p.x - size.x / 2, p.y - size.y / 2, std::min(size.x, img.width()), std::min(size.y, img.height())};
        r.x = std::clamp(r.x, 0, std::max(0, img.width() - r.w));
        r.y = std::clamp(r.y, 0, std::max(0, img.height() - r.h));
        frame_ = r;
    }
    void press(ToolHost& h, const PointerEvent& e) override
    {
        const int target = h.zoom() == 1 ? std::max(1, h.options().magnifierZoom) : 1;
        frame_.reset();
        h.zoomAt(target, e.pos);
    }
    std::optional<Rect> hoverFrame() const override { return frame_; }

private:
    std::optional<Rect> frame_;
};

class CurveTool : public Tool {
public:
    void press(ToolHost& h, const PointerEvent& e) override
    {
        switch (state_) {
        case State::Idle:
            h.document().beginEdit();
            button_ = e.button;
            p0_ = p3_ = c1_ = c2_ = e.pos;
            last_ = {};
            state_ = State::DragLine;
            break;
        case State::WaitC1:
            if (e.button != button_)
                return cancel(h);
            c1_ = c2_ = e.pos;
            state_ = State::DragC1;
            break;
        case State::WaitC2:
            if (e.button != button_)
                return cancel(h);
            c2_ = e.pos;
            state_ = State::DragC2;
            break;
        default:
            if (e.button != button_)
                return cancel(h);
            return;
        }
        redraw(h);
    }
    void move(ToolHost& h, const PointerEvent& e) override
    {
        switch (state_) {
        case State::DragLine:
            p3_ = e.shift ? constrainTo45(p0_, e.pos) : e.pos;
            c1_ = p0_;
            c2_ = p3_;
            break;
        case State::DragC1:
            c1_ = c2_ = e.pos;
            break;
        case State::DragC2:
            c2_ = e.pos;
            break;
        default:
            return;
        }
        redraw(h);
    }
    void release(ToolHost& h, const PointerEvent& e) override
    {
        if (e.button != button_)
            return;
        switch (state_) {
        case State::DragLine:
            state_ = State::WaitC1;
            break;
        case State::DragC1:
            state_ = State::WaitC2;
            break;
        case State::DragC2:
            finish(h);
            break;
        default:
            break;
        }
    }
    void cancel(ToolHost& h) override
    {
        if (state_ == State::Idle)
            return;
        h.document().cancelEdit();
        h.repaint(last_);
        state_ = State::Idle;
    }
    void finish(ToolHost& h) override
    {
        if (state_ == State::Idle)
            return;
        h.document().commitEdit();
        h.repaint(last_);
        state_ = State::Idle;
    }

private:
    enum class State { Idle, DragLine, WaitC1, DragC1, WaitC2, DragC2 };

    void redraw(ToolHost& h)
    {
        Document& doc = h.document();
        doc.restore(last_);
        const Rect r = drawBezier(doc.image(), p0_, c1_, c2_, p3_, buttonColor(h, button_), h.options().lineWidth);
        doc.addDirty(r);
        h.repaint(last_.united(r));
        last_ = r;
    }

    State state_ = State::Idle;
    int button_ = 0;
    Point p0_, c1_, c2_, p3_;
    Rect last_;
};

class PolygonTool : public Tool {
public:
    void press(ToolHost& h, const PointerEvent& e) override
    {
        if (state_ == State::Idle) {
            h.document().beginEdit();
            button_ = e.button;
            pts_ = {e.pos, e.pos};
            last_ = {};
        } else if (state_ == State::Waiting) {
            if (e.button != button_)
                return cancel(h);
            const Point p = e.shift ? constrainTo45(pts_.back(), e.pos) : e.pos;
            pts_.push_back(p);
        } else {
            if (e.button != button_)
                cancel(h);
            return;
        }
        state_ = State::Dragging;
        redraw(h);
    }
    void move(ToolHost& h, const PointerEvent& e) override
    {
        if (state_ != State::Dragging)
            return;
        const Point prev = pts_[pts_.size() - 2];
        pts_.back() = e.shift ? constrainTo45(prev, e.pos) : e.pos;
        redraw(h);
    }
    void release(ToolHost& h, const PointerEvent& e) override
    {
        if (state_ != State::Dragging || e.button != button_)
            return;
        state_ = State::Waiting;
        const Point a = pts_.front(), b = pts_.back();
        if (pts_.size() >= 4 && std::abs(a.x - b.x) <= 2 && std::abs(a.y - b.y) <= 2) {
            pts_.pop_back();
            finish(h);
        }
    }
    void doubleClick(ToolHost& h, const PointerEvent& e) override
    {
        if (state_ != State::Idle && e.button == button_)
            finish(h);
    }
    void cancel(ToolHost& h) override
    {
        if (state_ == State::Idle)
            return;
        h.document().cancelEdit();
        h.repaint(last_);
        state_ = State::Idle;
        pts_.clear();
    }
    void finish(ToolHost& h) override
    {
        if (state_ == State::Idle)
            return;
        Document& doc = h.document();
        doc.restore(last_);
        const Rect r = drawPolygon(doc.image(), pts_, shapeStyle(h, button_));
        doc.addDirty(r);
        doc.commitEdit();
        h.repaint(last_.united(r));
        state_ = State::Idle;
        pts_.clear();
    }

private:
    enum class State { Idle, Dragging, Waiting };

    void redraw(ToolHost& h)
    {
        Document& doc = h.document();
        doc.restore(last_);
        const Rect r = drawPolyline(doc.image(), pts_, buttonColor(h, button_), h.options().lineWidth);
        doc.addDirty(r);
        h.repaint(last_.united(r));
        last_ = r;
    }

    State state_ = State::Idle;
    int button_ = 0;
    std::vector<Point> pts_;
    Rect last_;
};

class SelectTool : public Tool {
public:
    explicit SelectTool(bool freeForm) : freeForm_(freeForm) {}

    void press(ToolHost& h, const PointerEvent& e) override
    {
        SelectionController& sel = h.selection();
        if (sel.hasSelection() && sel.rect().contains(e.pos)) {
            sel.setTransparency(h.options().transparentSelection, h.color(1));
            if (!sel.isFloating())
                sel.lift(e.ctrl, h.color(1));
            else if (e.ctrl)
                sel.stamp();
            dragging_ = true;
            stamping_ = e.shift;
            grab_ = e.pos - Point{sel.rect().x, sel.rect().y};
            h.repaint({});
            return;
        }
        sel.commit();
        selecting_ = true;
        anchor_ = e.pos;
        band_ = Rect::fromPoints(e.pos, e.pos);
        path_ = {e.pos};
        h.repaint({});
    }
    void move(ToolHost& h, const PointerEvent& e) override
    {
        SelectionController& sel = h.selection();
        if (dragging_) {
            if (stamping_)
                sel.stamp();
            sel.moveTo(e.pos - grab_);
            h.repaint({});
        } else if (selecting_) {
            const Image& img = h.document().image();
            const Point p{std::clamp(e.pos.x, 0, img.width() - 1), std::clamp(e.pos.y, 0, img.height() - 1)};
            if (freeForm_) {
                if (!(path_.back() == p))
                    path_.push_back(p);
            } else {
                band_ = Rect::fromPoints(anchor_, p);
                h.showSize(band_.w, band_.h);
            }
            h.repaint({});
        }
    }
    void release(ToolHost& h, const PointerEvent&) override
    {
        if (dragging_) {
            dragging_ = false;
        } else if (selecting_) {
            selecting_ = false;
            SelectionController& sel = h.selection();
            if (freeForm_) {
                if (path_.size() >= 3)
                    sel.selectPolygon(path_);
            } else if (band_.w > 1 || band_.h > 1) {
                sel.select(band_);
            }
            path_.clear();
        }
        h.repaint({});
    }
    void cancel(ToolHost& h) override
    {
        selecting_ = false;
        path_.clear();
        h.repaint({});
    }
    void finish(ToolHost& h) override
    {
        selecting_ = false;
        dragging_ = false;
        path_.clear();
        h.selection().commit();
        h.repaint({});
    }
    std::optional<Rect> rubberBand() const override
    {
        if (selecting_ && !freeForm_)
            return band_;
        return std::nullopt;
    }
    const std::vector<Point>* lassoPath() const override { return selecting_ && freeForm_ ? &path_ : nullptr; }

private:
    bool freeForm_;
    bool dragging_ = false;
    bool stamping_ = false;
    bool selecting_ = false;
    Point grab_;
    Point anchor_;
    Rect band_;
    std::vector<Point> path_;
};

} // namespace

std::unique_ptr<Tool> createTool(ToolId id)
{
    switch (id) {
    case ToolId::FreeSelect:
        return std::make_unique<SelectTool>(true);
    case ToolId::RectSelect:
        return std::make_unique<SelectTool>(false);
    case ToolId::Eraser:
        return std::make_unique<EraserTool>();
    case ToolId::Fill:
        return std::make_unique<FillTool>();
    case ToolId::PickColor:
        return std::make_unique<PickColorTool>();
    case ToolId::Magnifier:
        return std::make_unique<MagnifierTool>();
    case ToolId::Pencil:
        return std::make_unique<PencilTool>();
    case ToolId::Brush:
        return std::make_unique<BrushTool>();
    case ToolId::Airbrush:
        return std::make_unique<AirbrushTool>();
    case ToolId::Text:
        return nullptr;
    case ToolId::Line:
        return std::make_unique<LineTool>();
    case ToolId::Curve:
        return std::make_unique<CurveTool>();
    case ToolId::Rectangle:
        return std::make_unique<BoxTool>(BoxShape::Rectangle);
    case ToolId::Polygon:
        return std::make_unique<PolygonTool>();
    case ToolId::Ellipse:
        return std::make_unique<BoxTool>(BoxShape::Ellipse);
    case ToolId::RoundedRect:
        return std::make_unique<BoxTool>(BoxShape::Rounded);
    }
    return nullptr;
}

} // namespace lp
