#pragma once

#include "editor.h"

#include <QAbstractScrollArea>
#include <QFont>
#include <QPointer>
#include <QTimer>

#include <QPlainTextEdit>

namespace app {

struct TextStyle {
    QString family = QStringLiteral("Sans Serif");
    int pointSize = 12;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool smooth = true;
};

class Canvas : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit Canvas(Editor* editor, QWidget* parent = nullptr);

    bool showGrid() const { return showGrid_; }
    void setShowGrid(bool on);
    void setTextStyle(const TextStyle& style);
    const TextStyle& textStyle() const { return textStyle_; }
    bool isEditingText() const { return textEdit_ != nullptr; }
    void commitText();
    void cancelText();

    // Image point shown at the top-left corner of the viewport.
    QPoint visibleImageOrigin() const;
    QRect visibleImageRect() const;

signals:
    void cursorMoved(const QPoint& imagePos, bool inside);
    void viewChanged();

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void scrollContentsBy(int dx, int dy) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void leaveEvent(QEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    enum class Drag { None, Tool, CanvasResize, SelectionResize, TextBox };
    enum Handle { None = -1, Right = 0, Bottom = 1, Corner = 2 };

    int zoom() const { return editor_->zoom(); }
    QPoint origin() const;
    QPoint toImage(const QPointF& widgetPos) const;
    QRect toWidget(const QRect& imageRect) const;
    QPoint toWidget(const QPoint& imagePoint) const;
    lp::PointerEvent pointerEvent(QMouseEvent* e, int button) const;
    int buttonIndex(Qt::MouseButton b) const;

    void updateScrollBars();
    void onImageChanged(const QRect& r);
    void onZoomChanged(int level, const QPoint& center);
    void updateCursor(const QPoint& widgetPos);
    void updateTickTimer();

    QList<QRect> canvasHandleRects() const;
    int canvasHandleAt(const QPoint& p) const;
    QList<QRect> selectionHandleRects() const;
    int selectionHandleAt(const QPoint& p) const;
    bool selectionToolActive() const;

    void startText(const QRect& box);
    void layoutTextEdit();
    QFont textFont(int zoomFactor) const;

    Editor* editor_;
    bool showGrid_ = false;
    int lastZoom_ = 1;
    Drag drag_ = Drag::None;
    int dragButton_ = 0;
    int handle_ = None;
    QSize resizePreview_;
    QRect selectionStartRect_;
    QPoint dragStart_;
    QPoint lastImagePos_;
    QTimer tickTimer_;

    TextStyle textStyle_;
    QPointer<QPlainTextEdit> textEdit_;
    QRect textBox_;
    QRect textBand_;
};

} // namespace app
