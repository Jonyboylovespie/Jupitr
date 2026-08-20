#include "TrayApp.h"

#include "BellSchedule.h"
#include "PopupWindow.h"
#include "SettingsWindow.h"
#include "Theme.h"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QDateTime>
#include <QMenu>
#include <QScreen>

namespace jupitr {

TrayApp::TrayApp(QObject *parent)
    : QObject(parent), m_config(ScheduleConfig::load()), m_scraper(this), m_tray(Theme::trayIcon(), this)
{
    m_menu = new QMenu;
    auto *quitAction = m_menu->addAction(QStringLiteral("Quit"));
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

    m_tray.setContextMenu(m_menu);
    m_tray.setToolTip(QStringLiteral("Jupitr — School Schedule"));
    connect(&m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            showPopup();
    });
    m_tray.show();

    connect(&m_updateTimer, &QTimer::timeout, this, [this]() {
        const auto today = QDate::currentDate();
        if (today != m_lastCheckedDate)
            requestDayType(today);
        updateTrayTooltip();
    });
    m_updateTimer.setInterval(1000);
    m_updateTimer.start();

    requestDayType(QDate::currentDate());
}

void TrayApp::showPopup()
{
    if (!m_popup) {
        m_popup = new PopupWindow(m_config, [this]() { showSettings(); });
        connect(m_popup, &QObject::destroyed, this, [this]() { m_popup = nullptr; });
    }

    const auto today = QDate::currentDate();
    if (today != m_lastCheckedDate)
        requestDayType(today);

    m_popup->setDayType(m_currentDayType);
    m_popup->refreshData();
    m_popup->adjustSize();
    positionPopup();
    m_popup->show();
    m_popup->raise();
    m_popup->activateWindow();
}

void TrayApp::showSettings()
{
    if (!m_settings) {
        m_settings = new SettingsWindow(m_config);
        connect(m_settings, &QObject::destroyed, this, [this]() { m_settings = nullptr; });
    }
    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
}

void TrayApp::requestDayType(const QDate &date)
{
    if (!date.isValid() || date == m_lastCheckedDate)
        return;

    m_scraper.getDayType(date, [this, date](const QString &dayType) {
        m_lastCheckedDate = date;
        m_currentDayType = dayType.isEmpty() ? QStringLiteral("Unknown") : dayType;
        if (m_popup) {
            m_popup->setDayType(m_currentDayType);
            if (m_popup->isVisible())
                m_popup->refreshData();
        }
        updateTrayTooltip();
    });
}

void TrayApp::updateTrayTooltip()
{
    const auto now = QTime::currentTime();
    const auto blocks = BellSchedule::blocksForDayType(m_currentDayType);
    if (blocks.isEmpty()) {
        m_tray.setToolTip(QStringLiteral("Jupitr — schedule unavailable"));
        return;
    }

    const auto current = BellSchedule::currentBlock(now, m_currentDayType);
    QString tooltip;
    if (current.current.has_value()) {
        const auto letter = BellSchedule::extractDayLetter(m_currentDayType);
        const auto index = BellSchedule::configBlockIndex(current.index, m_currentDayType);
        const auto className = letter.has_value() && index.has_value()
            ? m_config.className(*letter, *index)
            : QString();
        tooltip = QStringLiteral("%1 — %2 left")
            .arg(className.isEmpty() ? current.current->name : className,
                 BellSchedule::formatRemaining(current.remainingSeconds));
    } else if (now < blocks.first().start) {
        tooltip = QStringLiteral("Before school — %1 at %2")
            .arg(blocks.first().name, BellSchedule::formatTime(blocks.first().start));
    } else {
        tooltip = QStringLiteral("School is over");
    }
    m_tray.setToolTip(tooltip);
}

void TrayApp::positionPopup()
{
    if (!m_popup)
        return;

    QScreen *screen = QApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QApplication::primaryScreen();
    if (!screen)
        return;

    const QRect workArea = screen->availableGeometry();
    const QRect trayGeometry = m_tray.geometry();
    QPoint position;
    if (trayGeometry.isValid()) {
        position.setX(qBound(workArea.left(), trayGeometry.center().x() - m_popup->width() / 2,
                             workArea.right() - m_popup->width()));
        position.setY(trayGeometry.top() - m_popup->height() - 8);
        if (position.y() < workArea.top())
            position.setY(trayGeometry.bottom() + 8);
    } else {
        position = QPoint(workArea.right() - m_popup->width() - 12,
                          workArea.bottom() - m_popup->height() - 12);
    }

    position.setX(qBound(workArea.left(), position.x(), workArea.right() - m_popup->width()));
    position.setY(qBound(workArea.top(), position.y(), workArea.bottom() - m_popup->height()));
    m_popup->move(position);
}

} // namespace jupitr
