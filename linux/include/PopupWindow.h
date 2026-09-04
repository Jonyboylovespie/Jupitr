#pragma once

#include "BellSchedule.h"
#include "ScheduleConfig.h"

#include <QLabel>
#include <QTimer>
#include <QWidget>

#include <functional>

class QFocusEvent;
class QVBoxLayout;

namespace jupitr {

class PopupWindow final : public QWidget {
    Q_OBJECT

public:
    PopupWindow(ScheduleConfig &config, std::function<void()> openSettings, QWidget *parent = nullptr);

    void setDayType(const QString &dayType);
    void refreshData();

protected:
    void focusOutEvent(QFocusEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    struct ScheduleItem {
        QTime start;
        QTime end;
        QString name;
        bool lunch = false;
        bool afterLunch = false;
        bool mini = false;
        QString className;
    };

    struct LunchPeriod {
        LunchInfo info;
        int wave = 0;
    };

    void updateMainStatus(const QTime &now,
                         const QVector<TimeBlock> &blocks,
                         const CurrentBlockResult &current,
                         const QVector<LunchPeriod> &lunches);
    QVector<ScheduleItem> buildScheduleItems(const QVector<TimeBlock> &blocks,
                                             const QVector<LunchPeriod> &lunches) const;
    void rebuildSchedule(const QVector<ScheduleItem> &items, const QTime &now);
    void updateScheduleHighlights(const QVector<ScheduleItem> &items, const QTime &now);
    void clearScheduleRows();
    QWidget *createScheduleRow(const ScheduleItem &item, bool current, bool advisory, bool evenRow) const;

    QString classForBlock(int applicationBlockIndex, const QString &dayType) const;
    static QString miniClassName(const QString &rawName, const QString &miniLabel);
    static QString formattedClassName(const QString &rawName);

    ScheduleConfig &m_config;
    std::function<void()> m_openSettings;
    QString m_dayType = QStringLiteral("Unknown");
    QLabel *m_dayLabel = nullptr;
    QLabel *m_timerLabel = nullptr;
    QLabel *m_subtitleLabel = nullptr;
    QVBoxLayout *m_scheduleLayout = nullptr;
    QTimer m_updateTimer;
    QVector<ScheduleItem> m_lastItems;
    QString m_lastDayType;
};

} // namespace jupitr
