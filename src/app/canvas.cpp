#include "canvas.h"

#include "qtbridge.h"
#include "raster/transform.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextDocument>
#include <QWheelEvent>

#include <cmath>

namespace app {

namespace {

constexpr int kMargin = 4;      // gap between the viewport corner and the image
constexpr int kHandle = 5;      // size of the resize handles
constexpr int kHandleSpace = 12; // extra scrollable room for the handles

QPen dashedPen()
{
    QPen pen(Qt::black, 1, Qt::DashLine);
    pen.setCosmetic(true);
    pen.setDashPattern({3, 3});
    return pen;
}

void drawDashedRect(QPainter& p, const QRect& r)
{
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Qt::white, 1));
    p.drawRect(r);
    p.setPen(dashedPen());
    p.drawRect(r);
}

} // namespace

Canvas::Canvas(Editor* editor, QWidget* parent) : QAbstractScrollArea(parent), editor_(editor)
{
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setMouseTracking(true);
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
    setAccessibleName(tr("Drawing area"));
    horizontalScrollBar()->setSingleStep(16);
    verticalScrollBar()->setSingleStep(16);
    lastZoom_ = editor_->zoom();

    editor_->viewportSize = [this] { return viewport()->size(); };
    connect(editor_, &Editor::imageChanged, this, &Canvas::onImageChanged);
    connect(editor_, &Editor::zoomChanged, this, &Canvas::onZoomChanged);
    connect(editor_, &Editor::toolChanged, this, [this] {
        commitText();
        updateCursor(viewport()->mapFromGlobal(QCursor::pos()));
        viewport()->update();
    });
    connect(editor_, &Editor::colorsChanged, this, [this] { layoutTextEdit(); });
    connect(editor_, &Editor::optionsChanged, this, [this] { layoutTextEdit(); });

    tickTimer_.setInterval(20);
    connect(&tickTimer_, &QTimer::timeout, this, [this] {
        if (editor_->tool())
            editor_->tool()->tick(*editor_);
    });
    updateScrollBars();
}

void Canvas::setShowGrid(bool on)
{
    showGrid_ = on;
    viewport()->update();
}

void Canvas::setTextStyle(const TextStyle& style)
{
    textStyle_ = style;
    layoutTextEdit();
}

QPoint Canvas::origin() const
{
    return {kMargin - horizontalScrollBar()->value(), kMargin - verticalScrollBar()->value()};
}

QPoint Canvas::toImage(const QPointF& pos) const
{
    const QPoint o = origin();
    const int z = zoom();
    return {int(std::floor((pos.x() - o.x()) / z)), int(std::floor((pos.y() - o.y()) / z))};
}

QRect Canvas::toWidget(const QRect& r) const
{
    const QPoint o = origin();
    const int z = zoom();
    return {o.x() + r.x() * z, o.y() + r.y() * z, r.width() * z, r.height() * z};
}

QPoint Canvas::toWidget(const QPoint& p) const
{
    const QPoint o = origin();
    return {o.x() + p.x() * zoom(), o.y() + p.y() * zoom()};
}

QPoint Canvas::visibleImageOrigin() const
{
    const QPoint p = toImage(QPointF(0, 0));
    return {std::max(0, p.x()), std::max(0, p.y())};
}

QRect Canvas::visibleImageRect() const
{
    const QPoint a = toImage(QPointF(0, 0));
    const QPoint b = toImage(QPointF(viewport()->width() - 1, viewport()->height() - 1));
    const lp::Image& img = editor_->document().image();
    return QRect(a, b).intersected(QRect(0, 0, img.width(), img.height()));
}

void Canvas::updateScrollBars()
{
    const lp::Image& img = editor_->document().image();
    const int z = zoom();
    const int cw = img.width() * z + 2 * kMargin + kHandleSpace;
    const int ch = img.height() * z + 2 * kMargin + kHandleSpace;
    const QSize vp = viewport()->size();
    horizontalScrollBar()->setRange(0, std::max(0, cw - vp.width()));
    horizontalScrollBar()->setPageStep(vp.width());
    verticalScrollBar()->setRange(0, std::max(0, ch - vp.height()));
    verticalScrollBar()->setPageStep(vp.height());
}

void Canvas::onImageChanged(const QRect& r)
{
    if (r.isNull()) {
        updateScrollBars();
        layoutTextEdit();
        viewport()->update();
    } else {
        viewport()->update(toWidget(r).adjusted(-2, -2, 2, 2));
    }
    emit viewChanged();
}

void Canvas::onZoomChanged(int level, const QPoint& center)
{
    const QSize vp = viewport()->size();
    QPointF c;
    if (center.x() < 0) {
        c = QPointF((horizontalScrollBar()->value() + vp.width() / 2.0 - kMargin) / lastZoom_,
                    (verticalScrollBar()->value() + vp.height() / 2.0 - kMargin) / lastZoom_);
    } else {
        c = QPointF(center.x() + 0.5, center.y() + 0.5);
    }
    lastZoom_ = level;
    updateScrollBars();
    horizontalScrollBar()->setValue(int(c.x() * level + kMargin - vp.width() / 2.0));
    verticalScrollBar()->setValue(int(c.y() * level + kMargin - vp.height() / 2.0));
    layoutTextEdit();
    viewport()->update();
    emit viewChanged();
}

void Canvas::resizeEvent(QResizeEvent* e)
{
    QAbstractScrollArea::resizeEvent(e);
    updateScrollBars();
    emit viewChanged();
}

void Canvas::scrollContentsBy(int, int)
{
    layoutTextEdit();
    viewport()->update();
    emit viewChanged();
}

QList<QRect> Canvas::canvasHandleRects() const
{
    const lp::Image& img = editor_->document().image();
    const QRect r = toWidget(QRect(0, 0, img.width(), img.height()));
    const int h = kHandle;
    return {QRect(r.right() + 1, r.top() + r.height() / 2 - h / 2, h, h),
            QRect(r.left() + r.width() / 2 - h / 2, r.bottom() + 1, h, h), QRect(r.right() + 1, r.bottom() + 1, h, h)};
}

int Canvas::canvasHandleAt(const QPoint& p) const
{
    const auto rects = canvasHandleRects();
    for (int i = 0; i < rects.size(); ++i)
        if (rects[i].adjusted(-2, -2, 2, 2).contains(p))
            return i;
    return None;
}

bool Canvas::selectionToolActive() const
{
    const lp::ToolId t = editor_->toolId();
    return t == lp::ToolId::FreeSelect || t == lp::ToolId::RectSelect;
}

QList<QRect> Canvas::selectionHandleRects() const
{
    if (!selectionToolActive() || !editor_->selection().hasSelection())
        return {};
    const QRect r = toWidget(toQRect(editor_->selection().rect())).adjusted(-1, -1, 0, 0);
    const int h = kHandle;
    const int xs[3] = {r.left() - h + 1, r.left() + r.width() / 2 - h / 2, r.right()};
    const int ys[3] = {r.top() - h + 1, r.top() + r.height() / 2 - h / 2, r.bottom()};
    // Order: TL, T, TR, L, R, BL, B, BR.
    QList<QRect> out;
    for (int j = 0; j < 3; ++j)
        for (int i = 0; i < 3; ++i)
            if (!(i == 1 && j == 1))
                out.append(QRect(xs[i], ys[j], h, h));
    return out;
}

int Canvas::selectionHandleAt(const QPoint& p) const
{
    const auto rects = selectionHandleRects();
    for (int i = 0; i < rects.size(); ++i)
        if (rects[i].adjusted(-2, -2, 2, 2).contains(p))
            return i;
    return None;
}

void Canvas::paintEvent(QPaintEvent* e)
{
    QPainter p(viewport());
    const QRect clip = e->rect();
    p.fillRect(clip, palette().color(QPalette::Dark));

    const lp::Image& img = editor_->document().image();
    const int z = zoom();
    const QRect imgW = toWidget(QRect(0, 0, img.width(), img.height()));
    const QRect exposed = clip.intersected(imgW);
    if (!exposed.isEmpty()) {
        const QPoint a = toImage(exposed.topLeft());
        const QPoint b = toImage(exposed.bottomRight());
        const QRect src = QRect(a, b).intersected(QRect(0, 0, img.width(), img.height()));
        p.drawImage(toWidget(src), wrap(img), src);

        if (showGrid_ && z >= 4) {
            QPen grid(QColor(128, 128, 128), 1);
            grid.setCosmetic(true);
            p.setPen(grid);
            for (int x = src.left(); x <= src.right() + 1; ++x) {
                const int wx = toWidget(QPoint(x, 0)).x();
                p.drawLine(wx, std::max(imgW.top(), exposed.top()), wx, std::min(imgW.bottom(), exposed.bottom()));
            }
            for (int y = src.top(); y <= src.bottom() + 1; ++y) {
                const int wy = toWidget(QPoint(0, y)).y();
                p.drawLine(std::max(imgW.left(), exposed.left()), wy, std::min(imgW.right(), exposed.right()), wy);
            }
        }
    }

    // Canvas resize handles, or the outline of the size being dragged.
    const QColor handleColor = palette().color(QPalette::Highlight);
    if (drag_ == Drag::CanvasResize) {
        drawDashedRect(p, QRect(imgW.topLeft(), resizePreview_ * z).adjusted(0, 0, -1, -1));
    } else {
        for (const QRect& h : canvasHandleRects())
            p.fillRect(h, handleColor);
    }

    // Selection outline and handles.
    lp::SelectionController& sel = editor_->selection();
    if (sel.hasSelection()) {
        const QRect r = toWidget(toQRect(sel.rect())).adjusted(-1, -1, 0, 0);
        const auto outline = sel.outline();
        if (!outline.empty() && !sel.isFloating()) {
            QPolygon poly;
            for (const lp::Point& pt : outline)
                poly << toWidget(QPoint(pt.x, pt.y)) + QPoint(z / 2, z / 2);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(Qt::white, 1));
            p.drawPolygon(poly);
            p.setPen(dashedPen());
            p.drawPolygon(poly);
        }
        drawDashedRect(p, r);
        if (selectionToolActive())
            for (const QRect& h : selectionHandleRects())
                p.fillRect(h, handleColor);
    }

    if (lp::Tool* tool = editor_->tool()) {
        if (const auto band = tool->rubberBand())
            drawDashedRect(p, toWidget(toQRect(*band)).adjusted(0, 0, -1, -1));
        if (const auto* path = tool->lassoPath(); path && path->size() > 1) {
            QPolygon poly;
            for (const lp::Point& pt : *path)
                poly << toWidget(QPoint(pt.x, pt.y)) + QPoint(z / 2, z / 2);
            p.setPen(QPen(Qt::black, 1));
            p.drawPolyline(poly);
        }
        if (const auto frame = tool->hoverFrame()) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(Qt::black, 1));
            p.drawRect(toWidget(toQRect(*frame)).adjusted(0, 0, -1, -1));
        }
    }

    if (!textBand_.isNull())
        drawDashedRect(p, toWidget(textBand_).adjusted(0, 0, -1, -1));
    if (textEdit_)
        drawDashedRect(p, toWidget(textBox_).adjusted(-1, -1, 0, 0));
}

int Canvas::buttonIndex(Qt::MouseButton b) const
{
    return b == Qt::RightButton ? 1 : 0;
}

lp::PointerEvent Canvas::pointerEvent(QMouseEvent* e, int button) const
{
    lp::PointerEvent pe;
    pe.pos = toPoint(toImage(e->position()));
    pe.button = button;
    pe.shift = e->modifiers() & Qt::ShiftModifier;
    pe.ctrl = e->modifiers() & Qt::ControlModifier;
    return pe;
}

void Canvas::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton && e->button() != Qt::RightButton)
        return;
    const QPoint wp = e->position().toPoint();

    if (drag_ != Drag::None) {
        // The other button while dragging cancels the operation in progress.
        if (drag_ == Drag::Tool && editor_->tool())
            editor_->tool()->press(*editor_, pointerEvent(e, buttonIndex(e->button())));
        return;
    }

    if (textEdit_) {
        commitText();
        return;
    }

    if (e->button() == Qt::LeftButton) {
        if (const int h = canvasHandleAt(wp); h != None) {
            editor_->finishPending();
            drag_ = Drag::CanvasResize;
            handle_ = h;
            const lp::Image& img = editor_->document().image();
            resizePreview_ = QSize(img.width(), img.height());
            return;
        }
        if (const int h = selectionHandleAt(wp); h != None) {
            lp::SelectionController& sel = editor_->selection();
            sel.setTransparency(editor_->options().transparentSelection, editor_->color(1));
            if (!sel.isFloating())
                sel.lift(false, editor_->color(1));
            drag_ = Drag::SelectionResize;
            handle_ = h;
            selectionStartRect_ = toQRect(sel.rect());
            dragStart_ = toImage(e->position());
            return;
        }
    }

    if (editor_->toolId() == lp::ToolId::Text) {
        if (e->button() != Qt::LeftButton)
            return;
        drag_ = Drag::TextBox;
        dragStart_ = toImage(e->position());
        textBand_ = QRect(dragStart_, QSize(1, 1));
        return;
    }

    if (lp::Tool* tool = editor_->tool()) {
        drag_ = Drag::Tool;
        dragButton_ = buttonIndex(e->button());
        tool->press(*editor_, pointerEvent(e, dragButton_));
        updateTickTimer();
    }
}

void Canvas::mouseMoveEvent(QMouseEvent* e)
{
    const QPoint ip = toImage(e->position());
    const lp::Image& img = editor_->document().image();
    emit cursorMoved(ip, ip.x() >= 0 && ip.y() >= 0 && ip.x() < img.width() && ip.y() < img.height());
    lastImagePos_ = ip;

    switch (drag_) {
    case Drag::None:
        if (lp::Tool* tool = editor_->tool()) {
            tool->hover(*editor_, toPoint(ip));
            if (tool->hoverFrame())
                viewport()->update();
        }
        updateCursor(e->position().toPoint());
        return;
    case Drag::Tool:
        if (lp::Tool* tool = editor_->tool())
            tool->move(*editor_, pointerEvent(e, dragButton_));
        return;
    case Drag::CanvasResize: {
        QSize s = resizePreview_;
        if (handle_ == Right || handle_ == Corner)
            s.setWidth(std::max(1, ip.x()));
        if (handle_ == Bottom || handle_ == Corner)
            s.setHeight(std::max(1, ip.y()));
        resizePreview_ = s;
        editor_->showSize(s.width(), s.height());
        viewport()->update();
        return;
    }
    case Drag::SelectionResize: {
        QRect r = selectionStartRect_;
        const QPoint d = ip - dragStart_;
        // Handles: 0 TL, 1 T, 2 TR, 3 L, 4 R, 5 BL, 6 B, 7 BR.
        if (handle_ == 0 || handle_ == 3 || handle_ == 5)
            r.setLeft(std::min(r.left() + d.x(), r.right()));
        if (handle_ == 2 || handle_ == 4 || handle_ == 7)
            r.setRight(std::max(r.right() + d.x(), r.left()));
        if (handle_ == 0 || handle_ == 1 || handle_ == 2)
            r.setTop(std::min(r.top() + d.y(), r.bottom()));
        if (handle_ == 5 || handle_ == 6 || handle_ == 7)
            r.setBottom(std::max(r.bottom() + d.y(), r.top()));
        editor_->selection().resizeTo(toRect(r));
        editor_->showSize(r.width(), r.height());
        viewport()->update();
        return;
    }
    case Drag::TextBox:
        textBand_ = QRect(dragStart_, ip).normalized().intersected(QRect(0, 0, img.width(), img.height()));
        editor_->showSize(textBand_.width(), textBand_.height());
        viewport()->update();
        return;
    }
}

void Canvas::mouseReleaseEvent(QMouseEvent* e)
{
    const int button = buttonIndex(e->button());
    switch (drag_) {
    case Drag::None:
        return;
    case Drag::Tool:
        if (button != dragButton_)
            return;
        drag_ = Drag::None;
        if (lp::Tool* tool = editor_->tool())
            tool->release(*editor_, pointerEvent(e, button));
        updateTickTimer();
        break;
    case Drag::CanvasResize: {
        drag_ = Drag::None;
        lp::Document& doc = editor_->document();
        if (resizePreview_ != QSize(doc.image().width(), doc.image().height()))
            doc.replaceImage(
                lp::resizeCanvas(doc.image(), resizePreview_.width(), resizePreview_.height(), editor_->color(1)));
        viewport()->update();
        break;
    }
    case Drag::SelectionResize:
        drag_ = Drag::None;
        viewport()->update();
        break;
    case Drag::TextBox: {
        drag_ = Drag::None;
        QRect box = textBand_;
        textBand_ = QRect();
        if (box.width() < 8 || box.height() < 8) {
            // A plain click opens a default-sized box at the pointer.
            const lp::Image& img = editor_->document().image();
            box = QRect(dragStart_, QSize(120, 2 * QFontMetrics(textFont(1)).height()))
                      .intersected(QRect(0, 0, img.width(), img.height()));
        }
        if (!box.isEmpty())
            startText(box);
        viewport()->update();
        break;
    }
    }
    updateCursor(e->position().toPoint());
}

void Canvas::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (drag_ == Drag::None && editor_->tool() && editor_->toolId() == lp::ToolId::Polygon) {
        editor_->tool()->doubleClick(*editor_, pointerEvent(e, buttonIndex(e->button())));
        return;
    }
    mousePressEvent(e);
}

void Canvas::wheelEvent(QWheelEvent* e)
{
    if (e->modifiers() & Qt::ControlModifier) {
        const int z = zoom();
        static constexpr int levels[] = {1, 2, 4, 6, 8};
        int idx = 0;
        while (idx < 4 && levels[idx] < z)
            ++idx;
        idx = std::clamp(idx + (e->angleDelta().y() > 0 ? 1 : -1), 0, 4);
        editor_->zoomAt(levels[idx], toPoint(toImage(e->position())));
        return;
    }
    QAbstractScrollArea::wheelEvent(e);
}

void Canvas::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        if (drag_ == Drag::CanvasResize || drag_ == Drag::TextBox) {
            drag_ = Drag::None;
            textBand_ = QRect();
        } else if (lp::Tool* tool = editor_->tool()) {
            tool->cancel(*editor_);
            drag_ = Drag::None;
            updateTickTimer();
        }
        viewport()->update();
        return;
    }
    QAbstractScrollArea::keyPressEvent(e);
}

void Canvas::leaveEvent(QEvent* e)
{
    emit cursorMoved(QPoint(), false);
    QAbstractScrollArea::leaveEvent(e);
}

void Canvas::updateTickTimer()
{
    const bool on = drag_ == Drag::Tool && editor_->tool() && editor_->tool()->wantsTicks();
    if (on && !tickTimer_.isActive())
        tickTimer_.start();
    else if (!on)
        tickTimer_.stop();
}

void Canvas::updateCursor(const QPoint& wp)
{
    static constexpr Qt::CursorShape selectionCursors[] = {Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor,
                                                           Qt::SizeHorCursor,   Qt::SizeHorCursor, Qt::SizeBDiagCursor,
                                                           Qt::SizeVerCursor,   Qt::SizeFDiagCursor};
    static constexpr Qt::CursorShape canvasCursors[] = {Qt::SizeHorCursor, Qt::SizeVerCursor, Qt::SizeFDiagCursor};
    if (drag_ == Drag::None) {
        if (const int h = canvasHandleAt(wp); h != None) {
            viewport()->setCursor(canvasCursors[h]);
            return;
        }
        if (const int h = selectionHandleAt(wp); h != None) {
            viewport()->setCursor(selectionCursors[h]);
            return;
        }
    }
    switch (editor_->toolId()) {
    case lp::ToolId::FreeSelect:
    case lp::ToolId::RectSelect: {
        const lp::SelectionController& sel = editor_->selection();
        viewport()->setCursor(sel.hasSelection() && sel.rect().contains(toPoint(toImage(wp))) ? Qt::SizeAllCursor
                                                                                              : Qt::CrossCursor);
        break;
    }
    case lp::ToolId::Text:
        viewport()->setCursor(Qt::IBeamCursor);
        break;
    case lp::ToolId::Magnifier:
        viewport()->setCursor(Qt::PointingHandCursor);
        break;
    default:
        viewport()->setCursor(Qt::CrossCursor);
        break;
    }
}

QFont Canvas::textFont(int zoomFactor) const
{
    QFont f(textStyle_.family);
    // Paint sizes are points at 96 dpi; the image is in pixels.
    f.setPixelSize(std::max(1, int(std::lround(textStyle_.pointSize * 96.0 / 72.0)) * zoomFactor));
    f.setBold(textStyle_.bold);
    f.setItalic(textStyle_.italic);
    f.setUnderline(textStyle_.underline);
    f.setStyleStrategy(textStyle_.smooth ? QFont::PreferAntialias : QFont::NoAntialias);
    return f;
}

void Canvas::startText(const QRect& box)
{
    textBox_ = box;
    auto* edit = new QPlainTextEdit(viewport());
    edit->setFrameShape(QFrame::NoFrame);
    edit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    edit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit->document()->setDocumentMargin(0);
    edit->setAccessibleName(tr("Text"));
    edit->installEventFilter(this);
    connect(edit, &QPlainTextEdit::textChanged, this, [this] {
        if (!textEdit_)
            return;
        // Grow downwards to fit the text, as the classic text box does.
        const int lines = int(textEdit_->document()->size().height());
        const int needed = lines * QFontMetrics(textFont(1)).lineSpacing() + 2;
        const lp::Image& img = editor_->document().image();
        if (needed > textBox_.height()) {
            textBox_.setHeight(std::min(needed, img.height() - textBox_.top()));
            layoutTextEdit();
            viewport()->update();
        }
    });
    textEdit_ = edit;
    layoutTextEdit();
    edit->show();
    edit->setFocus();
    viewport()->update();
}

void Canvas::layoutTextEdit()
{
    if (!textEdit_)
        return;
    const QColor fg = toQColor(editor_->color(0));
    const QColor bg = toQColor(editor_->color(1));
    const bool transparent = editor_->options().transparentSelection;
    textEdit_->setFont(textFont(zoom()));
    textEdit_->setStyleSheet(QStringLiteral("QPlainTextEdit { color: %1; background: %2; padding: 0; }")
                                 .arg(fg.name(), transparent ? QStringLiteral("transparent") : bg.name()));
    textEdit_->setGeometry(toWidget(textBox_));
}

void Canvas::commitText()
{
    if (!textEdit_)
        return;
    const QString text = textEdit_->toPlainText();
    const QRect box = textBox_;
    QPlainTextEdit* edit = textEdit_;
    textEdit_ = nullptr;
    edit->deleteLater();
    lp::Document& doc = editor_->document();
    if (!text.isEmpty() || !editor_->options().transparentSelection) {
        doc.beginEdit();
        lp::Image& img = doc.image();
        QImage target(reinterpret_cast<uchar*>(img.data()), img.width(), img.height(), img.width() * 4,
                      QImage::Format_ARGB32);
        QPainter p(&target);
        if (!editor_->options().transparentSelection)
            p.fillRect(box, toQColor(editor_->color(1)));
        p.setRenderHint(QPainter::TextAntialiasing, textStyle_.smooth);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setFont(textFont(1));
        p.setPen(toQColor(editor_->color(0)));
        p.drawText(box, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, text);
        p.end();
        doc.addDirty(toRect(box));
        doc.commitEdit();
    }
    setFocus();
    onImageChanged(box);
    viewport()->update();
}

void Canvas::cancelText()
{
    if (!textEdit_)
        return;
    textEdit_->deleteLater();
    textEdit_ = nullptr;
    setFocus();
    viewport()->update();
}

bool Canvas::eventFilter(QObject* obj, QEvent* e)
{
    if (obj == textEdit_ && e->type() == QEvent::KeyPress) {
        if (static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape) {
            cancelText();
            return true;
        }
    }
    return QAbstractScrollArea::eventFilter(obj, e);
}

} // namespace app
