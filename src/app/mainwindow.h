#pragma once

#include "canvas.h"
#include "editor.h"
#include "imageio.h"

#include <QDateTime>
#include <QLockFile>
#include <QMainWindow>

#include <functional>
#include <memory>

class QAction;
class QComboBox;
class QFontComboBox;
class QLabel;
class QPrinter;

namespace app {

class AutoSave;
class ColorBox;
class ThumbnailView;
class ToolBox;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool openFile(const QString& path);
    // Offers to restore the picture autosaved before a crash.
    void offerRecovery();

protected:
    void closeEvent(QCloseEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    void createActions();
    void createMenus();
    void createDocks();
    void createTextToolBar();
    void createStatusBar();
    void readSettings();
    void writeSettings();

    void newFile();
    void open();
    bool save();
    bool saveAs();
    bool saveTo(const QString& path, SaveFormat format);
    bool maybeSave();
    void setCurrentFile(const QString& path, SaveFormat format);
    void updateTitle();
    void addRecentFile(const QString& path);
    void updateRecentFiles();
    void printPreview();
    void pageSetup();
    void print();
    void renderForPrint(QPrinter* printer);
    void setAsBackground(bool tiled);

    void cut();
    void copy();
    void paste();
    void pasteImage(lp::Image img);
    void clearSelection();
    void selectAll();
    void copyTo();
    void pasteFrom();

    void zoomCustom();
    void toggleThumbnail(bool on);
    void viewBitmap();

    void flipRotate();
    void stretchSkew();
    void invertColors();
    void attributes();
    void clearImage();
    void applyImageOp(const std::function<lp::Image(const lp::Image&)>& op,
                      const std::function<lp::Mask(const lp::Mask&)>& maskOp);

    void about();
    void updateActions();
    void updateTextToolBar();
    void applyTextStyle();
    // Commits text being typed; most commands act on a settled picture.
    void settle();

    Editor* editor_;
    Canvas* canvas_;
    ToolBox* toolBox_ = nullptr;
    ColorBox* colorBox_ = nullptr;
    QDockWidget* toolDock_ = nullptr;
    QDockWidget* colorDock_ = nullptr;
    QToolBar* textBar_ = nullptr;
    QFontComboBox* fontCombo_ = nullptr;
    QComboBox* sizeCombo_ = nullptr;
    QLabel* posLabel_ = nullptr;
    QLabel* sizeLabel_ = nullptr;
    AutoSave* autoSave_ = nullptr;
    std::unique_ptr<QLockFile> instanceLock_;
    ThumbnailView* thumbnail_ = nullptr;
    std::unique_ptr<QPrinter> printer_;

    QString path_;
    SaveFormat format_ = SaveFormat::Png;
    QDateTime lastSaved_;
    QSize newImageSize_{640, 480};

    struct {
        QAction *newFile, *open, *save, *saveAs, *printPreview, *pageSetup, *print, *bgTiled, *bgCentered, *exit;
        QAction *undo, *redo, *cut, *copy, *paste, *clearSelection, *selectAll, *copyTo, *pasteFrom;
        QAction *toolBox, *colorBox, *statusBar, *textToolBar, *zoomNormal, *zoomLarge, *zoomCustom, *grid,
            *thumbnail, *viewBitmap;
        QAction *flipRotate, *stretchSkew, *invert, *attributes, *clearImage, *drawOpaque;
        QAction *editColors, *help, *about;
        QAction *bold, *italic, *underline, *smooth;
    } a_{};
    QList<QAction*> recentActions_;
    QAction* recentSeparator_ = nullptr;
};

} // namespace app
