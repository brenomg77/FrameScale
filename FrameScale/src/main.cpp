#include "MainWindow.h"
#include "ui/Typography.h"
#include "ui/InterfaceTranslator.h"
#include <QApplication>
#include <QFontDatabase>
#include <QIcon>
#include <QLocale>
#include <QSettings>
#ifdef Q_OS_WIN
#include <shobjidl.h>
#include <windows.h>
#endif

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
#ifdef Q_OS_WIN
    SetCurrentProcessExplicitAppUserModelID(L"FrameScale.Desktop");
#endif
    QCoreApplication::setApplicationName("FrameScale");
    QCoreApplication::setApplicationVersion(FRAMESCALE_VERSION);
    QCoreApplication::setOrganizationName("PAP");
    InterfaceTranslator::applyLanguage(QSettings().value("language", "pt-PT").toString());
    application.setWindowIcon(QIcon(":/brand/framescale.ico"));
    Typography::apply();

    MainWindow window;
    window.showMaximized();
    if (argc > 1)
        window.openInputFile(QString::fromLocal8Bit(argv[1]));
    return application.exec();
}