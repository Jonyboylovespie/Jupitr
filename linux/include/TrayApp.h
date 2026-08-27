#pragma once

#include "ScheduleConfig.h"
#include "Scraper.h"

#include <QDate>
#include <QObject>
#include <QPointer>
#include <QPoint>
#include <QTimer>

#ifdef JUPITR_USE_KSTATUSNOTIFIERITEM
#include <KStatusNotifierItem>
#else
#include <QSystemTrayIcon>
#endif

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
    void requestDayType(const QDate &date, bool force = false);
    void updateTrayTooltip();
    void positionPopup();

    ScheduleConfig m_config;
    Scraper m_scraper;
#ifdef JUPITR_USE_KSTATUSNOTIFIERITEM
    KStatusNotifierItem m_tray;
#else
    QSystemTrayIcon m_tray;
#endif
    QMenu *m_menu = nullptr;
    QPointer<PopupWindow> m_popup;
    QPointer<SettingsWindow> m_settings;
    QPoint m_popupAnchor;
    QString m_currentDayType = QStringLiteral("Loading...");
    QDate m_lastCheckedDate;
    QDate m_lastAttemptedDate;
    bool m_dayTypeRequestInFlight = false;
    QTimer m_updateTimer;
};

} // namespace jupitr
