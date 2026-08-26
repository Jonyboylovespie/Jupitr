#include "BellSchedule.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QDebug>

namespace jupitr {
namespace {

struct ScheduleDefinition {
    QVector<TimeBlock> blocks;
    QVector<QVector<MiniBlock>> minis;
    QMap<int, LunchInfo> lunches;
};

struct ScheduleDocument {
    ScheduleDefinition regular;
    ScheduleDefinition advisory;
    ScheduleDefinition allPeriods;
};

QTime parseTime(const QJsonValue &value)
{
    return QTime::fromString(value.toString(), QStringLiteral("HH:mm"));
}

ScheduleDefinition parseDefinition(const QJsonObject &object)
{
    ScheduleDefinition definition;
    const auto blocks = object.value(QStringLiteral("blocks")).toArray();
    for (const auto &blockValue : blocks) {
        const auto block = blockValue.toObject();
        definition.blocks.push_back({
            block.value(QStringLiteral("name")).toString(),
            parseTime(block.value(QStringLiteral("start"))),
            parseTime(block.value(QStringLiteral("end")))
        });

        QVector<MiniBlock> minis;
        for (const auto &miniValue : block.value(QStringLiteral("minis")).toArray()) {
            const auto mini = miniValue.toObject();
            minis.push_back({
                mini.value(QStringLiteral("name")).toString(),
                parseTime(mini.value(QStringLiteral("start"))),
                parseTime(mini.value(QStringLiteral("end")))
            });
        }
        definition.minis.push_back(minis);
    }

    const auto lunchWaves = object.value(QStringLiteral("lunchWaves")).toObject();
    for (auto it = lunchWaves.constBegin(); it != lunchWaves.constEnd(); ++it) {
        bool ok = false;
        const int wave = it.key().toInt(&ok);
        if (!ok || wave < 1 || wave > 4)
            continue;

        const auto lunch = it.value().toObject();
        definition.lunches.insert(wave, {
            lunch.value(QStringLiteral("name")).toString(QStringLiteral("Lunch")),
            parseTime(lunch.value(QStringLiteral("start"))),
            parseTime(lunch.value(QStringLiteral("end")))
        });
    }

    return definition;
}

ScheduleDocument loadSchedule()
{
    QFile file(QStringLiteral(":/schedule/bell-schedule.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Unable to open embedded bell schedule:" << file.errorString();
        return {};
    }

    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning() << "Unable to parse embedded bell schedule:" << error.errorString();
        return {};
    }

    const auto root = document.object();
    return {
        parseDefinition(root.value(QStringLiteral("regular")).toObject()),
        parseDefinition(root.value(QStringLiteral("advisory")).toObject()),
        parseDefinition(root.value(QStringLiteral("allPeriods")).toObject())
    };
}

const ScheduleDocument &schedule()
{
    static const ScheduleDocument document = loadSchedule();
    return document;
}

const ScheduleDefinition &definitionFor(bool advisory)
{
    return advisory ? schedule().advisory : schedule().regular;
}

const ScheduleDefinition &definitionFor(const QString &dayType)
{
    if (BellSchedule::isAllPeriodsDay(dayType))
        return schedule().allPeriods;
    return definitionFor(BellSchedule::isAdvisoryDay(dayType));
}

} // namespace

QVector<TimeBlock> BellSchedule::blocksForDayType(const QString &dayType)
{
    return definitionFor(dayType).blocks;
}

QVector<MiniBlock> BellSchedule::minisForApplicationBlock(int applicationBlockIndex, const QString &dayType)
{
    if (applicationBlockIndex < 0)
        return {};
    return definitionFor(dayType).minis.value(applicationBlockIndex);
}

QVector<MiniBlock> BellSchedule::minisForBlock(int academicBlockIndex, bool advisory)
{
    const auto &definition = definitionFor(advisory);
    if (academicBlockIndex < 0)
        return {};

    int academicIndex = 0;
    for (int applicationIndex = 0; applicationIndex < definition.blocks.size(); ++applicationIndex) {
        if (definition.blocks.at(applicationIndex).name.contains(QStringLiteral("Advisory"), Qt::CaseInsensitive))
            continue;
        if (academicIndex == academicBlockIndex)
            return definition.minis.value(applicationIndex);
        ++academicIndex;
    }
    return {};
}

CurrentBlockResult BellSchedule::currentBlock(const QTime &now, const QString &dayType)
{
    const auto blocks = blocksForDayType(dayType);
    for (int index = 0; index < blocks.size(); ++index) {
        const auto &block = blocks.at(index);
        if (now >= block.start && now < block.end)
            return {block, block.start.isValid() ? now.secsTo(block.end) : 0, index};
    }
    return {};
}

std::optional<CurrentMiniResult> BellSchedule::currentMini(const QTime &now, const QString &dayType)
{
    const auto blocks = blocksForDayType(dayType);
    for (int applicationIndex = 0; applicationIndex < blocks.size(); ++applicationIndex) {
        const auto &block = blocks.at(applicationIndex);
        if (now < block.start || now >= block.end)
            continue;

        const auto minis = minisForApplicationBlock(applicationIndex, dayType);
        for (const auto &mini : minis) {
            if (now >= mini.start && now < mini.end)
                return CurrentMiniResult{mini, now.secsTo(mini.end)};
        }
        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<LunchInfo> BellSchedule::lunchInfo(std::optional<int> wave, const QString &dayType)
{
    if (!wave.has_value() || *wave < 1 || *wave > 4)
        return std::nullopt;

    const auto &lunches = definitionFor(dayType).lunches;
    const auto it = lunches.constFind(*wave);
    return it == lunches.constEnd() ? std::nullopt : std::optional<LunchInfo>(it.value());
}

std::optional<LunchInfo> BellSchedule::lunchInfo(std::optional<int> wave, bool advisory)
{
    if (!wave.has_value() || *wave < 1 || *wave > 4)
        return std::nullopt;

    const auto &lunches = definitionFor(advisory).lunches;
    const auto it = lunches.constFind(*wave);
    return it == lunches.constEnd() ? std::nullopt : std::optional<LunchInfo>(it.value());
}

bool BellSchedule::isAdvisoryDay(const QString &dayType)
{
    return dayType.contains(QStringLiteral("Advisory"), Qt::CaseInsensitive);
}

bool BellSchedule::isAllPeriodsDay(const QString &dayType)
{
    return dayType.contains(QStringLiteral("All periods meet"), Qt::CaseInsensitive) ||
        dayType.contains(QStringLiteral("First Day of Classes"), Qt::CaseInsensitive);
}

std::optional<QString> BellSchedule::extractDayLetter(const QString &dayType)
{
    for (const auto &letter : {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"),
                               QStringLiteral("D"), QStringLiteral("E"), QStringLiteral("F"),
                               QStringLiteral("G"), QStringLiteral("H")}) {
        if (dayType.contains(letter + QStringLiteral(" Day"), Qt::CaseInsensitive))
            return letter;
    }
    return std::nullopt;
}

std::optional<QString> BellSchedule::configDayLetter(int applicationBlockIndex, const QString &dayType)
{
    if (isAllPeriodsDay(dayType)) {
        if (applicationBlockIndex >= 0 && applicationBlockIndex < 4)
            return QStringLiteral("A");
        if (applicationBlockIndex >= 4 && applicationBlockIndex < 8)
            return QStringLiteral("B");
        return std::nullopt;
    }
    return extractDayLetter(dayType);
}

std::optional<QString> BellSchedule::lunchDayLetter(const QString &dayType)
{
    return isAllPeriodsDay(dayType)
        ? std::optional<QString>(QStringLiteral("B"))
        : extractDayLetter(dayType);
}

std::optional<int> BellSchedule::configBlockIndex(int applicationBlockIndex, const QString &dayType)
{
    const auto blocks = blocksForDayType(dayType);
    if (applicationBlockIndex < 0 || applicationBlockIndex >= blocks.size())
        return std::nullopt;
    if (isAllPeriodsDay(dayType))
        return applicationBlockIndex % 4;
    if (blocks.at(applicationBlockIndex).name.contains(QStringLiteral("Advisory"), Qt::CaseInsensitive))
        return std::nullopt;

    int skipped = 0;
    for (int i = 0; i < applicationBlockIndex; ++i) {
        if (blocks.at(i).name.contains(QStringLiteral("Advisory"), Qt::CaseInsensitive))
            ++skipped;
    }
    return applicationBlockIndex - skipped;
}

QString BellSchedule::formatRemaining(int seconds)
{
    seconds = qMax(0, seconds);
    const int hours = seconds / 3600;
    const int minutes = (seconds % 3600) / 60;
    const int remainder = seconds % 60;
    if (hours > 0)
        return QStringLiteral("%1h %2m %3s").arg(hours).arg(minutes).arg(remainder);
    if (minutes > 0)
        return QStringLiteral("%1m %2s").arg(minutes).arg(remainder);
    return QStringLiteral("%1s").arg(remainder);
}

QString BellSchedule::formatTime(const QTime &time)
{
    return time.toString(QStringLiteral("h:mm AP"));
}

} // namespace jupitr
