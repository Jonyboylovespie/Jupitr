#pragma once

#include <QTime>
#include <QString>
#include <QVector>

#include <optional>

namespace jupitr {

struct TimeBlock {
    QString name;
    QTime start;
    QTime end;
};

struct LunchInfo {
    QString label;
    QTime start;
    QTime end;
};

struct MiniBlock {
    QString name;
    QTime start;
    QTime end;
};

struct CurrentBlockResult {
    std::optional<TimeBlock> current;
    int remainingSeconds = 0;
    int index = -1;
};

struct CurrentMiniResult {
    std::optional<MiniBlock> current;
    int remainingSeconds = 0;
};

class BellSchedule final {
public:
    static QVector<TimeBlock> blocksForDayType(const QString &dayType);
    static QVector<MiniBlock> minisForBlock(int academicBlockIndex, bool advisory);
    static QVector<MiniBlock> minisForApplicationBlock(int applicationBlockIndex, const QString &dayType);
    static CurrentBlockResult currentBlock(const QTime &now, const QString &dayType);
    static std::optional<CurrentMiniResult> currentMini(const QTime &now, const QString &dayType);
    static std::optional<LunchInfo> lunchInfo(std::optional<int> wave, bool advisory);
    static std::optional<LunchInfo> lunchInfo(std::optional<int> wave, const QString &dayType);

    static bool isAdvisoryDay(const QString &dayType);
    static bool isAllPeriodsDay(const QString &dayType);
    static std::optional<QString> extractDayLetter(const QString &dayType);
    static std::optional<QString> configDayLetter(int applicationBlockIndex, const QString &dayType);
    static std::optional<QString> lunchDayLetter(const QString &dayType);
    static std::optional<int> configBlockIndex(int applicationBlockIndex, const QString &dayType);

    static QString formatRemaining(int seconds);
    static QString formatTime(const QTime &time);
};

} // namespace jupitr
