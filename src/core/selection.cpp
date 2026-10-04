#include "core/selection.h"

#include "raster/transform.h"

#include <utility>

namespace lp {

std::vector<Point> SelectionController::outline() const
{
    std::vector<Point> out;
    if (outline_.empty() || outlineRect_.empty())
        return out;
    out.reserve(outline_.size());
    for (const Point& p : outline_) {
        const int x = rect_.x + int((long long)p.x * rect_.w / outlineRect_.w);
        const int y = rect_.y + int((long long)p.y * rect_.h / outlineRect_.h);
        out.push_back({x, y});
    }
    return out;
}

void SelectionController::select(Rect r)
{
    commit();
    r = r.intersected(doc_.image().bounds());
    active_ = !r.empty();
    rect_ = r;
    mask_ = Mask();
    originalMask_ = Mask();
    outline_.clear();
    outlineRect_ = {};
}

void SelectionController::selectPolygon(const std::vector<Point>& pts)
{
    commit();
    if (pts.size() < 3) {
        active_ = false;
        return;
    }
    int x0 = pts[0].x, y0 = pts[0].y, x1 = x0, y1 = y0;
    for (const Point& p : pts) {
        x0 = std::min(x0, p.x);
        y0 = std::min(y0, p.y);
        x1 = std::max(x1, p.x);
        y1 = std::max(y1, p.y);
    }
    const Rect full{x0, y0, x1 - x0 + 1, y1 - y0 + 1};
    const Rect r = full.intersected(doc_.image().bounds());
    if (r.empty()) {
        active_ = false;
        return;
    }
    std::vector<Point> local;
    local.reserve(pts.size());
    for (const Point& p : pts)
        local.push_back({p.x - r.x, p.y - r.y});
    active_ = true;
    rect_ = r;
    mask_ = Mask::fromPolygon(r.w, r.h, local);
    originalMask_ = mask_;
    outline_ = local;
    outlineRect_ = r;
}

void SelectionController::lift(bool keepOriginal, Rgba background)
{
    if (!active_ || floating_)
        return;
    doc_.beginEdit();
    pixels_ = doc_.image().copy(rect_);
    original_ = pixels_;
    originalMask_ = mask_;
    if (!keepOriginal) {
        if (mask_.isNull())
            doc_.image().fillRect(rect_, background);
        else
            doc_.image().fillMasked(rect_, mask_, background);
        doc_.addDirty(rect_);
    }
    base_ = doc_.image();
    floating_ = true;
    drawn_ = {};
    render();
}

void SelectionController::render()
{
    if (!floating_)
        return;
    Image& img = doc_.image();
    const Rect old = drawn_.intersected(img.bounds());
    for (int y = old.y; y < old.bottom(); ++y)
        std::copy_n(base_.scanLine(y) + old.x, old.w, img.scanLine(y) + old.x);
    img.blit(pixels_, {rect_.x, rect_.y}, mask_.isNull() ? nullptr : &mask_, transparent_, key_);
    drawn_ = rect_;
    doc_.addDirty(old);
    doc_.addDirty(rect_);
}

void SelectionController::moveTo(Point topLeft)
{
    if (!floating_)
        return;
    rect_.x = topLeft.x;
    rect_.y = topLeft.y;
    render();
}

void SelectionController::stamp()
{
    if (!floating_)
        return;
    base_.blit(pixels_, {rect_.x, rect_.y}, mask_.isNull() ? nullptr : &mask_, transparent_, key_);
    doc_.addDirty(rect_);
}

void SelectionController::resizeTo(Rect r)
{
    if (!floating_ || r.w < 1 || r.h < 1)
        return;
    rect_ = r;
    pixels_ = scaled(original_, r.w, r.h);
    if (!originalMask_.isNull())
        mask_ = scaled(originalMask_, r.w, r.h);
    render();
}

void SelectionController::setTransparency(bool transparent, Rgba key)
{
    transparent_ = transparent;
    key_ = key;
    render();
}

void SelectionController::transform(const std::function<Image(const Image&)>& op,
                                    const std::function<Mask(const Mask&)>& maskOp, Rgba background)
{
    if (!active_)
        return;
    lift(false, background);
    pixels_ = op(pixels_);
    if (!mask_.isNull() && maskOp)
        mask_ = maskOp(mask_);
    if (!mask_.isNull() && (mask_.width() != pixels_.width() || mask_.height() != pixels_.height()))
        mask_ = scaled(mask_, pixels_.width(), pixels_.height());
    original_ = pixels_;
    originalMask_ = mask_;
    rect_.w = pixels_.width();
    rect_.h = pixels_.height();
    outline_.clear();
    render();
}

void SelectionController::commit()
{
    if (floating_) {
        render();
        doc_.commitEdit();
        base_ = Image();
        floating_ = false;
    }
    active_ = false;
    mask_ = Mask();
    originalMask_ = Mask();
    outline_.clear();
}

void SelectionController::clear()
{
    if (floating_)
        commit();
    active_ = false;
}

void SelectionController::deleteContents(Rgba background)
{
    if (!active_)
        return;
    if (floating_) {
        // The pixels were already lifted: dropping them leaves the hole behind.
        Image& img = doc_.image();
        const Rect old = drawn_.intersected(img.bounds());
        for (int y = old.y; y < old.bottom(); ++y)
            std::copy_n(base_.scanLine(y) + old.x, old.w, img.scanLine(y) + old.x);
        doc_.addDirty(old);
        doc_.commitEdit();
        base_ = Image();
        floating_ = false;
    } else {
        doc_.beginEdit();
        if (mask_.isNull())
            doc_.image().fillRect(rect_, background);
        else
            doc_.image().fillMasked(rect_, mask_, background);
        doc_.addDirty(rect_);
        doc_.commitEdit();
    }
    active_ = false;
    mask_ = Mask();
    outline_.clear();
}

Image SelectionController::extract(Rgba background) const
{
    if (!active_)
        return {};
    Image out = floating_ ? pixels_ : doc_.image().copy(rect_);
    if (!mask_.isNull()) {
        for (int y = 0; y < out.height(); ++y)
            for (int x = 0; x < out.width(); ++x)
                if (!mask_.test(x, y))
                    out.setPixel(x, y, background);
    }
    return out;
}

void SelectionController::paste(Image img, Point at)
{
    commit();
    doc_.beginEdit();
    base_ = doc_.image();
    pixels_ = std::move(img);
    original_ = pixels_;
    mask_ = Mask();
    originalMask_ = Mask();
    outline_.clear();
    rect_ = {at.x, at.y, pixels_.width(), pixels_.height()};
    drawn_ = {};
    active_ = true;
    floating_ = true;
    render();
}

} // namespace lp
