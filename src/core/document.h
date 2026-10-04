#pragma once

#include "raster/image.h"

#include <cstddef>
#include <deque>
#include <functional>

namespace lp {

// A reversible change: either a rectangle of pixels or a whole image (size changes).
struct UndoEntry {
    Rect rect;
    Image before;
    Image after;
    bool whole = false;

    std::size_t bytes() const { return before.byteSize() + after.byteSize(); }
};

class UndoStack {
public:
    // The PRD asks for at least 50 levels; memory is the real limit for very large images.
    explicit UndoStack(int maxLevels = 100, std::size_t memoryLimit = std::size_t(768) << 20);

    void push(UndoEntry entry);
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    int undoCount() const { return int(undo_.size()); }
    void undo(Image& img);
    void redo(Image& img);
    void clear();

private:
    void trim();
    static void apply(Image& img, const UndoEntry& e, bool forward);

    int maxLevels_;
    std::size_t memoryLimit_;
    std::size_t bytes_ = 0;
    std::deque<UndoEntry> undo_;
    std::deque<UndoEntry> redo_;
};

class Document {
public:
    Document(int width = 640, int height = 480);

    Image& image() { return image_; }
    const Image& image() const { return image_; }

    // Replaces the content without history (New / Open).
    void reset(Image img);

    bool isModified() const { return modified_; }
    void setModified(bool m) { modified_ = m; }

    // Edit session: tools draw on image() and can roll back to the snapshot taken here.
    void beginEdit();
    bool isEditing() const { return editing_; }
    const Image& snapshot() const { return snapshot_; }
    void restore(Rect r);
    void addDirty(Rect r) { dirty_ = dirty_.united(r.intersected(image_.bounds())); }
    Rect dirty() const { return dirty_; }
    void commitEdit();
    void cancelEdit();

    // Undoable whole-image replacement (resize, rotate, attributes...).
    void replaceImage(Image img);

    bool canUndo() const { return history_.canUndo(); }
    bool canRedo() const { return history_.canRedo(); }
    void undo();
    void redo();
    UndoStack& history() { return history_; }

    // Called with the changed rect (empty rect = everything / size changed).
    std::function<void(Rect)> onChanged;
    std::function<void()> onHistoryChanged;

private:
    void notify(Rect r);

    Image image_;
    Image snapshot_;
    Rect dirty_;
    bool editing_ = false;
    bool modified_ = false;
    UndoStack history_;
};

} // namespace lp
