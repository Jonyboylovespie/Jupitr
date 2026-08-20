#pragma once

#include "ScheduleConfig.h"
#include "Scraper.h"

#include <QDate>
#include <QObject>
#include <QPointer>
#include <QPoint>
#include <QSystemTrayIcon>
#include <QTimer>

#include <optional>

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
    QPoint m_popupAnchor;
    std::optional<QPoint> m_nativePopupAnchor;
    QString m_popupScreenName;
    QString m_currentDayType = QStringLiteral("Loading...");
    QDate m_lastCheckedDate;
    QTimer m_updateTimer;
};

} // namespace jupitr
