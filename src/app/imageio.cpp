#include "imageio.h"

#include "qtbridge.h"
#include "raster/codecs.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QSaveFile>

namespace app {

namespace {

SaveFormat bmpDepth(const QByteArray& header)
{
    if (header.size() >= 30 && header.startsWith("BM")) {
        const int bpp = quint8(header[28]) | (quint8(header[29]) << 8);
        if (bpp == 1)
            return SaveFormat::Bmp1;
        if (bpp == 4)
            return SaveFormat::Bmp4;
        if (bpp == 8)
            return SaveFormat::Bmp8;
    }
    return SaveFormat::Bmp24;
}

LoadResult readFrom(QImageReader& reader, const QByteArray& header)
{
    LoadResult r;
    reader.setAutoTransform(true);
    const QByteArray fmt = reader.format().toLower();
    const QImage q = reader.read();
    if (q.isNull()) {
        r.error = reader.errorString();
        return r;
    }
    r.ok = true;
    r.image = fromQImage(q);
    if (fmt == "bmp" || fmt == "dib")
        r.format = bmpDepth(header);
    else if (fmt == "jpeg" || fmt == "jpg")
        r.format = SaveFormat::Jpeg;
    else if (fmt == "gif")
        r.format = SaveFormat::Gif;
    else
        r.format = SaveFormat::Png;
    return r;
}

} // namespace

QList<FormatInfo> saveFormats()
{
    return {
        {SaveFormat::Bmp1, QCoreApplication::translate("app::ImageIO", "Monochrome Bitmap (*.bmp *.dib)"), QStringLiteral("bmp"), true},
        {SaveFormat::Bmp4, QCoreApplication::translate("app::ImageIO", "16 Color Bitmap (*.bmp *.dib)"), QStringLiteral("bmp"), true},
        {SaveFormat::Bmp8, QCoreApplication::translate("app::ImageIO", "256 Color Bitmap (*.bmp *.dib)"), QStringLiteral("bmp"), true},
        {SaveFormat::Bmp24, QCoreApplication::translate("app::ImageIO", "24-bit Bitmap (*.bmp *.dib)"), QStringLiteral("bmp"), false},
        {SaveFormat::Jpeg, QCoreApplication::translate("app::ImageIO", "JPEG (*.jpg *.jpeg *.jpe *.jfif)"), QStringLiteral("jpg"), false},
        {SaveFormat::Gif, QCoreApplication::translate("app::ImageIO", "GIF (*.gif)"), QStringLiteral("gif"), true},
        {SaveFormat::Png, QCoreApplication::translate("app::ImageIO", "PNG (*.png)"), QStringLiteral("png"), false},
    };
}

const FormatInfo& formatInfo(SaveFormat f)
{
    static const QList<FormatInfo> cache = saveFormats();
    for (const FormatInfo& i : cache)
        if (i.format == f)
            return i;
    return cache.last();
}

QString openFilter()
{
    QStringList patterns;
    for (const QByteArray& f : QImageReader::supportedImageFormats())
        patterns << QStringLiteral("*.%1").arg(QString::fromLatin1(f));
    patterns << QStringLiteral("*.dib") << QStringLiteral("*.jpe") << QStringLiteral("*.jfif");
    patterns.removeDuplicates();
    return QCoreApplication::translate("app::ImageIO", "All Picture Files (%1)").arg(patterns.join(QLatin1Char(' '))) + QStringLiteral(";;") +
           QCoreApplication::translate("app::ImageIO", "Bitmap Files (*.bmp *.dib)") + QStringLiteral(";;") + QCoreApplication::translate("app::ImageIO", "JPEG (*.jpg *.jpeg *.jpe *.jfif)") +
           QStringLiteral(";;") + QCoreApplication::translate("app::ImageIO", "GIF (*.gif)") + QStringLiteral(";;") + QCoreApplication::translate("app::ImageIO", "PNG (*.png)") +
           QStringLiteral(";;") + QCoreApplication::translate("app::ImageIO", "All Files (*)");
}

SaveFormat formatForSuffix(const QString& path, SaveFormat fallback)
{
    const QString s = QFileInfo(path).suffix().toLower();
    if (s == QLatin1String("png"))
        return SaveFormat::Png;
    if (s == QLatin1String("jpg") || s == QLatin1String("jpeg") || s == QLatin1String("jpe") ||
        s == QLatin1String("jfif"))
        return SaveFormat::Jpeg;
    if (s == QLatin1String("gif"))
        return SaveFormat::Gif;
    if (s == QLatin1String("bmp") || s == QLatin1String("dib")) {
        const bool fallbackIsBmp = fallback == SaveFormat::Bmp1 || fallback == SaveFormat::Bmp4 ||
                                   fallback == SaveFormat::Bmp8 || fallback == SaveFormat::Bmp24;
        return fallbackIsBmp ? fallback : SaveFormat::Bmp24;
    }
    return fallback;
}

LoadResult loadImage(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        LoadResult r;
        r.error = f.errorString();
        return r;
    }
    const QByteArray header = f.peek(30);
    QImageReader reader(&f);
    return readFrom(reader, header);
}

LoadResult loadImageData(const QByteArray& data)
{
    QBuffer buf;
    buf.setData(data);
    buf.open(QIODevice::ReadOnly);
    QImageReader reader(&buf);
    return readFrom(reader, data.left(30));
}

bool saveImage(const lp::Image& img, const QString& path, SaveFormat format, QString* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    bool ok = true;
    switch (format) {
    case SaveFormat::Bmp1:
    case SaveFormat::Bmp4:
    case SaveFormat::Bmp8:
    case SaveFormat::Bmp24: {
        static constexpr int depth[] = {1, 4, 8, 24};
        const auto bytes = lp::encodeBmp(img, depth[int(format)]);
        ok = file.write(reinterpret_cast<const char*>(bytes.data()), qint64(bytes.size())) == qint64(bytes.size());
        break;
    }
    case SaveFormat::Gif: {
        const auto bytes = lp::encodeGif(img);
        ok = file.write(reinterpret_cast<const char*>(bytes.data()), qint64(bytes.size())) == qint64(bytes.size());
        break;
    }
    case SaveFormat::Jpeg:
    case SaveFormat::Png: {
        QImageWriter writer(&file, format == SaveFormat::Jpeg ? "jpeg" : "png");
        if (format == SaveFormat::Jpeg)
            writer.setQuality(90);
        ok = writer.write(wrap(img).convertToFormat(QImage::Format_RGB32));
        if (!ok && error)
            *error = writer.errorString();
        break;
    }
    }
    if (!ok) {
        file.cancelWriting();
        if (error && error->isEmpty())
            *error = file.errorString();
        return false;
    }
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

} // namespace app
