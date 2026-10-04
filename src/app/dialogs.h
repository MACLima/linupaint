#pragma once

#include <QDateTime>
#include <QDialog>

class QButtonGroup;
class QLabel;
class QRadioButton;
class QSpinBox;
class QDoubleSpinBox;

namespace app {

// Image > Flip/Rotate.
class FlipRotateDialog : public QDialog {
    Q_OBJECT
public:
    enum Action { FlipHorizontal, FlipVertical, Rotate };
    explicit FlipRotateDialog(QWidget* parent = nullptr);
    Action action() const;
    int degrees() const;

private:
    QButtonGroup* action_;
    QButtonGroup* angle_;
};

// Image > Stretch/Skew.
class StretchSkewDialog : public QDialog {
    Q_OBJECT
public:
    explicit StretchSkewDialog(QWidget* parent = nullptr);
    int stretchX() const;
    int stretchY() const;
    int skewX() const;
    int skewY() const;

private:
    QSpinBox* sx_;
    QSpinBox* sy_;
    QSpinBox* kx_;
    QSpinBox* ky_;
};

// Image > Attributes.
class AttributesDialog : public QDialog {
    Q_OBJECT
public:
    struct Info {
        QDateTime lastSaved;
        qint64 sizeOnDisk = -1;
    };
    AttributesDialog(int width, int height, const Info& info, QSize defaultSize, QWidget* parent = nullptr);
    int widthPixels() const;
    int heightPixels() const;
    bool blackAndWhite() const;

private:
    enum Unit { Inches, Centimeters, Pixels };
    void setUnit(Unit u);
    double fromPixels(int px) const;
    int toPixels(double v) const;

    QDoubleSpinBox* width_;
    QDoubleSpinBox* height_;
    QButtonGroup* units_;
    QRadioButton* bw_;
    Unit unit_ = Pixels;
    int pxWidth_;
    int pxHeight_;
    QSize default_;
};

// View > Zoom > Custom.
class CustomZoomDialog : public QDialog {
    Q_OBJECT
public:
    CustomZoomDialog(int current, QWidget* parent = nullptr);
    int zoom() const;

private:
    QButtonGroup* group_;
};

// Help > Help Topics.
class HelpDialog : public QDialog {
    Q_OBJECT
public:
    explicit HelpDialog(QWidget* parent = nullptr);
};

} // namespace app
