#include "toolbox.h"

#include "icons.h"

#include <QButtonGroup>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>

namespace app {

namespace {

const char* const kIconNames[lp::kToolCount] = {
    "free-select", "rect-select", "eraser", "fill",   "pick-color", "magnifier", "pencil",  "brush",
    "airbrush",    "text",        "line",   "curve",  "rectangle",  "polygon",   "ellipse", "rounded-rect",
};

// Option glyphs are drawn, like the classic options box, rather than loaded from files.
QIcon paintedIcon(int w, int h, const std::function<void(QPainter&, const QColor&)>& draw, const QPalette& pal)
{
    QPixmap pm(w * 2, h * 2);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    draw(p, pal.color(QPalette::WindowText));
    p.end();
    return QIcon(pm);
}

} // namespace

QString toolName(lp::ToolId id)
{
    switch (id) {
    case lp::ToolId::FreeSelect: return ToolBox::tr("Free-Form Select");
    case lp::ToolId::RectSelect: return ToolBox::tr("Select");
    case lp::ToolId::Eraser: return ToolBox::tr("Eraser/Color Eraser");
    case lp::ToolId::Fill: return ToolBox::tr("Fill With Color");
    case lp::ToolId::PickColor: return ToolBox::tr("Pick Color");
    case lp::ToolId::Magnifier: return ToolBox::tr("Magnifier");
    case lp::ToolId::Pencil: return ToolBox::tr("Pencil");
    case lp::ToolId::Brush: return ToolBox::tr("Brush");
    case lp::ToolId::Airbrush: return ToolBox::tr("Airbrush");
    case lp::ToolId::Text: return ToolBox::tr("Text");
    case lp::ToolId::Line: return ToolBox::tr("Line");
    case lp::ToolId::Curve: return ToolBox::tr("Curve");
    case lp::ToolId::Rectangle: return ToolBox::tr("Rectangle");
    case lp::ToolId::Polygon: return ToolBox::tr("Polygon");
    case lp::ToolId::Ellipse: return ToolBox::tr("Ellipse");
    case lp::ToolId::RoundedRect: return ToolBox::tr("Rounded Rectangle");
    }
    return {};
}

QString toolStatusTip(lp::ToolId id)
{
    switch (id) {
    case lp::ToolId::FreeSelect: return ToolBox::tr("Selects a free-form part of the picture to move, copy, or edit.");
    case lp::ToolId::RectSelect: return ToolBox::tr("Selects a rectangular part of the picture to move, copy, or edit.");
    case lp::ToolId::Eraser: return ToolBox::tr("Erases a portion of the picture, using the selected eraser shape.");
    case lp::ToolId::Fill: return ToolBox::tr("Fills an area with the current drawing color.");
    case lp::ToolId::PickColor: return ToolBox::tr("Picks up a color from the picture for drawing.");
    case lp::ToolId::Magnifier: return ToolBox::tr("Changes the magnification.");
    case lp::ToolId::Pencil: return ToolBox::tr("Draws a free-form line one pixel wide.");
    case lp::ToolId::Brush: return ToolBox::tr("Draws using a brush with the selected shape and size.");
    case lp::ToolId::Airbrush: return ToolBox::tr("Draws using an airbrush of the selected size.");
    case lp::ToolId::Text: return ToolBox::tr("Inserts text into the picture.");
    case lp::ToolId::Line: return ToolBox::tr("Draws a straight line with the selected line width.");
    case lp::ToolId::Curve: return ToolBox::tr("Draws a curved line with the selected line width.");
    case lp::ToolId::Rectangle: return ToolBox::tr("Draws a rectangle with the selected fill style.");
    case lp::ToolId::Polygon: return ToolBox::tr("Draws a polygon with the selected fill style.");
    case lp::ToolId::Ellipse: return ToolBox::tr("Draws an ellipse with the selected fill style.");
    case lp::ToolId::RoundedRect: return ToolBox::tr("Draws a rounded rectangle with the selected fill style.");
    }
    return {};
}

ToolBox::ToolBox(Editor* editor, QWidget* parent) : QWidget(parent), editor_(editor)
{
    setAccessibleName(tr("Tool Box"));
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(4, 4, 4, 4);
    outer->setSpacing(6);

    auto* grid = new QGridLayout;
    grid->setSpacing(2);
    toolGroup_ = new QButtonGroup(this);
    toolGroup_->setExclusive(true);
    for (int i = 0; i < lp::kToolCount; ++i) {
        const auto id = lp::ToolId(i);
        auto* b = new QToolButton(this);
        b->setCheckable(true);
        b->setAutoRaise(true);
        b->setIconSize(QSize(24, 24));
        b->setToolTip(toolName(id));
        b->setStatusTip(toolStatusTip(id));
        b->setAccessibleName(toolName(id));
        b->setAccessibleDescription(toolStatusTip(id));
        toolGroup_->addButton(b, i);
        grid->addWidget(b, i / 2, i % 2);
        toolButtons_.append(b);
    }
    outer->addLayout(grid);

    optionsBox_ = new QFrame(this);
    static_cast<QFrame*>(optionsBox_)->setFrameShape(QFrame::StyledPanel);
    optionsBox_->setAccessibleName(tr("Tool Options"));
    optionsBox_->setMinimumHeight(92);
    optionsLayout_ = new QGridLayout(optionsBox_);
    optionsLayout_->setContentsMargins(3, 3, 3, 3);
    optionsLayout_->setSpacing(1);
    outer->addWidget(optionsBox_);
    outer->addStretch(1);

    connect(toolGroup_, &QButtonGroup::idClicked, this, [this](int id) { editor_->setTool(lp::ToolId(id)); });
    connect(editor_, &Editor::toolChanged, this, [this](lp::ToolId id) {
        toolButtons_[int(id)]->setChecked(true);
        rebuildOptions();
    });
    // Queued: an option button may be the sender, and rebuilding deletes it.
    connect(editor_, &Editor::optionsChanged, this, &ToolBox::rebuildOptions, Qt::QueuedConnection);
    connect(editor_, &Editor::zoomChanged, this, &ToolBox::rebuildOptions, Qt::QueuedConnection);

    toolButtons_[int(editor_->toolId())]->setChecked(true);
    refreshIcons();
    rebuildOptions();
}

void ToolBox::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::PaletteChange || e->type() == QEvent::StyleChange) {
        refreshIcons();
        rebuildOptions();
    }
    QWidget::changeEvent(e);
}

void ToolBox::refreshIcons()
{
    // Native auto-raise styles barely mark the checked tool; draw a filled, outlined frame instead.
    const QColor hl = palette().color(QPalette::Highlight);
    const auto rgba = [&hl](int alpha) {
        return QStringLiteral("rgba(%1, %2, %3, %4)").arg(hl.red()).arg(hl.green()).arg(hl.blue()).arg(alpha);
    };
    const QString css = QStringLiteral("QToolButton { border: 1px solid transparent; border-radius: 3px; padding: 2px; }"
                                       "QToolButton:hover { border-color: %1; background: %2; }"
                                       "QToolButton:checked, QToolButton:pressed { border: 2px solid %3; padding: 1px;"
                                       " background: %4; }")
                            .arg(rgba(140), rgba(30), hl.name(), rgba(90));
    for (int i = 0; i < toolButtons_.size(); ++i) {
        toolButtons_[i]->setIcon(themedIcon(QString::fromLatin1(kIconNames[i]), palette()));
        if (toolButtons_[i]->styleSheet() != css)
            toolButtons_[i]->setStyleSheet(css);
    }
}

QToolButton* ToolBox::addOption(const QIcon& icon, const QString& name, bool checked, int row, int col,
                                const std::function<void()>& onClick)
{
    auto* b = new QToolButton(optionsBox_);
    b->setCheckable(true);
    b->setAutoRaise(true);
    b->setChecked(checked);
    b->setIcon(icon);
    b->setToolTip(name);
    b->setAccessibleName(name);
    b->setFocusPolicy(Qt::TabFocus);
    optionsGroup_->addButton(b);
    optionsLayout_->addWidget(b, row, col);
    connect(b, &QToolButton::clicked, this, onClick);
    return b;
}

void ToolBox::rebuildOptions()
{
    while (QLayoutItem* item = optionsLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    delete optionsGroup_;
    optionsGroup_ = new QButtonGroup(this);
    optionsGroup_->setExclusive(true);

    const lp::ToolOptions& o = editor_->options();
    const QPalette pal = palette();

    switch (editor_->toolId()) {
    case lp::ToolId::FreeSelect:
    case lp::ToolId::RectSelect:
    case lp::ToolId::Text: {
        for (int i = 0; i < 2; ++i) {
            const bool transparent = i == 1;
            auto draw = [transparent](QPainter& p, const QColor& ink) {
                if (transparent) {
                    // Checkerboard: the background shows through.
                    for (int y = 0; y < 14; y += 3)
                        for (int x = 0; x < 22; x += 3)
                            p.fillRect(QRect(3 + x, 3 + y, 3, 3),
                                       ((x + y) / 3) % 2 ? QColor(204, 204, 204) : QColor(255, 255, 255));
                } else {
                    p.fillRect(QRect(3, 3, 22, 14), Qt::white);
                }
                p.setPen(ink);
                p.setBrush(QColor(61, 139, 253));
                p.drawEllipse(QRect(9, 6, 12, 9));
                p.setBrush(QColor(255, 193, 7));
                p.drawRect(QRect(5, 9, 8, 6));
                p.setBrush(Qt::NoBrush);
                p.drawRect(QRect(3, 3, 22, 14));
            };
            const QString name = transparent ? tr("Transparent background") : tr("Opaque background");
            auto* b = addOption(paintedIcon(28, 20, draw, pal), name, o.transparentSelection == transparent, i, 0,
                                [this, transparent] { editor_->setTransparentSelection(transparent); });
            b->setIconSize(QSize(28, 20));
        }
        break;
    }
    case lp::ToolId::Eraser: {
        static constexpr int sizes[] = {4, 6, 8, 10};
        for (int i = 0; i < 4; ++i) {
            const int s = sizes[i];
            auto draw = [s](QPainter& p, const QColor& ink) { p.fillRect(QRect(12 - s / 2, 6 - s / 2, s, s), ink); };
            addOption(paintedIcon(24, 12, draw, pal), tr("Eraser size %1 pixels").arg(s), o.eraserSize == s, i, 0,
                      [this, s] { editor_->setEraserSize(s); })
                ->setIconSize(QSize(24, 12));
        }
        break;
    }
    case lp::ToolId::Magnifier: {
        static constexpr int levels[] = {1, 2, 6, 8};
        for (int i = 0; i < 4; ++i) {
            const int z = levels[i];
            auto* b = addOption(QIcon(), tr("%1x").arg(z), editor_->zoom() == z, i, 0,
                                [this, z] { editor_->setMagnifierZoom(z); });
            b->setText(tr("%1x").arg(z));
            b->setAccessibleName(tr("Zoom %1x").arg(z));
            b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        }
        break;
    }
    case lp::ToolId::Brush: {
        for (int i = 0; i < 12; ++i) {
            const auto fp = lp::brushFootprint(lp::classicBrush(i));
            auto draw = [fp](QPainter& p, const QColor& ink) {
                for (const lp::Point& pt : fp)
                    p.fillRect(QRect(8 + pt.x, 8 + pt.y, 1, 1), ink);
            };
            static const char* const shapes[] = {QT_TR_NOOP("Round"), QT_TR_NOOP("Square"), QT_TR_NOOP("Slash"),
                                                 QT_TR_NOOP("Backslash")};
            const QString name = tr("%1 brush, size %2").arg(tr(shapes[i / 3])).arg(lp::classicBrush(i).size);
            addOption(paintedIcon(16, 16, draw, pal), name, o.brushIndex == i, i / 3, i % 3,
                      [this, i] { editor_->setBrushIndex(i); })
                ->setIconSize(QSize(16, 16));
        }
        break;
    }
    case lp::ToolId::Airbrush: {
        static constexpr int sizes[] = {9, 16, 24};
        for (int i = 0; i < 3; ++i) {
            const int s = sizes[i];
            auto draw = [s](QPainter& p, const QColor& ink) {
                std::mt19937 rng(s);
                std::uniform_int_distribution<int> d(-s / 2, s / 2);
                for (int k = 0; k < s * 2; ++k) {
                    const int x = d(rng), y = d(rng);
                    if (x * x + y * y <= s * s / 4)
                        p.fillRect(QRect(14 + x, 14 + y, 1, 1), ink);
                }
            };
            addOption(paintedIcon(28, 28, draw, pal), tr("Airbrush size %1").arg(s), o.airbrushSize == s, i / 2, i % 2,
                      [this, s] { editor_->setAirbrushSize(s); })
                ->setIconSize(QSize(28, 28));
        }
        break;
    }
    case lp::ToolId::Line:
    case lp::ToolId::Curve: {
        for (int w = 1; w <= 5; ++w) {
            auto draw = [w](QPainter& p, const QColor& ink) { p.fillRect(QRect(3, 6 - w / 2, 26, w), ink); };
            addOption(paintedIcon(32, 12, draw, pal), tr("Line width %1").arg(w), o.lineWidth == w, w - 1, 0,
                      [this, w] { editor_->setLineWidth(w); })
                ->setIconSize(QSize(32, 12));
        }
        break;
    }
    case lp::ToolId::Rectangle:
    case lp::ToolId::Polygon:
    case lp::ToolId::Ellipse:
    case lp::ToolId::RoundedRect: {
        static constexpr lp::FillStyle styles[] = {lp::FillStyle::Outline, lp::FillStyle::OutlineAndFill,
                                                   lp::FillStyle::Fill};
        static const char* const names[] = {QT_TR_NOOP("Outline"), QT_TR_NOOP("Outline and fill"),
                                            QT_TR_NOOP("Fill without outline")};
        for (int i = 0; i < 3; ++i) {
            const lp::FillStyle st = styles[i];
            auto draw = [st](QPainter& p, const QColor& ink) {
                const QRect r(4, 3, 24, 11);
                if (st != lp::FillStyle::Outline)
                    p.fillRect(r, QColor(128, 128, 128));
                if (st != lp::FillStyle::Fill) {
                    p.setPen(ink);
                    p.setBrush(Qt::NoBrush);
                    p.drawRect(r);
                }
            };
            addOption(paintedIcon(32, 17, draw, pal), tr(names[i]), o.fillStyle == st, i, 0,
                      [this, st] { editor_->setFillStyle(st); })
                ->setIconSize(QSize(32, 17));
        }
        break;
    }
    default:
        break;
    }
}

} // namespace app
