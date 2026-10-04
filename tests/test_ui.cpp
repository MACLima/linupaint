// Drives the real widgets (offscreen in CI). Set LINUPAINT_SCREENSHOT_DIR to keep screenshots.
#include "canvas.h"
#include "editor.h"
#include "imageio.h"
#include "mainwindow.h"
#include "qtbridge.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTranslator>

using namespace app;

namespace {

void sendMouse(QWidget* w, QEvent::Type type, QPoint pos, Qt::MouseButton button, Qt::MouseButtons buttons,
               Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    QMouseEvent e(type, QPointF(pos), w->mapToGlobal(QPointF(pos)), button, buttons, mods);
    QApplication::sendEvent(w, &e);
}

void dragOn(QWidget* w, QPoint a, QPoint b, Qt::MouseButton button = Qt::LeftButton,
            Qt::KeyboardModifiers mods = Qt::NoModifier)
{
    sendMouse(w, QEvent::MouseButtonPress, a, button, button, mods);
    const int steps = 8;
    for (int i = 1; i <= steps; ++i)
        sendMouse(w, QEvent::MouseMove, a + (b - a) * i / steps, Qt::NoButton, button, mods);
    sendMouse(w, QEvent::MouseButtonRelease, b, button, Qt::NoButton, mods);
}

void clickOn(QWidget* w, QPoint p, Qt::MouseButton button = Qt::LeftButton)
{
    sendMouse(w, QEvent::MouseButtonPress, p, button, button);
    sendMouse(w, QEvent::MouseButtonRelease, p, button, Qt::NoButton);
}

int countColor(const lp::Image& img, lp::Rgba c)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            n += img.pixel(x, y) == c;
    return n;
}

} // namespace

class UiTest : public QObject {
    Q_OBJECT

private:
    MainWindow* window_ = nullptr;
    Editor* editor_ = nullptr;
    Canvas* canvas_ = nullptr;
    QWidget* viewport_ = nullptr;

    // Widget position of the center of image pixel (x, y).
    QPoint at(int x, int y) const
    {
        const int z = editor_->zoom();
        return QPoint(4 + x * z + z / 2 - canvas_->horizontalScrollBar()->value(),
                      4 + y * z + z / 2 - canvas_->verticalScrollBar()->value());
    }
    lp::Image& image() { return editor_->document().image(); }
    void screenshot(const QString& name)
    {
        const QString dir = qEnvironmentVariable("LINUPAINT_SCREENSHOT_DIR");
        if (!dir.isEmpty()) {
            QDir().mkpath(dir);
            QTest::qWait(50); // let queued layout/show events run

            window_->grab().save(dir + QLatin1Char('/') + name + QStringLiteral(".png"));
        }
    }

private slots:
    void init()
    {
        window_ = new MainWindow;
        window_->resize(900, 680);
        window_->show();
        QVERIFY(QTest::qWaitForWindowExposed(window_));
        editor_ = window_->findChild<Editor*>();
        canvas_ = window_->findChild<Canvas*>();
        QVERIFY(editor_ && canvas_);
        viewport_ = canvas_->viewport();
        editor_->document().reset(lp::Image(200, 150));
        editor_->setZoom(1);
    }

    void cleanup()
    {
        editor_->document().setModified(false);
        delete window_;
        window_ = nullptr;
    }

    void startsWithPencilAndBlankPicture()
    {
        QCOMPARE(editor_->toolId(), lp::ToolId::Pencil);
        QCOMPARE(countColor(image(), lp::kWhite), 200 * 150);
        QVERIFY(window_->windowTitle().contains(QStringLiteral("LinuPaint")));
        screenshot(QStringLiteral("01-start"));
    }

    void pencilDrawsWithLeftAndRightButtons()
    {
        editor_->setColor(1, lp::rgb(255, 0, 0));
        dragOn(viewport_, at(10, 10), at(60, 10));
        QCOMPARE(image().pixel(30, 10), lp::kBlack);
        dragOn(viewport_, at(10, 20), at(60, 20), Qt::RightButton);
        QCOMPARE(image().pixel(30, 20), lp::rgb(255, 0, 0));
        QVERIFY(editor_->document().isModified());
        QVERIFY(window_->isWindowModified());
    }

    void undoAndRedoShortcuts()
    {
        dragOn(viewport_, at(10, 10), at(60, 10));
        QTest::keyClick(window_, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(image().pixel(30, 10), lp::kWhite);
        QTest::keyClick(window_, Qt::Key_Y, Qt::ControlModifier);
        QCOMPARE(image().pixel(30, 10), lp::kBlack);
    }

    void shapesWithShiftAndFill()
    {
        editor_->setTool(lp::ToolId::Ellipse);
        editor_->setFillStyle(lp::FillStyle::OutlineAndFill);
        editor_->setColor(1, lp::rgb(0, 0, 255));
        dragOn(viewport_, at(20, 20), at(80, 50), Qt::LeftButton, Qt::ShiftModifier);
        // Shift makes a circle: 61x61 box.
        QCOMPARE(image().pixel(50, 20), lp::kBlack);
        QCOMPARE(image().pixel(50, 80), lp::kBlack);
        QCOMPARE(image().pixel(50, 50), lp::rgb(0, 0, 255));
        screenshot(QStringLiteral("02-ellipse"));
    }

    void fillAndPickColor()
    {
        editor_->setColor(0, lp::rgb(0, 128, 0));
        editor_->setTool(lp::ToolId::Fill);
        clickOn(viewport_, at(5, 5));
        QCOMPARE(image().pixel(199, 149), lp::rgb(0, 128, 0));
        editor_->setColor(1, lp::kWhite);
        editor_->setTool(lp::ToolId::PickColor);
        clickOn(viewport_, at(5, 5), Qt::RightButton);
        QCOMPARE(editor_->color(1), lp::rgb(0, 128, 0));
        QCOMPARE(editor_->toolId(), lp::ToolId::Fill); // returns to the previous tool
    }

    void selectionCopyPasteAndMove()
    {
        image().fillRect({10, 10, 20, 20}, lp::rgb(255, 0, 0));
        editor_->setTool(lp::ToolId::RectSelect);
        dragOn(viewport_, at(10, 10), at(29, 29));
        QVERIFY(editor_->selection().hasSelection());
        QCOMPARE(editor_->selection().rect(), (lp::Rect{10, 10, 20, 20}));
        QTest::keyClick(window_, Qt::Key_C, Qt::ControlModifier);
        const QImage clip = QApplication::clipboard()->image();
        QCOMPARE(clip.size(), QSize(20, 20));

        dragOn(viewport_, at(15, 15), at(115, 65));
        editor_->finishPending();
        QCOMPARE(image().pixel(115, 65), lp::rgb(255, 0, 0));
        QCOMPARE(image().pixel(15, 15), lp::kWhite);

        QTest::keyClick(window_, Qt::Key_V, Qt::ControlModifier);
        QVERIFY(editor_->selection().isFloating());
        editor_->finishPending();
        QCOMPARE(image().pixel(5, 5), lp::rgb(255, 0, 0));
        screenshot(QStringLiteral("03-selection"));
    }

    void deleteClearsSelection()
    {
        image().fill(lp::kBlack);
        editor_->setColor(1, lp::kWhite);
        editor_->setTool(lp::ToolId::RectSelect);
        dragOn(viewport_, at(0, 0), at(9, 9));
        QTest::keyClick(window_, Qt::Key_Delete);
        QCOMPARE(image().pixel(5, 5), lp::kWhite);
        QCOMPARE(image().pixel(15, 15), lp::kBlack);
    }

    void textToolTypesAndCommits()
    {
        editor_->setTool(lp::ToolId::Text);
        dragOn(viewport_, at(10, 10), at(150, 60));
        auto* edit = canvas_->findChild<QPlainTextEdit*>();
        QVERIFY(edit);
        QTest::keyClicks(edit, QStringLiteral("Hello"));
        screenshot(QStringLiteral("04-text"));
        editor_->setTool(lp::ToolId::Pencil); // switching tools commits the text
        QVERIFY(!canvas_->isEditingText());
        QVERIFY(countColor(image(), lp::kWhite) < 200 * 150);
        QVERIFY(editor_->document().canUndo());
    }

    void zoomMapsPointerToPixels()
    {
        editor_->setZoom(4);
        canvas_->horizontalScrollBar()->setValue(0);
        canvas_->verticalScrollBar()->setValue(0);
        editor_->setTool(lp::ToolId::Pencil);
        clickOn(viewport_, at(7, 9));
        QCOMPARE(image().pixel(7, 9), lp::kBlack);
        QCOMPARE(countColor(image(), lp::kBlack), 1);
        canvas_->setShowGrid(true);
        screenshot(QStringLiteral("05-zoom-grid"));
    }

    void canvasHandleResizesPicture()
    {
        editor_->setColor(1, lp::rgb(255, 255, 0));
        // Corner handle sits just outside the bottom-right pixel.
        const QPoint corner = at(199, 149) + QPoint(3, 3);
        dragOn(viewport_, corner, corner + QPoint(50, 30));
        QVERIFY(image().width() > 200);
        QVERIFY(image().height() > 150);
        QCOMPARE(image().pixel(image().width() - 1, image().height() - 1), lp::rgb(255, 255, 0));
        QTest::keyClick(window_, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(image().width(), 200);
    }

    void imageMenuOperations()
    {
        image().setPixel(0, 0, lp::kBlack);
        QTest::keyClick(window_, Qt::Key_I, Qt::ControlModifier);
        QCOMPARE(image().pixel(0, 0), lp::kWhite);
        QCOMPARE(image().pixel(5, 5), lp::kBlack);
    }

    void saveAndReopenEveryFormat()
    {
        QTemporaryDir dir;
        image().fillRect({0, 0, 50, 50}, lp::rgb(255, 0, 0));
        const lp::Image original = image();
        const QList<QPair<SaveFormat, QString>> formats = {
            {SaveFormat::Png, QStringLiteral("a.png")},   {SaveFormat::Bmp24, QStringLiteral("a.bmp")},
            {SaveFormat::Bmp8, QStringLiteral("b.bmp")},  {SaveFormat::Bmp4, QStringLiteral("c.bmp")},
            {SaveFormat::Gif, QStringLiteral("a.gif")},   {SaveFormat::Jpeg, QStringLiteral("a.jpg")},
            {SaveFormat::Bmp1, QStringLiteral("d.bmp")},
        };
        for (const auto& [fmt, name] : formats) {
            const QString path = dir.filePath(name);
            QString error;
            QVERIFY2(saveImage(original, path, fmt, &error), qPrintable(error));
            QVERIFY(window_->openFile(path));
            QCOMPARE(image().width(), 200);
            if (fmt != SaveFormat::Jpeg && fmt != SaveFormat::Bmp1)
                QVERIFY(image() == original);
        }
    }

    void largeImageStrokeLatency()
    {
        editor_->document().reset(lp::Image(4000, 4000));
        editor_->setTool(lp::ToolId::Brush);
        canvas_->horizontalScrollBar()->setValue(0);
        canvas_->verticalScrollBar()->setValue(0);
        sendMouse(viewport_, QEvent::MouseButtonPress, at(10, 10), Qt::LeftButton, Qt::LeftButton);
        QElapsedTimer timer;
        timer.start();
        const int moves = 200;
        for (int i = 1; i <= moves; ++i) {
            sendMouse(viewport_, QEvent::MouseMove, at(10 + i * 2, 10 + i), Qt::NoButton, Qt::LeftButton);
            viewport_->repaint(); // include painting in the measurement
        }
        const double perMove = double(timer.nsecsElapsed()) / 1e6 / moves;
        sendMouse(viewport_, QEvent::MouseButtonRelease, at(10 + moves * 2, 10 + moves), Qt::LeftButton,
                  Qt::NoButton);
        qInfo("brush stroke on 4000x4000: %.2f ms per pointer move (PRD target < 16 ms)", perMove);
        QVERIFY(perMove < 16.0);
        QCOMPARE(image().pixel(110, 60), lp::kBlack);
    }

    void portugueseTranslationLoads()
    {
        QTranslator t;
        QVERIFY(t.load(QStringLiteral("linupaint_pt_BR"), QStringLiteral(":/i18n")));
        QCOMPARE(t.translate("app::MainWindow", "&File"), QStringLiteral("&Arquivo"));
        QTranslator es;
        QVERIFY(es.load(QStringLiteral("linupaint_es"), QStringLiteral(":/i18n")));
        QCOMPARE(es.translate("app::MainWindow", "&File"), QStringLiteral("&Archivo"));
    }
};

QTEST_MAIN(UiTest)
#include "test_ui.moc"
