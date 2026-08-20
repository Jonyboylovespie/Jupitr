#include "TrayApp.h"

#include "BellSchedule.h"
#include "PopupWindow.h"
#include "SettingsWindow.h"
#include "Theme.h"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QDateTime>
#include <QDBusInterface>
#include <QDBusReply>
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

std::optional<QPoint> nativePointerPosition()
{
    if (QGuiApplication::platformName() != QStringLiteral("xcb"))
        return std::nullopt;

    auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11 || !x11->connection())
        return std::nullopt;

    xcb_connection_t *connection = x11->connection();
    const xcb_screen_iterator_t screens = xcb_setup_roots_iterator(xcb_get_setup(connection));
    if (!screens.data)
        return std::nullopt;

    const auto cookie = xcb_query_pointer(connection, screens.data->root);
    xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(connection, cookie, nullptr);
    if (!reply)
        return std::nullopt;
    const QPoint position(reply->root_x, reply->root_y);
    std::free(reply);
    return position;
}
#endif

} // namespace

namespace jupitr {

TrayApp::TrayApp(QObject *parent)
    : QObject(parent), m_config(ScheduleConfig::load()), m_scraper(this), m_tray(Theme::trayIcon(), this)
{
    m_menu = new QMenu;
    auto *quitAction = m_menu->addAction(QStringLiteral("Quit"));
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

    m_tray.setContextMenu(m_menu);
    m_tray.setToolTip(QStringLiteral("Jupitr — School Schedule"));
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive && m_popup && m_popup->isVisible())
            m_popup->hide();
    });
    connect(&m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            m_popupAnchor = QCursor::pos();
#ifdef Q_OS_LINUX
            m_nativePopupAnchor = nativePointerPosition();
            QDBusInterface kwin(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"),
                                QStringLiteral("org.kde.KWin"));
            const QDBusReply<QString> output = kwin.call(QStringLiteral("activeOutputName"));
            m_popupScreenName = output.isValid() ? output.value() : QString();
#endif
            showPopup();
        }
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

    const QPoint anchor = m_popupAnchor.isNull() ? QCursor::pos() : m_popupAnchor;
    QPoint scaledAnchor = anchor;
    QScreen *screen = nullptr;

    // KWin knows which output received the panel click, so prefer its output
    // identity over attempting to infer a monitor from mixed coordinate spaces.
    for (QScreen *candidate : QApplication::screens()) {
        if (candidate->name() == m_popupScreenName) {
            screen = candidate;
            break;
        }
    }

    auto mapNativeAnchor = [this](QScreen *candidate) -> QPoint {
        const QPoint nativeAnchor = *m_nativePopupAnchor;
        const qreal scale = candidate->devicePixelRatio();
        const QRect logical = candidate->geometry();
        // XWayland reports the pointer relative to the active output, while
        // QScreen uses a global origin plus logical (scaled) dimensions.
        return QPoint(logical.x() + qRound(nativeAnchor.x() / scale),
                      logical.y() + qRound(nativeAnchor.y() / scale));
    };

    if (screen && m_nativePopupAnchor.has_value())
        scaledAnchor = mapNativeAnchor(screen);

    // Fall back to matching the native pointer against physical output
    // rectangles on desktops that do not expose KWin's output-name method.
    if (!screen && m_nativePopupAnchor.has_value()) {
        const QPoint nativeAnchor = *m_nativePopupAnchor;
        for (QScreen *candidate : QApplication::screens()) {
            const qreal scale = candidate->devicePixelRatio();
            const QRect logical = candidate->geometry();
            const QRect native(logical.x(), logical.y(),
                               qRound(logical.width() * scale), qRound(logical.height() * scale));
            if (native.contains(nativeAnchor)) {
                screen = candidate;
                scaledAnchor = mapNativeAnchor(candidate);
                break;
            }
        }
    }

    if (!screen) {
        for (QScreen *candidate : QApplication::screens()) {
            const qreal scale = candidate->devicePixelRatio();
            const QPoint corrected(qRound(anchor.x() * scale), qRound(anchor.y() * scale));
            if (candidate->geometry().contains(corrected)) {
                screen = candidate;
                scaledAnchor = corrected;
                break;
            }
        }
    }
    if (!screen) {
        screen = QApplication::screenAt(anchor);
        if (!screen)
            screen = QApplication::primaryScreen();
        if (screen) {
            const qreal scale = screen->devicePixelRatio();
            scaledAnchor = QPoint(qRound(anchor.x() * scale), qRound(anchor.y() * scale));
        }
    }
    if (!screen)
        return;

    const QRect workArea = screen->availableGeometry();
    const QRect trayGeometry = m_tray.geometry();
    QPoint position;
    // KDE's XWayland work area can include the panel. The StatusNotifier
    // geometry gives us both the visible icon center and the panel's top edge,
    // while the activation cursor remains a fallback for other desktops.
    if (trayGeometry.isValid()) {
        position.setX(trayGeometry.center().x() - m_popup->width() / 2);
        position.setY(trayGeometry.top() - m_popup->height() - 8);
    } else {
        position.setX(scaledAnchor.x() - m_popup->width() / 2);
        position.setY(workArea.bottom() - m_popup->height() - 56);
    }

    position.setX(qBound(workArea.left(), position.x(), workArea.right() - m_popup->width()));
    position.setY(qBound(workArea.top(), position.y(), workArea.bottom() - m_popup->height()));
    m_popup->move(position);
}

} // namespace jupitr
