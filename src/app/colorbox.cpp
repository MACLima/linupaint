#include "colorbox.h"

#include "qtbridge.h"

#include <QColorDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

namespace app {

const QList<QColor>& classicPalette()
{
    static const QList<QColor> colors = {
        QColor::fromRgb(0xFF000000u), QColor::fromRgb(0xFF808080u), QColor::fromRgb(0xFF800000u), QColor::fromRgb(0xFF808000u), QColor::fromRgb(0xFF008000u),
        QColor::fromRgb(0xFF008080u), QColor::fromRgb(0xFF000080u), QColor::fromRgb(0xFF800080u), QColor::fromRgb(0xFF808040u), QColor::fromRgb(0xFF004040u),
        QColor::fromRgb(0xFF0080FFu), QColor::fromRgb(0xFF004080u), QColor::fromRgb(0xFF8000FFu), QColor::fromRgb(0xFF804000u),
        QColor::fromRgb(0xFFFFFFFFu), QColor::fromRgb(0xFFC0C0C0u), QColor::fromRgb(0xFFFF0000u), QColor::fromRgb(0xFFFFFF00u), QColor::fromRgb(0xFF00FF00u),
        QColor::fromRgb(0xFF00FFFFu), QColor::fromRgb(0xFF0000FFu), QColor::fromRgb(0xFFFF00FFu), QColor::fromRgb(0xFFFFFF80u), QColor::fromRgb(0xFF00FF80u),
        QColor::fromRgb(0xFF80FFFFu), QColor::fromRgb(0xFF8080FFu), QColor::fromRgb(0xFFFF0080u), QColor::fromRgb(0xFFFF8040u),
    };
    return colors;
}

ColorSwatch::ColorSwatch(QColor color, QWidget* parent) : QAbstractButton(parent), color_(color)
{
    setFocusPolicy(Qt::TabFocus);
    setColor(color);
}

void ColorSwatch::setColor(const QColor& c)
{
    color_ = c;
    setAccessibleName(tr("Color %1").arg(c.name()));
    setToolTip(c.name());
    update();
}

void ColorSwatch::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const QRect r = rect().adjusted(0, 0, -1, -1);
    p.setPen(palette().color(QPalette::Mid));
    p.setBrush(color_);
    p.drawRect(r);
    if (hasFocus()) {
        p.setPen(QPen(palette().color(QPalette::Highlight), 1, Qt::DotLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r.adjusted(1, 1, -1, -1));
    }
}

void ColorSwatch::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
        emit picked(0);
    else if (e->button() == Qt::RightButton)
        emit picked(1);
}

void ColorSwatch::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
        emit editRequested();
}

void ColorSwatch::keyPressEvent(QKeyEvent* e)
{
    // Keyboard access: Space/Enter = primary, Shift+Space/Enter = secondary, F2 = edit.
    if (e->key() == Qt::Key_Space || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        emit picked((e->modifiers() & Qt::ShiftModifier) ? 1 : 0);
        return;
    }
    if (e->key() == Qt::Key_F2) {
        emit editRequested();
        return;
    }
    QAbstractButton::keyPressEvent(e);
}

CurrentColors::CurrentColors(Editor* editor, QWidget* parent) : QWidget(parent), editor_(editor)
{
    setFixedSize(34, 34);
    auto describe = [this] {
        setAccessibleName(tr("Current colors"));
        setAccessibleDescription(tr("Primary color %1, secondary color %2")
                                     .arg(toQColor(editor_->color(0)).name(), toQColor(editor_->color(1)).name()));
        setToolTip(accessibleDescription());
        update();
    };
    connect(editor_, &Editor::colorsChanged, this, describe);
    describe();
}

void CurrentColors::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setPen(palette().color(QPalette::Mid));
    p.setBrush(toQColor(editor_->color(1)));
    p.drawRect(QRect(13, 13, 18, 18));
    p.setBrush(toQColor(editor_->color(0)));
    p.drawRect(QRect(3, 3, 18, 18));
}

ColorBox::ColorBox(Editor* editor, QWidget* parent) : QWidget(parent), editor_(editor)
{
    setAccessibleName(tr("Color Box"));
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);
    layout->addWidget(new CurrentColors(editor_, this));

    auto* grid = new QGridLayout;
    grid->setSpacing(1);
    const auto& colors = classicPalette();
    for (int i = 0; i < colors.size(); ++i) {
        auto* s = new ColorSwatch(colors[i], this);
        connect(s, &ColorSwatch::picked, this, [this, s](int index) { editor_->setColor(index, toRgba(s->color())); });
        connect(s, &ColorSwatch::editRequested, this, [this, s] { editSwatch(s); });
        grid->addWidget(s, i / 14, i % 14);
        swatches_.append(s);
    }
    layout->addLayout(grid);
    layout->addStretch(1);
}

void ColorBox::editSwatch(ColorSwatch* swatch)
{
    const QColor c = QColorDialog::getColor(swatch->color(), this, tr("Edit Colors"));
    if (!c.isValid())
        return;
    swatch->setColor(c);
    editor_->setColor(0, toRgba(c));
}

void ColorBox::editPrimaryColor()
{
    const QColor current = toQColor(editor_->color(0));
    const QColor c = QColorDialog::getColor(current, this, tr("Edit Colors"));
    if (!c.isValid())
        return;
    // Like the classic Paint, the edited color replaces the matching palette cell.
    for (ColorSwatch* s : swatches_) {
        if (s->color() == current) {
            s->setColor(c);
            break;
        }
    }
    editor_->setColor(0, toRgba(c));
}

} // namespace app
