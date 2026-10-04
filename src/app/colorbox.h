#pragma once

#include "editor.h"

#include <QAbstractButton>
#include <QWidget>

namespace app {

// One palette cell: left click sets the primary color, right click the secondary,
// double click edits it.
class ColorSwatch : public QAbstractButton {
    Q_OBJECT
public:
    explicit ColorSwatch(QColor color, QWidget* parent = nullptr);
    QColor color() const { return color_; }
    void setColor(const QColor& c);
    QSize sizeHint() const override { return {17, 17}; }

signals:
    void picked(int index); // 0 = primary, 1 = secondary
    void editRequested();

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    QColor color_;
};

class CurrentColors : public QWidget {
    Q_OBJECT
public:
    explicit CurrentColors(Editor* editor, QWidget* parent = nullptr);
    QSize sizeHint() const override { return {34, 34}; }

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    Editor* editor_;
};

class ColorBox : public QWidget {
    Q_OBJECT
public:
    explicit ColorBox(Editor* editor, QWidget* parent = nullptr);
    // Colors > Edit Colors: edits the primary color.
    void editPrimaryColor();

private:
    void editSwatch(ColorSwatch* swatch);

    Editor* editor_;
    QList<ColorSwatch*> swatches_;
};

// The 28 colors of the classic palette (two rows of 14).
const QList<QColor>& classicPalette();

} // namespace app
