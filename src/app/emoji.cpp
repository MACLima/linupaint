#include "emoji.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTabBar>
#include <QStringConverter>
#include <QTextStream>
#include <QVBoxLayout>

#include <optional>

namespace app {

namespace {

const QStringList& emojiFamilies()
{
    // Color emoji fonts of Linux distributions first, then other platforms.
    static const QStringList families = {QStringLiteral("Noto Color Emoji"), QStringLiteral("Twemoji"),
                                         QStringLiteral("JoyPixels"), QStringLiteral("Segoe UI Emoji"),
                                         QStringLiteral("Apple Color Emoji")};
    return families;
}

QFont emojiFont(int pixelSize)
{
    QFont f;
    f.setFamilies(emojiFamilies());
    f.setPixelSize(std::max(1, pixelSize));
    return f;
}

// Translatable names of the Unicode emoji groups, in file order.
const char* const kGroupNames[] = {
    QT_TRANSLATE_NOOP("app::EmojiDialog", "Smileys & Emotion"), QT_TRANSLATE_NOOP("app::EmojiDialog", "People & Body"),
    QT_TRANSLATE_NOOP("app::EmojiDialog", "Animals & Nature"),  QT_TRANSLATE_NOOP("app::EmojiDialog", "Food & Drink"),
    QT_TRANSLATE_NOOP("app::EmojiDialog", "Travel & Places"),   QT_TRANSLATE_NOOP("app::EmojiDialog", "Activities"),
    QT_TRANSLATE_NOOP("app::EmojiDialog", "Objects"),           QT_TRANSLATE_NOOP("app::EmojiDialog", "Symbols"),
    QT_TRANSLATE_NOOP("app::EmojiDialog", "Flags"),
};

QString namesFileSuffix()
{
    const QString lang = QLocale().name();
    if (lang.startsWith(QLatin1String("pt")))
        return QStringLiteral("pt_BR");
    if (lang.startsWith(QLatin1String("es")))
        return QStringLiteral("es");
    return QStringLiteral("en");
}

QHash<QString, QStringList> readNames(const QString& suffix)
{
    QHash<QString, QStringList> names;
    QFile f(QStringLiteral(":/emoji/names_%1.tsv").arg(suffix));
    if (!f.open(QIODevice::ReadOnly))
        return names;
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        const int tab = line.indexOf(QLatin1Char('\t'));
        if (tab > 0)
            names.insert(line.left(tab), line.mid(tab + 1).split(QLatin1Char('|')));
    }
    return names;
}

// True when the installed emoji font has a glyph for every visible code point of the sequence.
bool drawable(const QFontMetrics& fm, const QString& emoji)
{
    for (const char32_t cp : emoji.toUcs4()) {
        const bool invisible = cp == 0x200D || cp == 0xFE0F || cp == 0x20E3 || (cp >= 0xE0020 && cp <= 0xE007F);
        if (!invisible && !fm.inFontUcs4(cp))
            return false;
    }
    return true;
}

enum Role { GroupRole = Qt::UserRole + 1, SearchRole, BaseRole, TonesRole };

class EmojiFilter : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    // group == -1: the "Recent" tab.
    void set(int group, const QString& key, const QStringList& recent)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        beginFilterChange();
#endif
        group_ = group;
        key_ = key;
        recent_ = recent;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override
    {
        const QModelIndex idx = sourceModel()->index(row, 0, parent);
        if (!key_.isEmpty())
            return idx.data(SearchRole).toString().contains(key_);
        if (group_ < 0)
            return recent_.contains(idx.data(BaseRole).toString());
        return idx.data(GroupRole).toInt() == group_;
    }

private:
    int group_ = 0;
    QString key_;
    QStringList recent_;
};

} // namespace

QString searchKey(const QString& text)
{
    QString out;
    const QString decomposed = text.normalized(QString::NormalizationForm_D).toLower();
    out.reserve(decomposed.size());
    for (const QChar c : decomposed)
        if (c.category() != QChar::Mark_NonSpacing)
            out.append(c);
    return out;
}

const EmojiCatalog& EmojiCatalog::instance()
{
    static const EmojiCatalog catalog;
    return catalog;
}

EmojiCatalog::EmojiCatalog()
{
    const QHash<QString, QStringList> local = readNames(namesFileSuffix());
    const QHash<QString, QStringList> english = readNames(QStringLiteral("en"));

    std::optional<QFontMetrics> fm;
    const QStringList installed = QFontDatabase::families();
    for (const QString& family : emojiFamilies())
        if (installed.contains(family)) {
            fm.emplace(QFont(family));
            break;
        }

    QFile f(QStringLiteral(":/emoji/emoji.tsv"));
    if (!f.open(QIODevice::ReadOnly))
        return;
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.startsWith(QLatin1Char('@'))) {
            groups_.append(line.mid(1));
            continue;
        }
        const QStringList cols = line.split(QLatin1Char('\t'));
        if (cols.size() < 4 || groups_.isEmpty())
            continue;
        EmojiEntry e;
        e.emoji = cols[0];
        if (fm && !drawable(*fm, e.emoji))
            continue;
        const QStringList localNames = local.value(e.emoji);
        e.name = localNames.isEmpty() ? cols[2] : localNames.first();
        e.tones = cols[3].split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (e.tones.size() != 5 || (fm && !drawable(*fm, e.tones.first())))
            e.tones.clear();
        e.group = int(groups_.size()) - 1;
        e.searchText = searchKey(localNames.join(QLatin1Char(' ')) + QLatin1Char(' ') + cols[2] + QLatin1Char(' ') +
                                 english.value(e.emoji).join(QLatin1Char(' ')));
        entries_.append(e);
    }
}

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
    setWindowTitle(tr("Insert Emoji"));
    resize(560, 560);
    auto* layout = new QVBoxLayout(this);

    auto* top = new QHBoxLayout;
    search_ = new QLineEdit(this);
    search_->setObjectName(QStringLiteral("emojiSearch"));
    search_->setPlaceholderText(tr("Search emojis"));
    search_->setAccessibleName(tr("Search emojis"));
    search_->setClearButtonEnabled(true);
    top->addWidget(search_, 1);
    tone_ = new QComboBox(this);
    tone_->setAccessibleName(tr("Skin tone"));
    tone_->setFont(emojiFont(18));
    const QString hand = QString::fromUtf8("✋");
    const QStringList toneNames = {tr("Default skin tone"), tr("Light skin tone"), tr("Medium-light skin tone"),
                                   tr("Medium skin tone"), tr("Medium-dark skin tone"), tr("Dark skin tone")};
    for (int i = 0; i < 6; ++i) {
        const char32_t modifier = char32_t(0x1F3FA + i);
        tone_->addItem(i == 0 ? hand : hand + QString::fromUcs4(&modifier, 1));
        tone_->setItemData(i, toneNames[i], Qt::ToolTipRole);
        tone_->setItemData(i, toneNames[i], Qt::AccessibleTextRole);
    }
    tone_->setCurrentIndex(QSettings().value(QStringLiteral("emoji/tone"), 0).toInt());
    top->addWidget(tone_);
    layout->addLayout(top);

    const EmojiCatalog& catalog = EmojiCatalog::instance();
    tabs_ = new QTabBar(this);
    tabs_->setExpanding(false);
    tabs_->setUsesScrollButtons(true);
    hasRecent_ = !recent().isEmpty();
    if (hasRecent_)
        tabs_->addTab(tr("Recent"));
    for (int g = 0; g < catalog.groups().size(); ++g) {
        const char* known = g < int(std::size(kGroupNames)) ? kGroupNames[g] : nullptr;
        QString title = known ? tr(known) : catalog.groups()[g];
        tabs_->addTab(title.replace(QLatin1Char('&'), QStringLiteral("&&"))); // "&" would become a mnemonic
    }
    layout->addWidget(tabs_);

    model_ = new QStandardItemModel(this);
    for (const EmojiEntry& e : catalog.entries()) {
        auto* item = new QStandardItem(e.emoji);
        item->setEditable(false);
        item->setToolTip(e.name);
        item->setData(e.name, Qt::AccessibleTextRole);
        item->setData(e.group, GroupRole);
        item->setData(e.searchText, SearchRole);
        item->setData(e.emoji, BaseRole);
        item->setData(e.tones, TonesRole);
        item->setTextAlignment(Qt::AlignCenter);
        model_->appendRow(item);
    }
    proxy_ = new EmojiFilter(this);
    proxy_->setSourceModel(model_);

    view_ = new QListView(this);
    view_->setModel(proxy_);
    view_->setViewMode(QListView::IconMode);
    view_->setResizeMode(QListView::Adjust);
    view_->setMovement(QListView::Static);
    view_->setUniformItemSizes(true);
    view_->setGridSize(QSize(44, 44));
    view_->setFont(emojiFont(26));
    view_->setAccessibleName(tr("Emojis"));
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(view_, 1);

    auto* form = new QFormLayout;
    custom_ = new QLineEdit(this);
    custom_->setMaxLength(32);
    custom_->setFont(emojiFont(18));
    custom_->setPlaceholderText(tr("Type or paste any emoji"));
    form->addRow(tr("&Other emoji:"), custom_);
    auto* bottom = new QHBoxLayout;
    size_ = new QSpinBox(this);
    size_->setRange(16, 512);
    size_->setValue(QSettings().value(QStringLiteral("emoji/size"), 64).toInt());
    size_->setSuffix(tr(" px"));
    bottom->addWidget(size_);
    bottom->addStretch(1);
    preview_ = new QLabel(this);
    preview_->setFixedSize(72, 72);
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setAccessibleName(tr("Preview"));
    bottom->addWidget(preview_);
    auto* sizeLabel = new QLabel(tr("&Size:"), this);
    sizeLabel->setBuddy(size_);
    form->addRow(sizeLabel, bottom);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    auto refresh = [this, buttons] {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!emoji().isEmpty());
        updatePreview();
    };
    connect(search_, &QLineEdit::textChanged, this, &EmojiDialog::applyFilter);
    connect(tabs_, &QTabBar::currentChanged, this, &EmojiDialog::applyFilter);
    connect(tone_, &QComboBox::currentIndexChanged, this, [this, refresh] {
        applyTone();
        refresh();
    });
    connect(view_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this, refresh] {
        custom_->clear();
        refresh();
    });
    connect(view_, &QListView::doubleClicked, this, &EmojiDialog::accept);
    connect(custom_, &QLineEdit::textChanged, this, refresh);
    connect(size_, &QSpinBox::valueChanged, this, refresh);

    applyTone();
    if (hasRecent_)
        tabs_->setCurrentIndex(0);
    applyFilter();
    search_->setFocus();
    refresh();
}

QStringList EmojiDialog::recent() const
{
    return QSettings().value(QStringLiteral("emoji/recent")).toStringList();
}

void EmojiDialog::applyFilter()
{
    const int tab = tabs_->currentIndex();
    const int group = hasRecent_ ? tab - 1 : tab;
    static_cast<EmojiFilter*>(proxy_)->set(group, searchKey(search_->text().trimmed()), recent());
    if (proxy_->rowCount() > 0 && !view_->currentIndex().isValid())
        view_->setCurrentIndex(proxy_->index(0, 0));
}

void EmojiDialog::applyTone()
{
    const int tone = tone_->currentIndex();
    for (int row = 0; row < model_->rowCount(); ++row) {
        QStandardItem* item = model_->item(row);
        const QStringList tones = item->data(TonesRole).toStringList();
        if (!tones.isEmpty())
            item->setText(tone == 0 ? item->data(BaseRole).toString() : tones.value(tone - 1));
    }
}

QString EmojiDialog::emoji() const
{
    const QString typed = custom_->text().trimmed();
    if (!typed.isEmpty())
        return typed;
    const QModelIndex idx = view_->currentIndex();
    return idx.isValid() ? idx.data(Qt::DisplayRole).toString() : QString();
}

int EmojiDialog::emojiSize() const
{
    return size_->value();
}

void EmojiDialog::accept()
{
    if (emoji().isEmpty())
        return;
    QSettings s;
    const QModelIndex idx = view_->currentIndex();
    if (custom_->text().trimmed().isEmpty() && idx.isValid()) {
        QStringList list = recent();
        const QString base = idx.data(BaseRole).toString();
        list.removeAll(base);
        list.prepend(base);
        while (list.size() > 40)
            list.removeLast();
        s.setValue(QStringLiteral("emoji/recent"), list);
    }
    s.setValue(QStringLiteral("emoji/tone"), tone_->currentIndex());
    s.setValue(QStringLiteral("emoji/size"), size_->value());
    QDialog::accept();
}

void EmojiDialog::updatePreview()
{
    const QString e = emoji();
    if (e.isEmpty()) {
        preview_->clear();
        return;
    }
    preview_->setPixmap(QPixmap::fromImage(renderEmoji(e, std::min(72, emojiSize()), color_)));
}

} // namespace app
