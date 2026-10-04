#pragma once

#include "core/tools.h"

#include <QObject>
#include <QRect>
#include <QSize>

#include <array>
#include <functional>
#include <memory>

namespace app {

// Owns the document, the selection, the colors and the tools; the canvas and the boxes observe it.
class Editor : public QObject, public lp::ToolHost {
    Q_OBJECT
public:
    explicit Editor(QObject* parent = nullptr);
    ~Editor() override;

    // lp::ToolHost
    lp::Document& document() override { return doc_; }
    lp::SelectionController& selection() override { return selection_; }
    lp::Rgba color(int index) const override { return colors_[index & 1]; }
    void setColor(int index, lp::Rgba c) override;
    const lp::ToolOptions& options() const override { return options_; }
    void repaint(lp::Rect imageRect) override;
    int zoom() const override { return zoom_; }
    void zoomAt(int level, lp::Point imagePoint) override;
    lp::Point visibleImageSize(int zoom) const override;
    void restorePreviousTool() override;
    void showSize(int w, int h) override { emit sizeHint(w, h); }

    const lp::Document& document() const { return doc_; }
    lp::ToolId toolId() const { return toolId_; }
    lp::Tool* tool() const { return tool_.get(); }
    void setTool(lp::ToolId id);
    // Completes multi-step tools and floating selections (before menu actions, saving, ...).
    void finishPending();

    void setZoom(int level);
    void setEraserSize(int v);
    void setBrushIndex(int v);
    void setAirbrushSize(int v);
    void setLineWidth(int v);
    void setFillStyle(lp::FillStyle v);
    void setMagnifierZoom(int v);
    void setTransparentSelection(bool v);

    std::function<QSize()> viewportSize; // in device-independent pixels

signals:
    void colorsChanged();
    void toolChanged(lp::ToolId id);
    void optionsChanged();
    // Image coordinates; a null rect means "everything" (also used when the size changes).
    void imageChanged(const QRect& rect);
    void zoomChanged(int level, const QPoint& imageCenter);
    void historyChanged();
    void sizeHint(int w, int h);

private:
    lp::Document doc_;
    lp::SelectionController selection_;
    std::array<lp::Rgba, 2> colors_{lp::kBlack, lp::kWhite};
    lp::ToolOptions options_;
    lp::ToolId toolId_ = lp::ToolId::Pencil;
    lp::ToolId previousTool_ = lp::ToolId::Pencil;
    std::unique_ptr<lp::Tool> tool_;
    int zoom_ = 1;
};

} // namespace app
