#include "views.h"

#include "canvas.h"
#include "qtbridge.h"

#include <QCloseEvent>
#include <QPainter>

namespace app {

FullScreenView::FullScreenView(const QImage& image, QWidget* parent) : QWidget(parent), image_(image)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlag(Qt::Window);
    setCursor(Qt::BlankCursor);
    setAccessibleName(tr("Picture full screen"));
}

void FullScreenView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), palette().color(QPalette::Dark));
    // Same placement as the classic viewer: centered when smaller, top-left otherwise.
    const int x = std::max(0, (width() - image_.width()) / 2);
    const int y = std::max(0, (height() - image_.height()) / 2);
    p.drawImage(x, y, image_);
}

void FullScreenView::keyPressEvent(QKeyEvent*)
{
    close();
}

void FullScreenView::mousePressEvent(QMouseEvent*)
{
    close();
}

ThumbnailView::ThumbnailView(Editor* editor, Canvas* canvas, QWidget* parent)
    : QWidget(parent, Qt::Tool), editor_(editor), canvas_(canvas)
{
    setWindowTitle(tr("Thumbnail"));
    setAccessibleName(tr("Thumbnail"));
    resize(sizeHint());
    connect(editor_, &Editor::imageChanged, this, qOverload<>(&QWidget::update));
    connect(canvas_, &Canvas::viewChanged, this, qOverload<>(&QWidget::update));
}

void ThumbnailView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), palette().color(QPalette::Dark));
    const lp::Image& img = editor_->document().image();
    // Keep the area the canvas shows in the middle of the thumbnail.
    const QRect visible = canvas_->visibleImageRect();
    QPoint topLeft = visible.center() - QPoint(width() / 2, height() / 2);
    topLeft.setX(std::clamp(topLeft.x(), 0, std::max(0, img.width() - width())));
    topLeft.setY(std::clamp(topLeft.y(), 0, std::max(0, img.height() - height())));
    const QRect src = QRect(topLeft, size()).intersected(QRect(0, 0, img.width(), img.height()));
    p.drawImage(QPoint(0, 0), wrap(img), src);
}

void ThumbnailView::closeEvent(QCloseEvent* e)
{
    emit closed();
    QWidget::closeEvent(e);
}

} // namespace app
