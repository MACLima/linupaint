#include "mainwindow.h"

#include "colorbox.h"
#include "dialogs.h"
#include "icons.h"
#include "platform.h"
#include "qtbridge.h"
#include "raster/transform.h"
#include "toolbox.h"
#include "views.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QLabel>
#include <QLocale>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPageSetupDialog>
#include <QPainter>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QScreen>
#include <QSettings>
#include <QStatusBar>
#include <QToolBar>

namespace app {

namespace {

constexpr int kMaxRecent = 4;

bool isSelectTool(lp::ToolId id)
{
    return id == lp::ToolId::FreeSelect || id == lp::ToolId::RectSelect;
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowIcon(appIcon());
    setAcceptDrops(true);
    editor_ = new Editor(this);
    canvas_ = new Canvas(editor_, this);
    setCentralWidget(canvas_);

    createActions();
    createMenus();
    createDocks();
    createTextToolBar();
    createStatusBar();

    instanceLock_ = std::make_unique<QLockFile>(stateDirectory() + QStringLiteral("/instance.lock"));
    QDir().mkpath(stateDirectory());
    instanceLock_->setStaleLockTime(0);
    // Only the first running instance autosaves, so instances never overwrite each other's recovery data.
    if (instanceLock_->tryLock(0))
        autoSave_ = new AutoSave(editor_, this);

    connect(editor_, &Editor::historyChanged, this, [this] {
        updateTitle();
        updateActions();
    });
    connect(editor_, &Editor::imageChanged, this, &MainWindow::updateActions);
    connect(editor_, &Editor::toolChanged, this, [this] {
        updateActions();
        updateTextToolBar();
    });
    connect(editor_, &Editor::zoomChanged, this, &MainWindow::updateActions);
    connect(editor_, &Editor::optionsChanged, this,
            [this] { a_.drawOpaque->setChecked(!editor_->options().transparentSelection); });
    connect(editor_, &Editor::sizeHint, this, [this](int w, int h) { sizeLabel_->setText(tr("%1 x %2").arg(w).arg(h)); });
    connect(canvas_, &Canvas::cursorMoved, this, [this](const QPoint& p, bool inside) {
        posLabel_->setText(inside ? tr("%1, %2").arg(p.x()).arg(p.y()) : QString());
    });
    // The clipboard is only queried when the Edit menu opens: asking can block for seconds when the
    // clipboard owner is slow, which must never happen at startup or while drawing.
    connect(editMenu_, &QMenu::aboutToShow, this, [this] {
        const QMimeData* mime = QApplication::clipboard()->mimeData();
        a_.paste->setEnabled(mime && mime->hasImage());
    });
    connect(editMenu_, &QMenu::aboutToHide, this, [this] { a_.paste->setEnabled(true); });

    readSettings();
    editor_->document().reset(lp::Image(newImageSize_.width(), newImageSize_.height(), lp::kWhite));
    setCurrentFile(QString(), SaveFormat::Png);
    updateActions();
    updateTextToolBar();
}

MainWindow::~MainWindow() = default;

void MainWindow::createActions()
{
    auto make = [this](const QString& text, const QString& tip, const QKeySequence& key = {}) {
        auto* act = new QAction(text, this);
        act->setStatusTip(tip);
        if (!key.isEmpty())
            act->setShortcut(key);
        addAction(act);
        return act;
    };

    a_.newFile = make(tr("&New"), tr("Creates a new document."), QKeySequence::New);
    a_.open = make(tr("&Open..."), tr("Opens an existing document."), QKeySequence::Open);
    a_.save = make(tr("&Save"), tr("Saves the active document."), QKeySequence::Save);
    a_.saveAs = make(tr("Save &As..."), tr("Saves the active document with a new name."), QKeySequence::SaveAs);
    a_.printPreview = make(tr("Print Pre&view"), tr("Displays full pages."));
    a_.pageSetup = make(tr("Page Se&tup..."), tr("Changes the page layout."));
    a_.print = make(tr("&Print..."), tr("Prints the active document and sets printing options."), QKeySequence::Print);
    a_.bgTiled = make(tr("Set As Background (Tiled)"), tr("Tiles this bitmap as the desktop background."));
    a_.bgCentered = make(tr("Set As Background (Centered)"), tr("Centers this bitmap as the desktop background."));
    a_.exit = make(tr("E&xit"), tr("Quits LinuPaint."), QKeySequence::Quit);

    a_.undo = make(tr("&Undo"), tr("Undoes the last action."), QKeySequence(tr("Ctrl+Z")));
    a_.redo = make(tr("&Repeat"), tr("Redoes the previously undone action."), QKeySequence(tr("Ctrl+Y")));
    a_.cut = make(tr("Cu&t"), tr("Cuts the selection and puts it on the Clipboard."), QKeySequence::Cut);
    a_.copy = make(tr("&Copy"), tr("Copies the selection and puts it on the Clipboard."), QKeySequence::Copy);
    a_.paste = make(tr("&Paste"), tr("Inserts the contents of the Clipboard."), QKeySequence::Paste);
    a_.clearSelection = make(tr("C&lear Selection"), tr("Deletes the selection."), QKeySequence(Qt::Key_Delete));
    a_.selectAll = make(tr("Select &All"), tr("Selects everything."), QKeySequence::SelectAll);
    a_.copyTo = make(tr("C&opy To..."), tr("Copies the selection to a file."));
    a_.pasteFrom = make(tr("Paste &From..."), tr("Pastes a file into the selection."));

    a_.toolBox = make(tr("&Tool Box"), tr("Shows or hides the tool box."), QKeySequence(tr("Ctrl+T")));
    a_.colorBox = make(tr("&Color Box"), tr("Shows or hides the color box."), QKeySequence(tr("Ctrl+L")));
    a_.statusBar = make(tr("&Status Bar"), tr("Shows or hides the status bar."));
    a_.textToolBar = make(tr("T&ext Toolbar"), tr("Shows or hides the text toolbar."));
    a_.zoomNormal = make(tr("&Normal Size"), tr("Zooms the picture to 100%."), QKeySequence(tr("Ctrl+PgUp")));
    a_.zoomLarge = make(tr("&Large Size"), tr("Zooms the picture to 400%."), QKeySequence(tr("Ctrl+PgDown")));
    a_.zoomCustom = make(tr("C&ustom..."), tr("Zooms the picture."));
    a_.grid = make(tr("Show &Grid"), tr("Shows or hides the grid."), QKeySequence(tr("Ctrl+G")));
    a_.thumbnail = make(tr("Show T&humbnail"), tr("Shows or hides the thumbnail view of the picture."));
    a_.viewBitmap = make(tr("&View Bitmap"), tr("Displays the entire picture."), QKeySequence(tr("Ctrl+F")));

    a_.flipRotate = make(tr("&Flip/Rotate..."), tr("Flips or rotates the picture or a selection."),
                         QKeySequence(tr("Ctrl+R")));
    a_.stretchSkew = make(tr("&Stretch/Skew..."), tr("Stretches or skews the picture or a selection."),
                          QKeySequence(tr("Ctrl+W")));
    a_.invert = make(tr("&Invert Colors"), tr("Inverts the colors of the picture or a selection."),
                     QKeySequence(tr("Ctrl+I")));
    a_.attributes = make(tr("&Attributes..."), tr("Changes the attributes of the picture."), QKeySequence(tr("Ctrl+E")));
    a_.clearImage = make(tr("&Clear Image"), tr("Clears the picture."), QKeySequence(tr("Ctrl+Shift+N")));
    a_.drawOpaque = make(tr("&Draw Opaque"), tr("Makes the current selection either opaque or transparent."));

    a_.editColors = make(tr("&Edit Colors..."), tr("Creates a new color."));
    a_.help = make(tr("&Help Topics"), tr("Displays Help for current task or command."), QKeySequence::HelpContents);
    a_.about = make(tr("&About LinuPaint"), tr("Displays program information, version number, and copyright."));

    for (QAction* act : {a_.toolBox, a_.colorBox, a_.statusBar, a_.textToolBar, a_.grid, a_.thumbnail, a_.drawOpaque})
        act->setCheckable(true);
    a_.statusBar->setChecked(true);
    a_.textToolBar->setChecked(true);
    a_.drawOpaque->setChecked(true);

    connect(a_.newFile, &QAction::triggered, this, &MainWindow::newFile);
    connect(a_.open, &QAction::triggered, this, &MainWindow::open);
    connect(a_.save, &QAction::triggered, this, &MainWindow::save);
    connect(a_.saveAs, &QAction::triggered, this, &MainWindow::saveAs);
    connect(a_.printPreview, &QAction::triggered, this, &MainWindow::printPreview);
    connect(a_.pageSetup, &QAction::triggered, this, &MainWindow::pageSetup);
    connect(a_.print, &QAction::triggered, this, &MainWindow::print);
    connect(a_.bgTiled, &QAction::triggered, this, [this] { setAsBackground(true); });
    connect(a_.bgCentered, &QAction::triggered, this, [this] { setAsBackground(false); });
    connect(a_.exit, &QAction::triggered, this, &QWidget::close);

    connect(a_.undo, &QAction::triggered, this, [this] {
        if (canvas_->isEditingText()) {
            canvas_->cancelText();
            return;
        }
        editor_->finishPending();
        editor_->document().undo();
    });
    connect(a_.redo, &QAction::triggered, this, [this] {
        settle();
        editor_->finishPending();
        editor_->document().redo();
    });
    connect(a_.cut, &QAction::triggered, this, &MainWindow::cut);
    connect(a_.copy, &QAction::triggered, this, &MainWindow::copy);
    connect(a_.paste, &QAction::triggered, this, &MainWindow::paste);
    connect(a_.clearSelection, &QAction::triggered, this, &MainWindow::clearSelection);
    connect(a_.selectAll, &QAction::triggered, this, &MainWindow::selectAll);
    connect(a_.copyTo, &QAction::triggered, this, &MainWindow::copyTo);
    connect(a_.pasteFrom, &QAction::triggered, this, &MainWindow::pasteFrom);

    connect(a_.statusBar, &QAction::toggled, this, [this](bool on) { statusBar()->setVisible(on); });
    connect(a_.textToolBar, &QAction::toggled, this, &MainWindow::updateTextToolBar);
    connect(a_.zoomNormal, &QAction::triggered, this, [this] { editor_->setZoom(1); });
    connect(a_.zoomLarge, &QAction::triggered, this, [this] { editor_->setZoom(4); });
    connect(a_.zoomCustom, &QAction::triggered, this, &MainWindow::zoomCustom);
    connect(a_.grid, &QAction::toggled, canvas_, &Canvas::setShowGrid);
    connect(a_.thumbnail, &QAction::toggled, this, &MainWindow::toggleThumbnail);
    connect(a_.viewBitmap, &QAction::triggered, this, &MainWindow::viewBitmap);

    connect(a_.flipRotate, &QAction::triggered, this, &MainWindow::flipRotate);
    connect(a_.stretchSkew, &QAction::triggered, this, &MainWindow::stretchSkew);
    connect(a_.invert, &QAction::triggered, this, &MainWindow::invertColors);
    connect(a_.attributes, &QAction::triggered, this, &MainWindow::attributes);
    connect(a_.clearImage, &QAction::triggered, this, &MainWindow::clearImage);
    connect(a_.drawOpaque, &QAction::triggered, this,
            [this](bool opaque) { editor_->setTransparentSelection(!opaque); });

    connect(a_.editColors, &QAction::triggered, this, [this] { colorBox_->editPrimaryColor(); });
    connect(a_.help, &QAction::triggered, this, [this] {
        HelpDialog dlg(this);
        dlg.exec();
    });
    connect(a_.about, &QAction::triggered, this, &MainWindow::about);

    for (int i = 0; i < kMaxRecent; ++i) {
        auto* act = new QAction(this);
        act->setVisible(false);
        connect(act, &QAction::triggered, this, [this, act] {
            if (maybeSave())
                openFile(act->data().toString());
        });
        recentActions_.append(act);
    }
}

void MainWindow::createMenus()
{
    QMenu* file = menuBar()->addMenu(tr("&File"));
    file->addActions({a_.newFile, a_.open, a_.save, a_.saveAs});
    file->addSeparator();
    file->addActions({a_.printPreview, a_.pageSetup, a_.print});
    file->addSeparator();
    file->addActions({a_.bgTiled, a_.bgCentered});
    recentSeparator_ = file->addSeparator();
    file->addActions(recentActions_);
    file->addSeparator();
    file->addAction(a_.exit);

    QMenu* edit = menuBar()->addMenu(tr("&Edit"));
    editMenu_ = edit;
    edit->addActions({a_.undo, a_.redo});
    edit->addSeparator();
    edit->addActions({a_.cut, a_.copy, a_.paste, a_.clearSelection, a_.selectAll});
    edit->addSeparator();
    edit->addActions({a_.copyTo, a_.pasteFrom});

    QMenu* view = menuBar()->addMenu(tr("&View"));
    view->addActions({a_.toolBox, a_.colorBox, a_.statusBar, a_.textToolBar});
    view->addSeparator();
    QMenu* zoom = view->addMenu(tr("&Zoom"));
    zoom->addActions({a_.zoomNormal, a_.zoomLarge, a_.zoomCustom});
    zoom->addSeparator();
    zoom->addActions({a_.grid, a_.thumbnail});
    view->addAction(a_.viewBitmap);

    QMenu* image = menuBar()->addMenu(tr("&Image"));
    image->addActions({a_.flipRotate, a_.stretchSkew, a_.invert, a_.attributes, a_.clearImage, a_.drawOpaque});

    QMenu* colors = menuBar()->addMenu(tr("&Colors"));
    colors->addAction(a_.editColors);

    QMenu* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(a_.help);
    help->addSeparator();
    help->addAction(a_.about);
}

void MainWindow::createDocks()
{
    toolBox_ = new ToolBox(editor_, this);
    toolDock_ = new QDockWidget(tr("Tools"), this);
    toolDock_->setObjectName(QStringLiteral("toolDock"));
    toolDock_->setWidget(toolBox_);
    toolDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                           QDockWidget::DockWidgetClosable);
    toolDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, toolDock_);

    colorBox_ = new ColorBox(editor_, this);
    colorDock_ = new QDockWidget(tr("Colors"), this);
    colorDock_->setObjectName(QStringLiteral("colorDock"));
    colorDock_->setWidget(colorBox_);
    colorDock_->setFeatures(toolDock_->features());
    colorDock_->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, colorDock_);

    // No title bars: the boxes look like part of the window, as in the classic layout.
    toolDock_->setTitleBarWidget(new QWidget(toolDock_));
    colorDock_->setTitleBarWidget(new QWidget(colorDock_));

    // The tool box keeps the full height, as in the classic layout.
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);

    connect(a_.toolBox, &QAction::toggled, toolDock_, &QDockWidget::setVisible);
    connect(toolDock_, &QDockWidget::visibilityChanged, this, [this](bool) {
        a_.toolBox->setChecked(!toolDock_->isHidden());
    });
    connect(a_.colorBox, &QAction::toggled, colorDock_, &QDockWidget::setVisible);
    connect(colorDock_, &QDockWidget::visibilityChanged, this, [this](bool) {
        a_.colorBox->setChecked(!colorDock_->isHidden());
    });
    a_.toolBox->setChecked(true);
    a_.colorBox->setChecked(true);
}

void MainWindow::createTextToolBar()
{
    textBar_ = new QToolBar(tr("Fonts"), this);
    textBar_->setObjectName(QStringLiteral("textToolBar"));
    fontCombo_ = new QFontComboBox(textBar_);
    fontCombo_->setAccessibleName(tr("Font"));
    sizeCombo_ = new QComboBox(textBar_);
    sizeCombo_->setEditable(true);
    sizeCombo_->setAccessibleName(tr("Font size"));
    for (int s : {8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72})
        sizeCombo_->addItem(QString::number(s));
    sizeCombo_->setCurrentText(QStringLiteral("12"));
    textBar_->addWidget(fontCombo_);
    textBar_->addWidget(sizeCombo_);
    a_.bold = textBar_->addAction(tr("B"));
    a_.italic = textBar_->addAction(tr("I"));
    a_.underline = textBar_->addAction(tr("U"));
    a_.smooth = textBar_->addAction(tr("Smooth"));
    a_.bold->setToolTip(tr("Bold"));
    a_.italic->setToolTip(tr("Italic"));
    a_.underline->setToolTip(tr("Underline"));
    a_.smooth->setToolTip(tr("Smooth edges (antialiasing)"));
    QFont f;
    f.setBold(true);
    a_.bold->setFont(f);
    f.setBold(false);
    f.setItalic(true);
    a_.italic->setFont(f);
    f.setItalic(false);
    f.setUnderline(true);
    a_.underline->setFont(f);
    for (QAction* act : {a_.bold, a_.italic, a_.underline, a_.smooth}) {
        act->setCheckable(true);
        connect(act, &QAction::toggled, this, &MainWindow::applyTextStyle);
    }
    a_.smooth->setChecked(true);
    connect(fontCombo_, &QFontComboBox::currentFontChanged, this, &MainWindow::applyTextStyle);
    connect(sizeCombo_, &QComboBox::currentTextChanged, this, &MainWindow::applyTextStyle);
    addToolBar(Qt::TopToolBarArea, textBar_);
    textBar_->hide();
}

void MainWindow::createStatusBar()
{
    posLabel_ = new QLabel(this);
    sizeLabel_ = new QLabel(this);
    posLabel_->setMinimumWidth(90);
    sizeLabel_->setMinimumWidth(90);
    posLabel_->setAccessibleName(tr("Pointer position"));
    sizeLabel_->setAccessibleName(tr("Size"));
    statusBar()->addPermanentWidget(posLabel_);
    statusBar()->addPermanentWidget(sizeLabel_);
    statusBar()->showMessage(tr("For Help, click Help Topics on the Help Menu."));
}

void MainWindow::applyTextStyle()
{
    TextStyle s = canvas_->textStyle();
    s.family = fontCombo_->currentFont().family();
    bool ok = false;
    const int size = sizeCombo_->currentText().toInt(&ok);
    if (ok && size > 0 && size <= 500)
        s.pointSize = size;
    s.bold = a_.bold->isChecked();
    s.italic = a_.italic->isChecked();
    s.underline = a_.underline->isChecked();
    s.smooth = a_.smooth->isChecked();
    canvas_->setTextStyle(s);
}

void MainWindow::updateTextToolBar()
{
    textBar_->setVisible(editor_->toolId() == lp::ToolId::Text && a_.textToolBar->isChecked());
}

void MainWindow::readSettings()
{
    QSettings s;
    restoreGeometry(s.value(QStringLiteral("window/geometry")).toByteArray());
    restoreState(s.value(QStringLiteral("window/state")).toByteArray());
    if (!s.contains(QStringLiteral("window/geometry")))
        resize(900, 680);
    const QSize size = s.value(QStringLiteral("image/newSize"), QSize(640, 480)).toSize();
    if (size.isValid())
        newImageSize_ = size;
    a_.grid->setChecked(s.value(QStringLiteral("view/grid"), false).toBool());
    a_.statusBar->setChecked(s.value(QStringLiteral("view/statusBar"), true).toBool());
    a_.textToolBar->setChecked(s.value(QStringLiteral("view/textToolBar"), true).toBool());
    const QString family = s.value(QStringLiteral("text/family")).toString();
    if (!family.isEmpty())
        fontCombo_->setCurrentFont(QFont(family));
    sizeCombo_->setCurrentText(s.value(QStringLiteral("text/size"), 12).toString());
    applyTextStyle();
    updateRecentFiles();
    textBar_->hide();
}

void MainWindow::writeSettings()
{
    QSettings s;
    s.setValue(QStringLiteral("window/geometry"), saveGeometry());
    s.setValue(QStringLiteral("window/state"), saveState());
    s.setValue(QStringLiteral("image/newSize"), newImageSize_);
    s.setValue(QStringLiteral("view/grid"), a_.grid->isChecked());
    s.setValue(QStringLiteral("view/statusBar"), a_.statusBar->isChecked());
    s.setValue(QStringLiteral("view/textToolBar"), a_.textToolBar->isChecked());
    s.setValue(QStringLiteral("text/family"), canvas_->textStyle().family);
    s.setValue(QStringLiteral("text/size"), canvas_->textStyle().pointSize);
}

void MainWindow::settle()
{
    canvas_->commitText();
}

void MainWindow::updateTitle()
{
    const QString name = path_.isEmpty() ? tr("untitled") : QFileInfo(path_).fileName();
    setWindowTitle(tr("%1[*] - LinuPaint").arg(name));
    setWindowModified(editor_->document().isModified());
}

void MainWindow::updateActions()
{
    const lp::SelectionController& sel = editor_->selection();
    const bool hasSel = sel.hasSelection();
    a_.undo->setEnabled(editor_->document().canUndo() || editor_->document().isEditing() || canvas_->isEditingText());
    a_.redo->setEnabled(editor_->document().canRedo());
    a_.cut->setEnabled(hasSel);
    a_.copy->setEnabled(hasSel);
    a_.clearSelection->setEnabled(hasSel);
    a_.copyTo->setEnabled(hasSel);
    a_.grid->setEnabled(editor_->zoom() >= 4);
    a_.thumbnail->setEnabled(editor_->zoom() > 1);
    if (editor_->zoom() == 1 && thumbnail_)
        a_.thumbnail->setChecked(false);
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    settle();
    if (!maybeSave()) {
        e->ignore();
        return;
    }
    writeSettings();
    if (autoSave_)
        autoSave_->clear();
    if (thumbnail_)
        thumbnail_->close();
    e->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasUrls() || e->mimeData()->hasImage())
        e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e)
{
    const QMimeData* mime = e->mimeData();
    if (mime->hasUrls()) {
        for (const QUrl& url : mime->urls()) {
            if (url.isLocalFile()) {
                if (maybeSave())
                    openFile(url.toLocalFile());
                e->acceptProposedAction();
                return;
            }
        }
    }
    if (mime->hasImage()) {
        pasteImage(fromQImage(qvariant_cast<QImage>(mime->imageData()), editor_->color(1)));
        e->acceptProposedAction();
    }
}

bool MainWindow::maybeSave()
{
    settle();
    if (!editor_->document().isModified())
        return true;
    const QString name = path_.isEmpty() ? tr("untitled") : QFileInfo(path_).fileName();
    const auto answer = QMessageBox::question(this, tr("LinuPaint"), tr("Save changes to %1?").arg(name),
                                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                              QMessageBox::Save);
    if (answer == QMessageBox::Save)
        return save();
    return answer == QMessageBox::Discard;
}

void MainWindow::setCurrentFile(const QString& path, SaveFormat format)
{
    path_ = path;
    format_ = format;
    lastSaved_ = path.isEmpty() ? QDateTime() : QFileInfo(path).lastModified();
    if (autoSave_)
        autoSave_->setDocumentPath(path);
    updateTitle();
}

void MainWindow::newFile()
{
    if (!maybeSave())
        return;
    editor_->finishPending();
    editor_->document().reset(lp::Image(newImageSize_.width(), newImageSize_.height(), lp::kWhite));
    editor_->setZoom(1);
    setCurrentFile(QString(), SaveFormat::Png);
    if (autoSave_)
        autoSave_->clear();
}

void MainWindow::open()
{
    if (!maybeSave())
        return;
    QSettings s;
    const QString dir = s.value(QStringLiteral("files/lastDir"), QDir::homePath()).toString();
    const QString path = QFileDialog::getOpenFileName(this, tr("Open"), dir, openFilter());
    if (!path.isEmpty())
        openFile(path);
}

bool MainWindow::openFile(const QString& path)
{
    const LoadResult r = loadImage(path);
    if (!r.ok) {
        QMessageBox::warning(this, tr("LinuPaint"),
                             tr("LinuPaint cannot read %1.\nThis is not a valid picture file, or its format is not "
                                "currently supported.")
                                 .arg(QDir::toNativeSeparators(path)));
        return false;
    }
    editor_->finishPending();
    editor_->document().reset(r.image);
    editor_->setZoom(1);
    setCurrentFile(path, r.format);
    addRecentFile(path);
    QSettings().setValue(QStringLiteral("files/lastDir"), QFileInfo(path).absolutePath());
    if (autoSave_)
        autoSave_->clear();
    return true;
}

bool MainWindow::save()
{
    settle();
    if (path_.isEmpty())
        return saveAs();
    return saveTo(path_, format_);
}

bool MainWindow::saveAs()
{
    settle();
    const QList<FormatInfo> formats = saveFormats();
    QStringList filters;
    QString selected;
    for (const FormatInfo& f : formats) {
        filters << f.filter;
        if (f.format == format_)
            selected = f.filter;
    }
    QSettings s;
    const QString dir = path_.isEmpty() ? s.value(QStringLiteral("files/lastDir"), QDir::homePath()).toString()
                                        : QFileInfo(path_).absolutePath();
    const QString base = path_.isEmpty() ? tr("untitled") : QFileInfo(path_).completeBaseName();
    QString path = QFileDialog::getSaveFileName(this, tr("Save As"),
                                                dir + QLatin1Char('/') + base + QLatin1Char('.') +
                                                    formatInfo(format_).suffix,
                                                filters.join(QStringLiteral(";;")), &selected);
    if (path.isEmpty())
        return false;
    SaveFormat fmt = format_;
    for (const FormatInfo& f : formats)
        if (f.filter == selected)
            fmt = f.format;
    if (QFileInfo(path).suffix().isEmpty())
        path += QLatin1Char('.') + formatInfo(fmt).suffix;
    else if (selected.isEmpty())
        fmt = formatForSuffix(path, fmt);
    if (formatInfo(fmt).reducesColors &&
        QMessageBox::warning(this, tr("LinuPaint"),
                             tr("Saving into this format may cause some loss of color information.\nDo you want to "
                                "continue?"),
                             QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return false;
    s.setValue(QStringLiteral("files/lastDir"), QFileInfo(path).absolutePath());
    return saveTo(path, fmt);
}

bool MainWindow::saveTo(const QString& path, SaveFormat format)
{
    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = saveImage(editor_->document().image(), path, format, &error);
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, tr("LinuPaint"),
                             tr("Could not save %1.\n%2").arg(QDir::toNativeSeparators(path), error));
        return false;
    }
    editor_->document().setModified(false);
    setCurrentFile(path, format);
    addRecentFile(path);
    if (autoSave_)
        autoSave_->clear();
    statusBar()->showMessage(tr("Saved %1").arg(QFileInfo(path).fileName()), 3000);
    return true;
}

void MainWindow::addRecentFile(const QString& path)
{
    QSettings s;
    QStringList files = s.value(QStringLiteral("files/recent")).toStringList();
    files.removeAll(path);
    files.prepend(path);
    while (files.size() > kMaxRecent)
        files.removeLast();
    s.setValue(QStringLiteral("files/recent"), files);
    updateRecentFiles();
}

void MainWindow::updateRecentFiles()
{
    const QStringList files = QSettings().value(QStringLiteral("files/recent")).toStringList();
    for (int i = 0; i < kMaxRecent; ++i) {
        QAction* act = recentActions_[i];
        if (i < files.size()) {
            act->setText(QStringLiteral("&%1 %2").arg(i + 1).arg(QFileInfo(files[i]).fileName()));
            act->setStatusTip(QDir::toNativeSeparators(files[i]));
            act->setData(files[i]);
            act->setVisible(true);
        } else {
            act->setVisible(false);
        }
    }
    if (recentSeparator_)
        recentSeparator_->setVisible(!files.isEmpty());
}

void MainWindow::renderForPrint(QPrinter* printer)
{
    QPainter p(printer);
    const QImage img = toQImage(editor_->document().image());
    const QRect page = printer->pageLayout().paintRectPixels(printer->resolution());
    // Pictures print at their real size (96 dpi), shrunk only when they do not fit the page.
    const double scale = printer->resolution() / 96.0;
    QSizeF target(img.width() * scale, img.height() * scale);
    if (target.width() > page.width() || target.height() > page.height())
        target.scale(page.size(), Qt::KeepAspectRatio);
    p.drawImage(QRectF(QPointF(0, 0), target), img);
}

QPrinter* MainWindow::printer()
{
    // Created on first use: querying the system printers can take seconds.
    if (!printer_)
        printer_ = std::make_unique<QPrinter>(QPrinter::HighResolution);
    return printer_.get();
}

void MainWindow::printPreview()
{
    settle();
    QPrintPreviewDialog dlg(printer(), this);
    connect(&dlg, &QPrintPreviewDialog::paintRequested, this, &MainWindow::renderForPrint);
    dlg.exec();
}

void MainWindow::pageSetup()
{
    QPageSetupDialog dlg(printer(), this);
    dlg.exec();
}

void MainWindow::print()
{
    settle();
    QPrintDialog dlg(printer(), this);
    if (dlg.exec() == QDialog::Accepted)
        renderForPrint(printer());
}

void MainWindow::setAsBackground(bool tiled)
{
    settle();
    QString error;
    if (!setWallpaper(toQImage(editor_->document().image()), tiled, &error))
        QMessageBox::warning(this, tr("LinuPaint"), error);
}

void MainWindow::copy()
{
    settle();
    const lp::SelectionController& sel = editor_->selection();
    if (!sel.hasSelection())
        return;
    QApplication::clipboard()->setImage(toQImage(sel.extract(editor_->color(1))));
}

void MainWindow::cut()
{
    copy();
    clearSelection();
}

void MainWindow::clearSelection()
{
    settle();
    editor_->selection().deleteContents(editor_->color(1));
    editor_->repaint({});
}

void MainWindow::selectAll()
{
    settle();
    if (!isSelectTool(editor_->toolId()))
        editor_->setTool(lp::ToolId::RectSelect);
    editor_->selection().selectAll();
    editor_->repaint({});
}

void MainWindow::paste()
{
    settle();
    const QMimeData* mime = QApplication::clipboard()->mimeData();
    if (!mime || !mime->hasImage())
        return;
    const QImage q = qvariant_cast<QImage>(mime->imageData());
    if (q.isNull())
        return;
    pasteImage(fromQImage(q, editor_->color(1)));
}

void MainWindow::pasteImage(lp::Image img)
{
    if (img.isNull())
        return;
    editor_->finishPending();
    lp::Document& doc = editor_->document();
    if (img.width() > doc.image().width() || img.height() > doc.image().height()) {
        const auto answer = QMessageBox::question(
            this, tr("LinuPaint"),
            tr("The image in the clipboard is larger than the bitmap.\nWould you like the bitmap enlarged?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
        if (answer == QMessageBox::Cancel)
            return;
        if (answer == QMessageBox::Yes)
            doc.replaceImage(lp::resizeCanvas(doc.image(), std::max(img.width(), doc.image().width()),
                                              std::max(img.height(), doc.image().height()), editor_->color(1)));
    }
    if (!isSelectTool(editor_->toolId()))
        editor_->setTool(lp::ToolId::RectSelect);
    lp::SelectionController& sel = editor_->selection();
    sel.setTransparency(editor_->options().transparentSelection, editor_->color(1));
    sel.paste(std::move(img), toPoint(canvas_->visibleImageOrigin()));
    editor_->repaint({});
    updateActions();
}

void MainWindow::copyTo()
{
    settle();
    const lp::SelectionController& sel = editor_->selection();
    if (!sel.hasSelection())
        return;
    QStringList filters;
    for (const FormatInfo& f : saveFormats())
        filters << f.filter;
    QString selected = formatInfo(SaveFormat::Png).filter;
    QString path = QFileDialog::getSaveFileName(this, tr("Copy To"), QDir::homePath(),
                                                filters.join(QStringLiteral(";;")), &selected);
    if (path.isEmpty())
        return;
    SaveFormat fmt = SaveFormat::Png;
    for (const FormatInfo& f : saveFormats())
        if (f.filter == selected)
            fmt = f.format;
    if (QFileInfo(path).suffix().isEmpty())
        path += QLatin1Char('.') + formatInfo(fmt).suffix;
    QString error;
    if (!saveImage(sel.extract(editor_->color(1)), path, fmt, &error))
        QMessageBox::warning(this, tr("LinuPaint"), tr("Could not save %1.\n%2").arg(path, error));
}

void MainWindow::pasteFrom()
{
    settle();
    const QString path = QFileDialog::getOpenFileName(this, tr("Paste From"), QDir::homePath(), openFilter());
    if (path.isEmpty())
        return;
    const LoadResult r = loadImage(path);
    if (!r.ok) {
        QMessageBox::warning(this, tr("LinuPaint"), tr("LinuPaint cannot read %1.").arg(path));
        return;
    }
    pasteImage(r.image);
}

void MainWindow::zoomCustom()
{
    CustomZoomDialog dlg(editor_->zoom(), this);
    if (dlg.exec() == QDialog::Accepted)
        editor_->setZoom(dlg.zoom());
}

void MainWindow::toggleThumbnail(bool on)
{
    if (on && !thumbnail_) {
        thumbnail_ = new ThumbnailView(editor_, canvas_, this);
        thumbnail_->setAttribute(Qt::WA_DeleteOnClose);
        connect(thumbnail_, &ThumbnailView::closed, this, [this] {
            thumbnail_ = nullptr;
            a_.thumbnail->setChecked(false);
        });
        thumbnail_->move(mapToGlobal(QPoint(width() - 240, 80)));
        thumbnail_->show();
    } else if (!on && thumbnail_) {
        thumbnail_->close();
    }
}

void MainWindow::viewBitmap()
{
    settle();
    auto* view = new FullScreenView(toQImage(editor_->document().image()), this);
    view->showFullScreen();
}

void MainWindow::applyImageOp(const std::function<lp::Image(const lp::Image&)>& op,
                              const std::function<lp::Mask(const lp::Mask&)>& maskOp)
{
    settle();
    if (lp::Tool* t = editor_->tool())
        t->finish(*editor_);
    lp::SelectionController& sel = editor_->selection();
    if (sel.hasSelection()) {
        sel.setTransparency(editor_->options().transparentSelection, editor_->color(1));
        sel.transform(op, maskOp, editor_->color(1));
    } else {
        lp::Document& doc = editor_->document();
        doc.replaceImage(op(doc.image()));
    }
    editor_->repaint({});
}

void MainWindow::flipRotate()
{
    FlipRotateDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    switch (dlg.action()) {
    case FlipRotateDialog::FlipHorizontal:
        applyImageOp([](const lp::Image& i) { return lp::flipHorizontal(i); },
                     [](const lp::Mask& m) { return lp::flipHorizontal(m); });
        break;
    case FlipRotateDialog::FlipVertical:
        applyImageOp([](const lp::Image& i) { return lp::flipVertical(i); },
                     [](const lp::Mask& m) { return lp::flipVertical(m); });
        break;
    case FlipRotateDialog::Rotate: {
        const int deg = dlg.degrees();
        applyImageOp([deg](const lp::Image& i) { return lp::rotate(i, deg); },
                     [deg](const lp::Mask& m) { return lp::rotate(m, deg); });
        break;
    }
    }
}

void MainWindow::stretchSkew()
{
    StretchSkewDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const int sx = dlg.stretchX(), sy = dlg.stretchY(), kx = dlg.skewX(), ky = dlg.skewY();
    const lp::Rgba bg = editor_->color(1);
    applyImageOp(
        [=](const lp::Image& i) {
            lp::Image out = (sx == 100 && sy == 100) ? i : lp::stretch(i, sx, sy);
            if (kx != 0 || ky != 0)
                out = lp::skew(out, kx, ky, bg);
            return out;
        },
        nullptr);
}

void MainWindow::invertColors()
{
    applyImageOp([](const lp::Image& i) { return lp::invertColors(i); }, [](const lp::Mask& m) { return m; });
}

void MainWindow::attributes()
{
    settle();
    editor_->finishPending();
    lp::Document& doc = editor_->document();
    AttributesDialog::Info info;
    if (!path_.isEmpty()) {
        const QFileInfo fi(path_);
        info.lastSaved = fi.lastModified();
        info.sizeOnDisk = fi.size();
    }
    AttributesDialog dlg(doc.image().width(), doc.image().height(), info, QSize(640, 480), this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const int w = dlg.widthPixels(), h = dlg.heightPixels();
    lp::Image img = doc.image();
    bool changed = false;
    if (w != img.width() || h != img.height()) {
        img = lp::resizeCanvas(img, w, h, editor_->color(1));
        changed = true;
    }
    if (dlg.blackAndWhite()) {
        if (QMessageBox::warning(this, tr("LinuPaint"),
                                 tr("Converting to black and white is permanent and cannot be undone after saving.\n"
                                    "Do you want to continue?"),
                                 QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            img = lp::toMonochrome(img);
            changed = true;
        }
    }
    if (changed)
        doc.replaceImage(std::move(img));
    newImageSize_ = QSize(w, h);
}

void MainWindow::clearImage()
{
    settle();
    editor_->finishPending();
    lp::Document& doc = editor_->document();
    doc.beginEdit();
    doc.image().fill(editor_->color(1));
    doc.addDirty(doc.image().bounds());
    doc.commitEdit();
    editor_->repaint({});
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("About LinuPaint"),
                       tr("<h3>LinuPaint %1</h3><p>A classic Paint for Linux.</p>"
                          "<p>Copyright © 2026 Marco Lima and contributors.</p>"
                          "<p>This program comes with ABSOLUTELY NO WARRANTY. It is free software: you can "
                          "redistribute it and/or modify it under the terms of the "
                          "<a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">GNU General Public License</a>, "
                          "version 3 or later.</p>"
                          "<p>LinuPaint is not affiliated with Microsoft.</p>")
                           .arg(QApplication::applicationVersion()));
}

void MainWindow::offerRecovery()
{
    if (!autoSave_)
        return;
    AutoSave::Recovery rec;
    if (!AutoSave::findRecovery(&rec))
        return;
    const QString when = rec.when.isValid() ? QLocale().toString(rec.when, QLocale::ShortFormat) : tr("an unknown time");
    const auto answer =
        QMessageBox::question(this, tr("LinuPaint"),
                              tr("LinuPaint was not closed properly.\nDo you want to recover the picture saved at %1?")
                                  .arg(when),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer == QMessageBox::Yes) {
        editor_->document().reset(rec.image);
        editor_->document().setModified(true);
        setCurrentFile(rec.originalPath, formatForSuffix(rec.originalPath, SaveFormat::Png));
    } else {
        autoSave_->clear();
    }
}

} // namespace app
