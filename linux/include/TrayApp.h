#pragma once

#include "ScheduleConfig.h"
#include "Scraper.h"

#include <QDate>
#include <QObject>
#include <QPointer>
#include <QSystemTrayIcon>
#include <QTimer>

class QMenu;

namespace jupitr {

class PopupWindow;
class SettingsWindow;

class TrayApp final : public QObject {
    Q_OBJECT

public:
    explicit TrayApp(QObject *parent = nullptr);

private:
    void showPopup();
    void showSettings();
    void requestDayType(const QDate &date);
    void updateTrayTooltip();
    void positionPopup();

    ScheduleConfig m_config;
    Scraper m_scraper;
    QSystemTrayIcon m_tray;
    QMenu *m_menu = nullptr;
    QPointer<PopupWindow> m_popup;
    QPointer<SettingsWindow> m_settings;
    QString m_currentDayType = QStringLiteral("Loading...");
    QDate m_lastCheckedDate;
    QTimer m_updateTimer;
};

} // namespace jupitr
