#pragma once

#include "core/document.h"

#include <functional>
#include <vector>

namespace lp {

// Rectangular or free-form selection. While "floating", its pixels hover over a copy of the
// image (the base) inside a document edit session, exactly like the classic Paint selection.
class SelectionController {
public:
    explicit SelectionController(Document& doc) : doc_(doc) {}

    bool hasSelection() const { return active_; }
    bool isFloating() const { return floating_; }
    bool isFreeForm() const { return !mask_.isNull(); }
    Rect rect() const { return rect_; }
    // Free-form outline in image coordinates (follows moves; scaled outlines are approximated).
    std::vector<Point> outline() const;

    void select(Rect r);
    void selectPolygon(const std::vector<Point>& pts);
    void selectAll() { select(doc_.image().bounds()); }

    // Picks the pixels up. `keepOriginal` (Ctrl+drag) leaves the source untouched,
    // otherwise the hole is filled with `background`.
    void lift(bool keepOriginal, Rgba background);
    void moveTo(Point topLeft);
    // Shift+drag: leaves a copy of the selection at its current position.
    void stamp();
    void resizeTo(Rect r);

    void setTransparency(bool transparent, Rgba key);
    bool isTransparent() const { return transparent_; }

    // Applies an image operation (flip, rotate, stretch, invert...) to the selection pixels.
    void transform(const std::function<Image(const Image&)>& op, const std::function<Mask(const Mask&)>& maskOp,
                   Rgba background);

    // Merges a floating selection into the image (one undo step) and clears the selection.
    void commit();
    // Forgets the selection without touching pixels (only valid when not floating).
    void clear();
    // Delete / Clear Selection: fills the selected area with `background`.
    void deleteContents(Rgba background);

    // Selected pixels for the clipboard; pixels outside a free-form outline become `background`.
    Image extract(Rgba background) const;
    // Paste: a new floating selection at `at`.
    void paste(Image img, Point at);

private:
    void render();

    Document& doc_;
    bool active_ = false;
    bool floating_ = false;
    bool transparent_ = false;
    Rgba key_ = kWhite;
    Rect rect_;
    Rect drawn_;
    Image pixels_;   // current (possibly resized) pixels
    Image original_; // pixels as lifted, used as the source for resizing
    Mask mask_;
    Mask originalMask_;
    Image base_;
    std::vector<Point> outline_; // relative to the original rect origin
    Rect outlineRect_;
};

} // namespace lp
