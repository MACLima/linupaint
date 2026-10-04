#pragma once

#include "raster/image.h"

#include <QDateTime>
#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>

namespace app {

class Editor;

// File > Set As Background (Tiled / Centered). Uses the XDG desktop portal on Linux,
// with GNOME and KDE command-line fallbacks.
bool setWallpaper(const QImage& image, bool tiled, QString* error);

// Directory for crash-recovery data ($XDG_STATE_HOME/linupaint on Linux).
QString stateDirectory();

// Saves the picture every two minutes while it has unsaved changes.
class AutoSave : public QObject {
    Q_OBJECT
public:
    explicit AutoSave(Editor* editor, QObject* parent = nullptr);
    void setDocumentPath(const QString& path);
    // Called after a successful save or a clean exit.
    void clear();
    void saveNow();

    struct Recovery {
        lp::Image image;
        QString originalPath;
        QDateTime when;
    };
    static bool findRecovery(Recovery* out);

private:
    Editor* editor_;
    QTimer timer_;
    QString path_;
    bool pending_ = false;
};

} // namespace app
