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
#include <QTimer>

#include <cstdlib>
#include <cstring>
#include <iterator>

#ifdef Q_OS_LINUX
#include <QtGui/qguiapplication_platform.h>
#include <xcb/xcb.h>
#endif

namespace {

#ifdef Q_OS_LINUX
xcb_atom_t xcbAtom(xcb_connection_t *connection, const char *name)
{
    const auto cookie = xcb_intern_atom(connection, false, static_cast<uint16_t>(std::strlen(name)), name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, nullptr);
    if (!reply)
        return XCB_ATOM_NONE;
    const xcb_atom_t atom = reply->atom;
    std::free(reply);
    return atom;
}

void markAsTrayPopup(QWidget *window)
{
    if (QGuiApplication::platformName() != QStringLiteral("xcb"))
        return;

    auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11 || !x11->connection())
        return;

    xcb_connection_t *connection = x11->connection();
    const xcb_window_t id = static_cast<xcb_window_t>(window->winId());
    const xcb_atom_t state = xcbAtom(connection, "_NET_WM_STATE");
    const xcb_atom_t states[] = {
        xcbAtom(connection, "_NET_WM_STATE_SKIP_TASKBAR"),
        xcbAtom(connection, "_NET_WM_STATE_SKIP_PAGER"),
        xcbAtom(connection, "_KDE_NET_WM_STATE_SKIP_SWITCHER"),
    };
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, id, state, XCB_ATOM_ATOM, 32,
                        std::size(states), states);
    xcb_flush(connection);
}

#ifdef JUPITR_USE_KSTATUSNOTIFIERITEM
QPoint logicalTrayPosition(const QPoint &nativePosition)
{
    QScreen *screen = nullptr;
    for (QScreen *candidate : QApplication::screens()) {
        const qreal scale = candidate->devicePixelRatio();
        const QRect logical = candidate->geometry();
        const QRect native(qRound(logical.x() * scale), qRound(logical.y() * scale),
                           qRound(logical.width() * scale), qRound(logical.height() * scale));
        if (native.contains(nativePosition)) {
            screen = candidate;
            break;
        }
    }

    if (!screen)
        screen = QApplication::primaryScreen();
    if (!screen)
        return nativePosition;

    const qreal scale = screen->devicePixelRatio();
    const QRect logical = screen->geometry();
    const QPoint nativeOrigin(qRound(logical.x() * scale), qRound(logical.y() * scale));
    return logical.topLeft()
        + QPoint(qRound((nativePosition.x() - nativeOrigin.x()) / scale),
                 qRound((nativePosition.y() - nativeOrigin.y()) / scale));
}
#endif

#endif

} // namespace

namespace jupitr {

TrayApp::TrayApp(QObject *parent)
    : QObject(parent), m_config(ScheduleConfig::load()), m_scraper(this)
#ifdef JUPITR_USE_KSTATUSNOTIFIERITEM
    , m_tray(QStringLiteral("jupitr"), this)
#else
    , m_tray(Theme::trayIcon(), this)
#endif
{
    m_menu = new QMenu;
    auto *quitAction = m_menu->addAction(QStringLiteral("Quit"));
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

    m_tray.setContextMenu(m_menu);
#ifdef JUPITR_USE_KSTATUSNOTIFIERITEM
    m_tray.setCategory(KStatusNotifierItem::ApplicationStatus);
    m_tray.setTitle(QStringLiteral("Jupitr"));
    m_tray.setIconByPixmap(Theme::trayIcon());
    m_tray.setToolTip(Theme::trayIcon(), QStringLiteral("Jupitr"),
                      QStringLiteral("School Schedule"));
#else
    m_tray.setToolTip(QStringLiteral("Jupitr — School Schedule"));
#endif
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive && m_popup && m_popup->isVisible())
            m_popup->hide();
    });
#ifdef JUPITR_USE_KSTATUSNOTIFIERITEM
    connect(&m_tray, &KStatusNotifierItem::activateRequested,
            this, [this](bool, const QPoint &position) {
#ifdef Q_OS_LINUX
        // Plasma sends StatusNotifierItem activation coordinates in physical
        // pixels. QWidget::move() uses Qt logical coordinates, so convert once
        // before using the click as the popup's horizontal center.
        m_popupAnchor = logicalTrayPosition(position);
#else
        m_popupAnchor = position;
#endif
        showPopup();
    });
    m_tray.setStatus(KStatusNotifierItem::Active);
#else
    connect(&m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            m_popupAnchor = QCursor::pos();
            showPopup();
        }
    });
    m_tray.show();
#endif

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
    if (m_popup && m_popup->isVisible()) {
        m_popup->hide();
        return;
    }

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
#ifdef Q_OS_LINUX
    markAsTrayPopup(m_popup);
#endif
    positionPopup();
    m_popup->show();
#ifdef Q_OS_LINUX
    markAsTrayPopup(m_popup);
#endif
    m_popup->raise();
    m_popup->activateWindow();
    m_popup->setFocus(Qt::PopupFocusReason);
    // KWin can apply its own placement policy while mapping a new window.
    // Reapply the tray-relative position after that map has completed.
    QTimer::singleShot(0, this, [this]() {
        if (m_popup && m_popup->isVisible()) {
#ifdef Q_OS_LINUX
            markAsTrayPopup(m_popup);
#endif
            positionPopup();
            m_popup->raise();
        }
    });
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
#ifdef JUPITR_USE_KSTATUSNOTIFIERITEM
        m_tray.setToolTip(Theme::trayIcon(), QStringLiteral("Jupitr"),
                          QStringLiteral("Schedule unavailable"));
#else
        m_tray.setToolTip(QStringLiteral("Jupitr — schedule unavailable"));
#endif
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
#ifdef JUPITR_USE_KSTATUSNOTIFIERITEM
    m_tray.setToolTip(Theme::trayIcon(), QStringLiteral("Jupitr"), tooltip);
#else
    m_tray.setToolTip(tooltip);
#endif
}

void TrayApp::positionPopup()
{
    if (!m_popup)
        return;

    const QPoint anchor = m_popupAnchor.isNull() ? QCursor::pos() : m_popupAnchor;
    QScreen *screen = QApplication::screenAt(anchor);
    if (!screen)
        screen = QApplication::primaryScreen();
    if (!screen)
        return;

    const QRect workArea = screen->availableGeometry();
    QPoint position;
    position.setX(qBound(workArea.left(), anchor.x() - m_popup->width() / 2,
                         workArea.right() - m_popup->width() + 1));
#ifdef JUPITR_USE_KSTATUSNOTIFIERITEM
    position.setY(workArea.bottom() - m_popup->height() - 56);
#else
    const QRect trayGeometry = m_tray.geometry();
    if (trayGeometry.isValid()) {
        position.setY(trayGeometry.top() - m_popup->height() - 8);
    } else {
        position.setY(workArea.bottom() - m_popup->height() - 56);
    }
#endif

    position.setY(qBound(workArea.top(), position.y(), workArea.bottom() - m_popup->height()));
    m_popup->move(position);
}

} // namespace jupitr
