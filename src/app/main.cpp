#include "icons.h"
#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("linupaint"));
    QApplication::setOrganizationName(QStringLiteral("LinuPaint"));
    QApplication::setApplicationVersion(QStringLiteral(LINUPAINT_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral(LINUPAINT_APP_ID));
    QApplication::setWindowIcon(app::appIcon());

    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale(), QStringLiteral("qtbase"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtTranslator);
    QTranslator appTranslator;
    if (appTranslator.load(QLocale(), QStringLiteral("linupaint"), QStringLiteral("_"), QStringLiteral(":/i18n")))
        QApplication::installTranslator(&appTranslator);

    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::translate("main", "A classic Paint for Linux."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"), QApplication::translate("main", "Picture to open."));
    parser.process(app);

    app::MainWindow window;
    window.show();
    const QStringList files = parser.positionalArguments();
    if (!files.isEmpty())
        window.openFile(files.first());
    else
        window.offerRecovery();
    return app.exec();
}
