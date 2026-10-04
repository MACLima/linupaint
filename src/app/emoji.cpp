#include "emoji.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace app {

namespace {

QFont emojiFont(int pixelSize)
{
    QFont f;
    // Color emoji fonts of Linux distributions first, then other platforms.
    f.setFamilies({QStringLiteral("Noto Color Emoji"), QStringLiteral("Twemoji"), QStringLiteral("JoyPixels"),
                   QStringLiteral("Segoe UI Emoji"), QStringLiteral("Apple Color Emoji")});
    f.setPixelSize(std::max(1, pixelSize));
    return f;
}

struct Category {
    const char* name;
    const char* emojis; // space separated
};

const Category kCategories[] = {
    {QT_TRANSLATE_NOOP("app::EmojiDialog", "Smileys"),
     "😀 😃 😄 😁 😆 😅 😂 🤣 😊 😇 🙂 😉 😍 🥰 😘 😎 🤩 🥳 😏 😢 😭 😡 😱 🤔"},
    {QT_TRANSLATE_NOOP("app::EmojiDialog", "Gestures"), "👍 👎 👏 🙌 👋 🤝 🙏 💪 ✌️ 🤞 👌 👉 👈 👆 👇 ✋"},
    {QT_TRANSLATE_NOOP("app::EmojiDialog", "Symbols"),
     "❤️ 🧡 💛 💚 💙 💜 🖤 🤍 💔 ⭐ 🌟 ✨ ⚡ 🔥 💯 ✅ ❌ ❓ ❗ ⚠️ 🚫 ➡️ ⬅️ 🔴"},
    {QT_TRANSLATE_NOOP("app::EmojiDialog", "Nature"),
     "🐶 🐱 🐭 🐰 🦊 🐻 🐼 🐨 🐯 🦁 🐮 🐷 🐸 🐵 🐔 🐧 🐦 🦋 🐢 🐟 🌸 🌻 🌳 ☀️ 🌙 ⛅ 🌈 ❄️"},
    {QT_TRANSLATE_NOOP("app::EmojiDialog", "Objects"),
     "🍎 🍌 🍓 🍕 🍔 🍟 🍰 🎂 🍫 ☕ 🎁 🎈 🎉 🏆 ⚽ 🎮 📷 💡 📌 📎 ✏️ 📝 🏠 🚗 ✈️ 🚀 ⏰ 🔑"},
};

} // namespace

QImage renderEmoji(const QString& emoji, int size, const QColor& color)
{
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setRenderHint(QPainter::Antialiasing);
    p.setFont(emojiFont(int(size * 0.8)));
    p.setPen(color);
    p.drawText(img.rect(), Qt::AlignCenter, emoji);
    p.end();
    return img.convertToFormat(QImage::Format_ARGB32);
}

EmojiDialog::EmojiDialog(const QColor& color, QWidget* parent) : QDialog(parent), color_(color)
{
    setWindowTitle(tr("Insert Emoji (Extra)"));
    auto* layout = new QVBoxLayout(this);

    auto* note = new QLabel(tr("Extra feature: not part of the classic Paint. The emoji is inserted as a "
                               "selection that you can move and resize."),
                            this);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* tabs = new QTabWidget(this);
    group_ = new QButtonGroup(this);
    group_->setExclusive(true);
    for (const Category& c : kCategories) {
        auto* page = new QWidget(tabs);
        auto* grid = new QGridLayout(page);
        grid->setSpacing(2);
        const QStringList list = QString::fromUtf8(c.emojis).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (int i = 0; i < list.size(); ++i) {
            auto* b = new QToolButton(page);
            b->setText(list[i]);
            b->setFont(emojiFont(24));
            b->setCheckable(true);
            b->setAutoRaise(true);
            b->setMinimumSize(40, 40);
            b->setAccessibleName(tr("Emoji %1").arg(list[i]));
            group_->addButton(b);
            grid->addWidget(b, i / 8, i % 8);
        }
        grid->setRowStretch(grid->rowCount(), 1);
        tabs->addTab(page, tr(c.name));
    }
    layout->addWidget(tabs);

    auto* form = new QFormLayout;
    custom_ = new QLineEdit(this);
    custom_->setMaxLength(16);
    custom_->setFont(emojiFont(18));
    custom_->setPlaceholderText(tr("Type or paste any emoji"));
    form->addRow(tr("&Other emoji:"), custom_);
    size_ = new QSpinBox(this);
    size_->setRange(16, 512);
    size_->setValue(64);
    size_->setSuffix(tr(" px"));
    form->addRow(tr("&Size:"), size_);
    preview_ = new QLabel(this);
    preview_->setFixedSize(96, 96);
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setAccessibleName(tr("Preview"));
    form->addRow(tr("Preview:"), preview_);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    auto refresh = [this, buttons] {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!emoji().isEmpty());
        updatePreview();
    };
    connect(group_, &QButtonGroup::buttonClicked, this, [this, refresh] {
        custom_->clear();
        refresh();
    });
    connect(custom_, &QLineEdit::textChanged, this, refresh);
    connect(size_, &QSpinBox::valueChanged, this, refresh);
    group_->buttons().first()->setChecked(true);
    refresh();
}

QString EmojiDialog::emoji() const
{
    const QString typed = custom_->text().trimmed();
    if (!typed.isEmpty())
        return typed;
    return group_->checkedButton() ? group_->checkedButton()->text() : QString();
}

int EmojiDialog::emojiSize() const
{
    return size_->value();
}

void EmojiDialog::updatePreview()
{
    const QString e = emoji();
    if (e.isEmpty()) {
        preview_->clear();
        return;
    }
    preview_->setPixmap(QPixmap::fromImage(renderEmoji(e, std::min(96, emojiSize()), color_)));
}

} // namespace app
