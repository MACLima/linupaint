#pragma once

#include "editor.h"

#include <QWidget>

namespace app {

class Canvas;

// View > View Bitmap: the picture alone on the whole screen; any key or click closes it.
class FullScreenView : public QWidget {
    Q_OBJECT
public:
    explicit FullScreenView(const QImage& image, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;

private:
    QImage image_;
};

// View > Zoom > Show Thumbnail: the area around the view at 100% while zoomed in.
class ThumbnailView : public QWidget {
    Q_OBJECT
public:
    ThumbnailView(Editor* editor, Canvas* canvas, QWidget* parent = nullptr);
    QSize sizeHint() const override { return {200, 150}; }

signals:
    void closed();

protected:
    void paintEvent(QPaintEvent* e) override;
    void closeEvent(QCloseEvent* e) override;

private:
    Editor* editor_;
    Canvas* canvas_;
};

} // namespace app
