#include "platform.h"

#include "editor.h"
#include "imageio.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>
#include <QUrl>

#ifdef LINUPAINT_HAVE_DBUS
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace app {

namespace {

QImage composeWallpaper(const QImage& image, bool tiled)
{
    QSize screen(1920, 1080);
    if (QScreen* s = QGuiApplication::primaryScreen())
        screen = s->size() * s->devicePixelRatio();
    QImage out(screen, QImage::Format_RGB32);
    out.fill(Qt::black);
    QPainter p(&out);
    if (tiled) {
        for (int y = 0; y < screen.height(); y += image.height())
            for (int x = 0; x < screen.width(); x += image.width())
                p.drawImage(x, y, image);
    } else {
        p.drawImage((screen.width() - image.width()) / 2, (screen.height() - image.height()) / 2, image);
    }
    return out;
}

#ifdef LINUPAINT_HAVE_DBUS
bool viaPortal(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.portal.Desktop"), QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.Wallpaper"), QStringLiteral("SetWallpaperFile"));
    const QVariantMap options{{QStringLiteral("show-preview"), false},
                              {QStringLiteral("set-on"), QStringLiteral("background")}};
    msg << QString() << QVariant::fromValue(QDBusUnixFileDescriptor(f.handle())) << options;
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 10000);
    return reply.type() == QDBusMessage::ReplyMessage;
}
#endif

} // namespace

bool setWallpaper(const QImage& image, bool tiled, QString* error)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/wallpaper.png");
    if (!composeWallpaper(image, tiled).save(path, "PNG")) {
        if (error)
            *error = QObject::tr("Could not write %1.").arg(path);
        return false;
    }
#if defined(Q_OS_WIN)
    std::wstring w = QDir::toNativeSeparators(path).toStdWString();
    if (SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, w.data(), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE))
        return true;
#elif defined(Q_OS_UNIX)
#ifdef LINUPAINT_HAVE_DBUS
    if (viaPortal(path))
        return true;
#endif
    const QString uri = QUrl::fromLocalFile(path).toString();
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    if (desktop.contains(QLatin1String("KDE"), Qt::CaseInsensitive) &&
        QProcess::execute(QStringLiteral("plasma-apply-wallpaperimage"), {path}) == 0)
        return true;
    if (QProcess::execute(QStringLiteral("gsettings"), {QStringLiteral("set"), QStringLiteral("org.gnome.desktop.background"),
                                                        QStringLiteral("picture-uri"), uri}) == 0) {
        QProcess::execute(QStringLiteral("gsettings"), {QStringLiteral("set"), QStringLiteral("org.gnome.desktop.background"),
                                                        QStringLiteral("picture-uri-dark"), uri});
        return true;
    }
#endif
    if (error)
        *error = QObject::tr("This desktop does not allow LinuPaint to change the background.");
    return false;
}

QString stateDirectory()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QString base = qEnvironmentVariable("XDG_STATE_HOME");
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.local/state");
    return base + QStringLiteral("/linupaint");
#else
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/state");
#endif
}

AutoSave::AutoSave(Editor* editor, QObject* parent) : QObject(parent), editor_(editor)
{
    timer_.setInterval(2 * 60 * 1000);
    connect(&timer_, &QTimer::timeout, this, &AutoSave::saveNow);
    connect(editor_, &Editor::historyChanged, this, [this] { pending_ = true; });
    timer_.start();
}

void AutoSave::setDocumentPath(const QString& path)
{
    path_ = path;
}

void AutoSave::saveNow()
{
    if (!pending_ || !editor_->document().isModified())
        return;
    const QString dir = stateDirectory();
    QDir().mkpath(dir);
    QString error;
    if (!saveImage(editor_->document().image(), dir + QStringLiteral("/autosave.png"), SaveFormat::Png, &error))
        return;
    QJsonObject meta{{QStringLiteral("path"), path_},
                     {QStringLiteral("time"), QDateTime::currentDateTime().toString(Qt::ISODate)}};
    QFile f(dir + QStringLiteral("/autosave.json"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(meta).toJson());
    pending_ = false;
}

void AutoSave::clear()
{
    const QString dir = stateDirectory();
    QFile::remove(dir + QStringLiteral("/autosave.png"));
    QFile::remove(dir + QStringLiteral("/autosave.json"));
    pending_ = false;
}

bool AutoSave::findRecovery(Recovery* out)
{
    const QString dir = stateDirectory();
    if (!QFile::exists(dir + QStringLiteral("/autosave.png")))
        return false;
    const LoadResult r = loadImage(dir + QStringLiteral("/autosave.png"));
    if (!r.ok)
        return false;
    out->image = r.image;
    QFile f(dir + QStringLiteral("/autosave.json"));
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject meta = QJsonDocument::fromJson(f.readAll()).object();
        out->originalPath = meta.value(QStringLiteral("path")).toString();
        out->when = QDateTime::fromString(meta.value(QStringLiteral("time")).toString(), Qt::ISODate);
    }
    return true;
}

} // namespace app
