#include "dialogs.h"

#include "toolbox.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace app {

namespace {

QDialogButtonBox* okCancel(QDialog* d)
{
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, d);
    QObject::connect(box, &QDialogButtonBox::accepted, d, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, d, &QDialog::reject);
    return box;
}

} // namespace

FlipRotateDialog::FlipRotateDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Flip and Rotate"));
    auto* layout = new QVBoxLayout(this);
    auto* group = new QGroupBox(tr("Flip or rotate"), this);
    auto* g = new QVBoxLayout(group);
    action_ = new QButtonGroup(this);
    angle_ = new QButtonGroup(this);
    auto* fh = new QRadioButton(tr("&Flip horizontal"), group);
    auto* fv = new QRadioButton(tr("Flip &vertical"), group);
    auto* rot = new QRadioButton(tr("&Rotate by angle"), group);
    fh->setChecked(true);
    action_->addButton(fh, FlipHorizontal);
    action_->addButton(fv, FlipVertical);
    action_->addButton(rot, Rotate);
    g->addWidget(fh);
    g->addWidget(fv);
    g->addWidget(rot);
    auto* angles = new QVBoxLayout;
    angles->setContentsMargins(24, 0, 0, 0);
    const int values[] = {90, 180, 270};
    for (int v : values) {
        auto* b = new QRadioButton(tr("%1°").arg(v), group);
        b->setEnabled(false);
        angle_->addButton(b, v);
        angles->addWidget(b);
        if (v == 90)
            b->setChecked(true);
    }
    g->addLayout(angles);
    connect(rot, &QRadioButton::toggled, this, [this](bool on) {
        for (QAbstractButton* b : angle_->buttons())
            b->setEnabled(on);
    });
    layout->addWidget(group);
    layout->addWidget(okCancel(this));
}

FlipRotateDialog::Action FlipRotateDialog::action() const
{
    return Action(action_->checkedId());
}

int FlipRotateDialog::degrees() const
{
    return angle_->checkedId();
}

StretchSkewDialog::StretchSkewDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Stretch and Skew"));
    auto* layout = new QVBoxLayout(this);
    auto make = [this](int min, int max, int value, const QString& suffix) {
        auto* s = new QSpinBox(this);
        s->setRange(min, max);
        s->setValue(value);
        s->setSuffix(suffix);
        return s;
    };
    auto* stretch = new QGroupBox(tr("Stretch"), this);
    auto* sf = new QFormLayout(stretch);
    sx_ = make(1, 500, 100, tr(" %"));
    sy_ = make(1, 500, 100, tr(" %"));
    sf->addRow(tr("&Horizontal:"), sx_);
    sf->addRow(tr("&Vertical:"), sy_);
    auto* skew = new QGroupBox(tr("Skew"), this);
    auto* kf = new QFormLayout(skew);
    kx_ = make(-89, 89, 0, tr(" degrees"));
    ky_ = make(-89, 89, 0, tr(" degrees"));
    kf->addRow(tr("H&orizontal:"), kx_);
    kf->addRow(tr("V&ertical:"), ky_);
    layout->addWidget(stretch);
    layout->addWidget(skew);
    layout->addWidget(okCancel(this));
}

int StretchSkewDialog::stretchX() const { return sx_->value(); }
int StretchSkewDialog::stretchY() const { return sy_->value(); }
int StretchSkewDialog::skewX() const { return kx_->value(); }
int StretchSkewDialog::skewY() const { return ky_->value(); }

AttributesDialog::AttributesDialog(int width, int height, const Info& info, QSize defaultSize, QWidget* parent)
    : QDialog(parent), pxWidth_(width), pxHeight_(height), default_(defaultSize)
{
    setWindowTitle(tr("Attributes"));
    auto* layout = new QVBoxLayout(this);

    const QLocale loc;
    auto* facts = new QFormLayout;
    facts->addRow(tr("File last saved:"),
                  new QLabel(info.lastSaved.isValid() ? loc.toString(info.lastSaved, QLocale::ShortFormat)
                                                      : tr("Not Available"),
                             this));
    facts->addRow(tr("Size on disk:"),
                  new QLabel(info.sizeOnDisk >= 0 ? loc.formattedDataSize(info.sizeOnDisk) : tr("Not Available"),
                             this));
    facts->addRow(tr("Resolution:"), new QLabel(tr("96 x 96 dots per inch"), this));
    layout->addLayout(facts);

    auto* size = new QFormLayout;
    width_ = new QDoubleSpinBox(this);
    height_ = new QDoubleSpinBox(this);
    for (QDoubleSpinBox* s : {width_, height_})
        s->setRange(0.01, 100000);
    size->addRow(tr("&Width:"), width_);
    size->addRow(tr("&Height:"), height_);
    layout->addLayout(size);

    auto* unitsBox = new QGroupBox(tr("Units"), this);
    auto* ul = new QHBoxLayout(unitsBox);
    units_ = new QButtonGroup(this);
    const QString unitNames[] = {tr("&Inches"), tr("C&m"), tr("&Pixels")};
    for (int i = 0; i < 3; ++i) {
        auto* b = new QRadioButton(unitNames[i], unitsBox);
        units_->addButton(b, i);
        ul->addWidget(b);
    }
    units_->button(Pixels)->setChecked(true);
    layout->addWidget(unitsBox);

    auto* colorsBox = new QGroupBox(tr("Colors"), this);
    auto* cl = new QHBoxLayout(colorsBox);
    bw_ = new QRadioButton(tr("&Black and white"), colorsBox);
    auto* colors = new QRadioButton(tr("&Colors"), colorsBox);
    colors->setChecked(true);
    cl->addWidget(bw_);
    cl->addWidget(colors);
    layout->addWidget(colorsBox);

    auto* buttons = okCancel(this);
    auto* def = buttons->addButton(tr("&Default"), QDialogButtonBox::ResetRole);
    connect(def, &QPushButton::clicked, this, [this] {
        pxWidth_ = default_.width();
        pxHeight_ = default_.height();
        setUnit(unit_);
    });
    layout->addWidget(buttons);

    connect(units_, &QButtonGroup::idClicked, this, [this](int id) {
        pxWidth_ = toPixels(width_->value());
        pxHeight_ = toPixels(height_->value());
        setUnit(Unit(id));
    });
    setUnit(Pixels);
}

double AttributesDialog::fromPixels(int px) const
{
    switch (unit_) {
    case Inches: return px / 96.0;
    case Centimeters: return px / 96.0 * 2.54;
    case Pixels: return px;
    }
    return px;
}

int AttributesDialog::toPixels(double v) const
{
    switch (unit_) {
    case Inches: return std::max(1, int(std::lround(v * 96.0)));
    case Centimeters: return std::max(1, int(std::lround(v / 2.54 * 96.0)));
    case Pixels: return std::max(1, int(std::lround(v)));
    }
    return 1;
}

void AttributesDialog::setUnit(Unit u)
{
    unit_ = u;
    const int decimals = u == Pixels ? 0 : 2;
    for (QDoubleSpinBox* s : {width_, height_})
        s->setDecimals(decimals);
    width_->setValue(fromPixels(pxWidth_));
    height_->setValue(fromPixels(pxHeight_));
}

int AttributesDialog::widthPixels() const { return toPixels(width_->value()); }
int AttributesDialog::heightPixels() const { return toPixels(height_->value()); }
bool AttributesDialog::blackAndWhite() const { return bw_->isChecked(); }

CustomZoomDialog::CustomZoomDialog(int current, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Custom Zoom"));
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Current zoom: %1%").arg(current * 100), this));
    auto* box = new QGroupBox(tr("Zoom to"), this);
    auto* g = new QVBoxLayout(box);
    group_ = new QButtonGroup(this);
    for (int z : {1, 2, 4, 6, 8}) {
        auto* b = new QRadioButton(tr("%1%").arg(z * 100), box);
        group_->addButton(b, z);
        g->addWidget(b);
        if (z == current)
            b->setChecked(true);
    }
    if (!group_->checkedButton())
        group_->button(1)->setChecked(true);
    layout->addWidget(box);
    layout->addWidget(okCancel(this));
}

int CustomZoomDialog::zoom() const
{
    return group_->checkedId();
}

HelpDialog::HelpDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("LinuPaint Help"));
    resize(640, 560);
    auto* layout = new QVBoxLayout(this);
    auto* browser = new QTextBrowser(this);
    browser->setOpenExternalLinks(true);

    QString tools;
    for (int i = 0; i < lp::kToolCount; ++i)
        tools += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td></tr>")
                     .arg(toolName(lp::ToolId(i)).toHtmlEscaped(), toolStatusTip(lp::ToolId(i)).toHtmlEscaped());

    const QList<QPair<QString, QString>> keys = {
        {tr("Ctrl+N / Ctrl+O / Ctrl+S"), tr("New, open and save a picture")},
        {tr("Ctrl+Z / Ctrl+Y"), tr("Undo and redo")},
        {tr("Ctrl+X / Ctrl+C / Ctrl+V"), tr("Cut, copy and paste")},
        {tr("Del"), tr("Clear the selection")},
        {tr("Ctrl+A"), tr("Select the whole picture")},
        {tr("Ctrl+R"), tr("Flip or rotate")},
        {tr("Ctrl+W"), tr("Stretch or skew")},
        {tr("Ctrl+I"), tr("Invert colors")},
        {tr("Ctrl+E"), tr("Picture attributes")},
        {tr("Ctrl+Shift+N"), tr("Clear the picture")},
        {tr("Ctrl+T / Ctrl+L"), tr("Show or hide the tool box and the color box")},
        {tr("Ctrl+Page Up / Ctrl+Page Down"), tr("Normal size and large size zoom")},
        {tr("Ctrl+G"), tr("Show or hide the grid (zoom 400% or more)")},
        {tr("Ctrl+F"), tr("View the picture full screen")},
        {tr("Esc"), tr("Cancel the shape being drawn")},
    };
    QString keyRows;
    for (const auto& [k, v] : keys)
        keyRows += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td></tr>").arg(k.toHtmlEscaped(), v.toHtmlEscaped());

    const QStringList tips = {
        tr("The left mouse button draws with the primary color; the right button draws with the secondary color."),
        tr("Hold Shift to draw horizontal, vertical or 45° lines, squares and circles."),
        tr("Drag a selection with Ctrl to copy it, or with Shift to leave a trail of copies."),
        tr("While drawing a shape, press Esc or the other mouse button to cancel it."),
        tr("Drag the handles at the edges of the picture to change its size."),
        tr("Double-click a color in the color box to edit it."),
        tr("Extra: Extras > Insert Emoji adds an emoji as a selection you can move and resize. This is not part of "
           "the classic Paint."),
    };
    QString tipItems;
    for (const QString& t : tips)
        tipItems += QStringLiteral("<li>%1</li>").arg(t.toHtmlEscaped());

    browser->setHtml(QStringLiteral("<h2>%1</h2><p>%2</p><h3>%3</h3><table cellpadding='3'>%4</table>"
                                    "<h3>%5</h3><ul>%6</ul><h3>%7</h3><table cellpadding='3'>%8</table>")
                         .arg(tr("LinuPaint Help"),
                              tr("LinuPaint is a simple bitmap editor that works like the classic Paint. Pick a "
                                 "tool on the left, a color at the bottom and draw on the picture.")
                                  .toHtmlEscaped(),
                              tr("Tools"), tools, tr("Tips"), tipItems, tr("Keyboard shortcuts"), keyRows));
    layout->addWidget(browser);
    auto* box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(box);
}

} // namespace app
