#pragma once

#include "raster/image.h"

#include <QList>
#include <QString>

namespace app {

enum class SaveFormat { Bmp1, Bmp4, Bmp8, Bmp24, Jpeg, Gif, Png };

struct FormatInfo {
    SaveFormat format;
    QString filter;   // "PNG (*.png)"
    QString suffix;   // default extension
    bool reducesColors;
};

// In the order of the classic "Save as type" list.
QList<FormatInfo> saveFormats();
const FormatInfo& formatInfo(SaveFormat f);
QString openFilter();
SaveFormat formatForSuffix(const QString& path, SaveFormat fallback);

struct LoadResult {
    bool ok = false;
    lp::Image image;
    SaveFormat format = SaveFormat::Png;
    QString error;
};

LoadResult loadImage(const QString& path);
// Also accepts in-memory data (clipboard, drag and drop).
LoadResult loadImageData(const QByteArray& data);
bool saveImage(const lp::Image& img, const QString& path, SaveFormat format, QString* error);

} // namespace app
