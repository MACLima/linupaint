#pragma once

// Extra feature (not in the classic Paint): inserting emojis into the picture.

#include <QColor>
#include <QDialog>
#include <QImage>

class QButtonGroup;
class QLabel;
class QLineEdit;
class QSpinBox;

namespace app {

// Renders an emoji on a transparent square of `size` pixels. Monochrome emoji fonts use `color`.
QImage renderEmoji(const QString& emoji, int size, const QColor& color);

class EmojiDialog : public QDialog {
    Q_OBJECT
public:
    explicit EmojiDialog(const QColor& color, QWidget* parent = nullptr);
    QString emoji() const;
    int emojiSize() const;

private:
    void updatePreview();

    QColor color_;
    QButtonGroup* group_;
    QLineEdit* custom_;
    QSpinBox* size_;
    QLabel* preview_;
};

} // namespace app
