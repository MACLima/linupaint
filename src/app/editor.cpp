#include "editor.h"

#include "qtbridge.h"

namespace app {

Editor::Editor(QObject* parent) : QObject(parent), doc_(640, 480), selection_(doc_)
{
    tool_ = lp::createTool(toolId_);
    doc_.onChanged = [this](lp::Rect r) { emit imageChanged(r.empty() ? QRect() : toQRect(r)); };
    doc_.onHistoryChanged = [this] { emit historyChanged(); };
}

Editor::~Editor()
{
    doc_.onChanged = nullptr;
    doc_.onHistoryChanged = nullptr;
}

void Editor::setColor(int index, lp::Rgba c)
{
    if (colors_[index & 1] == c)
        return;
    colors_[index & 1] = c;
    if (index == 1 && selection_.isFloating())
        selection_.setTransparency(options_.transparentSelection, c);
    emit colorsChanged();
}

void Editor::repaint(lp::Rect imageRect)
{
    emit imageChanged(imageRect.empty() ? QRect() : toQRect(imageRect));
}

void Editor::zoomAt(int level, lp::Point imagePoint)
{
    level = std::clamp(level, 1, 8);
    zoom_ = level;
    emit zoomChanged(level, toQPoint(imagePoint));
}

void Editor::setZoom(int level)
{
    zoom_ = std::clamp(level, 1, 8);
    // (-1, -1): keep the current view center.
    emit zoomChanged(zoom_, QPoint(-1, -1));
}

lp::Point Editor::visibleImageSize(int zoom) const
{
    const QSize vp = viewportSize ? viewportSize() : QSize(640, 480);
    zoom = std::max(1, zoom);
    return {vp.width() / zoom, vp.height() / zoom};
}

void Editor::setTool(lp::ToolId id)
{
    if (tool_)
        tool_->finish(*this);
    if (id != lp::ToolId::FreeSelect && id != lp::ToolId::RectSelect)
        selection_.commit();
    if (id == lp::ToolId::PickColor && toolId_ != lp::ToolId::PickColor)
        previousTool_ = toolId_;
    toolId_ = id;
    tool_ = lp::createTool(id);
    emit toolChanged(id);
    emit imageChanged(QRect());
}

void Editor::restorePreviousTool()
{
    setTool(previousTool_);
}

void Editor::finishPending()
{
    if (tool_)
        tool_->finish(*this);
    selection_.commit();
    emit imageChanged(QRect());
}

void Editor::setEraserSize(int v)
{
    options_.eraserSize = v;
    emit optionsChanged();
}

void Editor::setBrushIndex(int v)
{
    options_.brushIndex = v;
    emit optionsChanged();
}

void Editor::setAirbrushSize(int v)
{
    options_.airbrushSize = v;
    emit optionsChanged();
}

void Editor::setLineWidth(int v)
{
    options_.lineWidth = v;
    emit optionsChanged();
}

void Editor::setFillStyle(lp::FillStyle v)
{
    options_.fillStyle = v;
    emit optionsChanged();
}

void Editor::setMagnifierZoom(int v)
{
    options_.magnifierZoom = v;
    emit optionsChanged();
    // Picking a level in the options box zooms right away, as in the classic Paint.
    setZoom(v);
}

void Editor::setTransparentSelection(bool v)
{
    options_.transparentSelection = v;
    selection_.setTransparency(v, colors_[1]);
    emit optionsChanged();
    emit imageChanged(QRect());
}

} // namespace app
