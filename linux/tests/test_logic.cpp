#include "BellSchedule.h"
#include "ScheduleConfig.h"
#include "Scraper.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace jupitr;

class LogicTest final : public QObject {
    Q_OBJECT

private slots:
    void scheduleUsesSharedTimes();
    void currentBlockAndMiniLookup();
    void advisoryIndexMappingAndLunch();
    void allPeriodsScheduleMapping();
    void formattingAndDayLetters();
    void officialLetterDayCalendar();
    void configurationRoundTrip();
};

void LogicTest::scheduleUsesSharedTimes()
{
    const auto regular = BellSchedule::blocksForDayType(QStringLiteral("C Day"));
    QCOMPARE(regular.size(), 4);
    QCOMPARE(regular.at(0).start, QTime(7, 40));
    QCOMPARE(regular.at(0).end, QTime(9, 6));
    QCOMPARE(regular.at(2).start, QTime(11, 4));

    const auto minis = BellSchedule::minisForBlock(0, false);
    QCOMPARE(minis.size(), 2);
    QCOMPARE(minis.at(1).start, QTime(8, 26));
    QCOMPARE(minis.at(1).end, QTime(9, 6));
}

void LogicTest::currentBlockAndMiniLookup()
{
    const auto current = BellSchedule::currentBlock(QTime(8, 0), QStringLiteral("A Day"));
    QVERIFY(current.current.has_value());
    QCOMPARE(current.current->name, QStringLiteral("Block 1"));
    QCOMPARE(current.index, 0);
    QCOMPARE(current.remainingSeconds, 66 * 60);

    const auto mini = BellSchedule::currentMini(QTime(11, 10), QStringLiteral("A Day"));
    QVERIFY(mini.has_value());
    QVERIFY(mini->current.has_value());
    QCOMPARE(mini->current->name, QStringLiteral("M1"));
    QCOMPARE(mini->remainingSeconds, 18 * 60);
}

void LogicTest::advisoryIndexMappingAndLunch()
{
    const auto blocks = BellSchedule::blocksForDayType(QStringLiteral("D Day - Advisory Schedule"));
    QCOMPARE(blocks.size(), 5);
    QVERIFY(!BellSchedule::configBlockIndex(1, QStringLiteral("D Day - Advisory Schedule")).has_value());
    QCOMPARE(BellSchedule::configBlockIndex(2, QStringLiteral("D Day - Advisory Schedule")).value(), 1);

    const auto mini = BellSchedule::currentMini(QTime(9, 50), QStringLiteral("D Day - Advisory Schedule"));
    QVERIFY(mini.has_value());
    QCOMPARE(mini->current->name, QStringLiteral("M1"));
    QCOMPARE(mini->current->end, QTime(10, 13));

    const auto lunch = BellSchedule::lunchInfo(3, true);
    QVERIFY(lunch.has_value());
    QCOMPARE(lunch->start, QTime(11, 54));
    QCOMPARE(lunch->end, QTime(12, 22));
    QVERIFY(!BellSchedule::lunchInfo(std::nullopt, false).has_value());
}

void LogicTest::allPeriodsScheduleMapping()
{
    const auto dayType = QStringLiteral("Special \"A\" Day Schedule-All Periods Meet");
    QVERIFY(BellSchedule::isAllPeriodsDay(dayType));
    QVERIFY(BellSchedule::isAllPeriodsDay(QStringLiteral("First Day of Classes: All periods meet")));

    const auto blocks = BellSchedule::blocksForDayType(dayType);
    QCOMPARE(blocks.size(), 8);
    QCOMPARE(blocks.at(0).name, QStringLiteral("Period 2"));
    QCOMPARE(blocks.at(4).name, QStringLiteral("Period 1"));
    QCOMPARE(blocks.at(4).start, QTime(10, 40));
    QCOMPARE(blocks.last().end, QTime(14, 20));

    QCOMPARE(BellSchedule::configDayLetter(0, dayType).value(), QStringLiteral("A"));
    QCOMPARE(BellSchedule::configDayLetter(4, dayType).value(), QStringLiteral("B"));
    QCOMPARE(BellSchedule::configBlockIndex(0, dayType).value(), 0);
    QCOMPARE(BellSchedule::configBlockIndex(4, dayType).value(), 0);
    QCOMPARE(BellSchedule::lunchDayLetter(dayType).value(), QStringLiteral("B"));

    const auto mini = BellSchedule::currentMini(QTime(12, 35), dayType);
    QVERIFY(mini.has_value());
    QCOMPARE(mini->current->name, QStringLiteral("M2"));
    QCOMPARE(mini->current->end, QTime(12, 50));

    const auto lunch = BellSchedule::lunchInfo(4, dayType);
    QVERIFY(lunch.has_value());
    QCOMPARE(lunch->start, QTime(11, 35));
    QCOMPARE(lunch->end, QTime(12, 5));
}

void LogicTest::formattingAndDayLetters()
{
    QCOMPARE(BellSchedule::formatRemaining(3723), QStringLiteral("1h 2m 3s"));
    QCOMPARE(BellSchedule::formatRemaining(125), QStringLiteral("2m 5s"));
    QCOMPARE(BellSchedule::formatTime(QTime(14, 20)), QStringLiteral("2:20 PM"));
    QCOMPARE(BellSchedule::extractDayLetter(QStringLiteral("Wed, Aug 19 · C Day")).value(), QStringLiteral("C"));
    QVERIFY(!BellSchedule::extractDayLetter(QStringLiteral("Unknown")).has_value());
}

void LogicTest::officialLetterDayCalendar()
{
    const auto firstDay = Scraper::dayTypeForDate(QDate(2026, 8, 26));
    QCOMPARE(firstDay, QStringLiteral("A Day - First Day of Classes"));
    QVERIFY(BellSchedule::isAllPeriodsDay(firstDay));
    QCOMPARE(Scraper::dayTypeForDate(QDate(2026, 8, 28)),
             QStringLiteral("C Day - Advisory Schedule"));
    QCOMPARE(Scraper::dayTypeForDate(QDate(2027, 6, 1)), QStringLiteral("G Day"));
    QVERIFY(Scraper::dayTypeForDate(QDate(2026, 11, 3)).isEmpty());
    QVERIFY(Scraper::dayTypeForDate(QDate(2028, 1, 1)).isEmpty());
}

void LogicTest::configurationRoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("schedule.json"));

    auto config = ScheduleConfig::createDefault();
    config.setClasses(QStringLiteral("C"), {QStringLiteral("AP Physics / Spanish"), QStringLiteral("English"), {}, QStringLiteral("Lunch")});
    config.setLunchWave(QStringLiteral("C"), 4);
    config.setClasses(QStringLiteral("H"), {QStringLiteral("Statistics"), QStringLiteral("Literature"), QStringLiteral("Wind/Physics"), QStringLiteral("Seminar")});
    config.setAdditionalLunchWave(QStringLiteral("H"), 1);
    config.setClasses(QStringLiteral("A"), {QString(), QString(), QStringLiteral("Free/Health"), QString()});
    config.setAdditionalLunchWave(QStringLiteral("A"), 2);
    QVERIFY(config.save(path));

    const auto loaded = ScheduleConfig::load(path);
    QCOMPARE(loaded.className(QStringLiteral("C"), 0), QStringLiteral("AP Physics / Spanish"));
    QCOMPARE(loaded.className(QStringLiteral("C"), 1), QStringLiteral("English"));
    QCOMPARE(loaded.lunchWave(QStringLiteral("C")).value(), 4);
    QCOMPARE(loaded.lunchWave(QStringLiteral("A")).value(), 1);
    QCOMPARE(loaded.additionalLunchWave(QStringLiteral("H")).value(), 1);
    QVERIFY(!loaded.additionalLunchWave(QStringLiteral("A")).has_value());
    QVERIFY(ScheduleConfig::supportsAdditionalLunch(QStringLiteral("Wind/Physics")));
    QVERIFY(!ScheduleConfig::supportsAdditionalLunch(QStringLiteral("Free/Health")));
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    Q_INIT_RESOURCE(jupitr);
    LogicTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_logic.moc"
