#include "core/document.h"

#include <utility>

namespace lp {

UndoStack::UndoStack(int maxLevels, std::size_t memoryLimit) : maxLevels_(maxLevels), memoryLimit_(memoryLimit) {}

void UndoStack::push(UndoEntry entry)
{
    for (const UndoEntry& e : redo_)
        bytes_ -= e.bytes();
    redo_.clear();
    bytes_ += entry.bytes();
    undo_.push_back(std::move(entry));
    trim();
}

void UndoStack::trim()
{
    while (!undo_.empty() && (int(undo_.size()) > maxLevels_ || (bytes_ > memoryLimit_ && undo_.size() > 1))) {
        bytes_ -= undo_.front().bytes();
        undo_.pop_front();
    }
}

void UndoStack::apply(Image& img, const UndoEntry& e, bool forward)
{
    const Image& src = forward ? e.after : e.before;
    if (e.whole)
        img = src;
    else
        img.blit(src, {e.rect.x, e.rect.y});
}

void UndoStack::undo(Image& img)
{
    if (undo_.empty())
        return;
    UndoEntry e = std::move(undo_.back());
    undo_.pop_back();
    apply(img, e, false);
    redo_.push_back(std::move(e));
}

void UndoStack::redo(Image& img)
{
    if (redo_.empty())
        return;
    UndoEntry e = std::move(redo_.back());
    redo_.pop_back();
    apply(img, e, true);
    undo_.push_back(std::move(e));
}

void UndoStack::clear()
{
    undo_.clear();
    redo_.clear();
    bytes_ = 0;
}

Document::Document(int width, int height) : image_(width, height, kWhite) {}

void Document::notify(Rect r)
{
    if (onChanged)
        onChanged(r);
}

void Document::reset(Image img)
{
    cancelEdit();
    image_ = std::move(img);
    history_.clear();
    modified_ = false;
    notify({});
    if (onHistoryChanged)
        onHistoryChanged();
}

void Document::beginEdit()
{
    if (editing_)
        return;
    snapshot_ = image_;
    dirty_ = {};
    editing_ = true;
}

void Document::restore(Rect r)
{
    if (!editing_)
        return;
    r = r.intersected(image_.bounds());
    if (r.empty())
        return;
    for (int y = r.y; y < r.bottom(); ++y)
        std::copy_n(snapshot_.scanLine(y) + r.x, r.w, image_.scanLine(y) + r.x);
}

void Document::commitEdit()
{
    if (!editing_)
        return;
    editing_ = false;
    if (!dirty_.empty()) {
        UndoEntry e;
        e.rect = dirty_;
        e.before = snapshot_.copy(dirty_);
        e.after = image_.copy(dirty_);
        if (!(e.before == e.after)) {
            history_.push(std::move(e));
            modified_ = true;
            if (onHistoryChanged)
                onHistoryChanged();
        }
    }
    snapshot_ = Image();
    dirty_ = {};
}

void Document::cancelEdit()
{
    if (!editing_)
        return;
    const Rect d = dirty_;
    restore(d);
    editing_ = false;
    snapshot_ = Image();
    dirty_ = {};
    if (!d.empty())
        notify(d);
}

void Document::replaceImage(Image img)
{
    commitEdit();
    UndoEntry e;
    e.whole = true;
    e.before = image_;
    e.after = img;
    e.rect = img.bounds();
    image_ = std::move(img);
    history_.push(std::move(e));
    modified_ = true;
    notify({});
    if (onHistoryChanged)
        onHistoryChanged();
}

void Document::undo()
{
    commitEdit();
    const int w = image_.width(), h = image_.height();
    history_.undo(image_);
    modified_ = true;
    notify((w == image_.width() && h == image_.height()) ? image_.bounds() : Rect{});
    if (onHistoryChanged)
        onHistoryChanged();
}

void Document::redo()
{
    commitEdit();
    const int w = image_.width(), h = image_.height();
    history_.redo(image_);
    modified_ = true;
    notify((w == image_.width() && h == image_.height()) ? image_.bounds() : Rect{});
    if (onHistoryChanged)
        onHistoryChanged();
}

} // namespace lp
