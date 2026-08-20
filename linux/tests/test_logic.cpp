#include "BellSchedule.h"
#include "ScheduleConfig.h"
#include "Scraper.h"

#include <QFile>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QtTest>

using namespace jupitr;

class LogicTest final : public QObject {
    Q_OBJECT

private slots:
    void scheduleUsesSharedTimes();
    void currentBlockAndMiniLookup();
    void advisoryIndexMappingAndLunch();
    void formattingAndDayLetters();
    void scraperFixtures();
    void scraperUsesCache();
    void scraperHandlesNetworkFailure();
    void configurationRoundTrip();

private:
    static QString fixture(const QString &name);
};

QString LogicTest::fixture(const QString &name)
{
    QFile file(QStringLiteral(JUPITR_FIXTURE_DIR) + QLatin1Char('/') + name);
    if (!file.open(QIODevice::ReadOnly))
        qFatal("Could not open fixture: %s", qPrintable(file.errorString()));
    return QString::fromUtf8(file.readAll());
}

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

void LogicTest::formattingAndDayLetters()
{
    QCOMPARE(BellSchedule::formatRemaining(3723), QStringLiteral("1h 2m 3s"));
    QCOMPARE(BellSchedule::formatRemaining(125), QStringLiteral("2m 5s"));
    QCOMPARE(BellSchedule::formatTime(QTime(14, 20)), QStringLiteral("2:20 PM"));
    QCOMPARE(BellSchedule::extractDayLetter(QStringLiteral("Wed, Aug 19 · C Day")).value(), QStringLiteral("C"));
    QVERIFY(!BellSchedule::extractDayLetter(QStringLiteral("Unknown")).has_value());
}

void LogicTest::scraperFixtures()
{
    QCOMPARE(Scraper::parseDayTypeFromHtml(fixture(QStringLiteral("normal-a-day.html")), QDate(2026, 8, 19)),
             QStringLiteral("C Day"));
    QCOMPARE(Scraper::parseDayTypeFromHtml(fixture(QStringLiteral("advisory-day.html")), QDate(2026, 8, 20)),
             QStringLiteral("D Day - Advisory Schedule"));
    QVERIFY(Scraper::parseDayTypeFromHtml(fixture(QStringLiteral("no-dhs-entry.html")), QDate(2026, 8, 21)).isEmpty());
    QVERIFY(Scraper::parseDayTypeFromHtml(fixture(QStringLiteral("malformed.html")), QDate(2026, 8, 19)).isEmpty());
}

void LogicTest::scraperUsesCache()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile cache(directory.filePath(QStringLiteral("calendar_cache.txt")));
    QVERIFY(cache.open(QIODevice::WriteOnly | QIODevice::Text));
    cache.write("2026-08-22|Cached C Day\n");
    cache.close();

    Scraper scraper(QUrl(QStringLiteral("http://127.0.0.1:1/")), directory.path());
    QString result;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(1000);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    scraper.getDayType(QDate(2026, 8, 22), [&result, &loop](const QString &dayType) {
        result = dayType;
        loop.quit();
    });
    timeout.start();
    loop.exec();
    QCOMPARE(result, QStringLiteral("Cached C Day"));
}

void LogicTest::scraperHandlesNetworkFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Scraper scraper(QUrl(QStringLiteral("http://127.0.0.1:1/")), directory.path());
    QString result = QStringLiteral("not-finished");
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(3000);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    scraper.getDayType(QDate(2026, 8, 23), [&result, &loop](const QString &dayType) {
        result = dayType;
        loop.quit();
    });
    timeout.start();
    loop.exec();
    QVERIFY(result.isEmpty());
}

void LogicTest::configurationRoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("schedule.json"));

    auto config = ScheduleConfig::createDefault();
    config.setClasses(QStringLiteral("C"), {QStringLiteral("AP Physics / Spanish"), QStringLiteral("English"), {}, QStringLiteral("Lunch")});
    config.setLunchWave(QStringLiteral("C"), 4);
    QVERIFY(config.save(path));

    const auto loaded = ScheduleConfig::load(path);
    QCOMPARE(loaded.className(QStringLiteral("C"), 0), QStringLiteral("AP Physics / Spanish"));
    QCOMPARE(loaded.className(QStringLiteral("C"), 1), QStringLiteral("English"));
    QCOMPARE(loaded.lunchWave(QStringLiteral("C")).value(), 4);
    QCOMPARE(loaded.lunchWave(QStringLiteral("A")).value(), 1);
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    Q_INIT_RESOURCE(jupitr);
    LogicTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_logic.moc"
