#include "Theme.h"
#include "TrayApp.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QSystemTrayIcon>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    Q_INIT_RESOURCE(jupitr);
    QCoreApplication::setOrganizationName(QStringLiteral("Jupitr"));
    QCoreApplication::setApplicationName(QStringLiteral("jupitr"));
    QCoreApplication::setApplicationVersion(QStringLiteral(JUPITR_VERSION));
    QApplication::setQuitOnLastWindowClosed(false);

    jupitr::Theme::apply(application);
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        qWarning("Jupitr could not detect a desktop system tray; it will continue running.");

    jupitr::TrayApp trayApp;
    return application.exec();
}
