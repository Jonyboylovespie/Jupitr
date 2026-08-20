#include "Scraper.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QDebug>

#include <utility>

namespace jupitr {
namespace {

const QUrl kCalendarUrl(QStringLiteral("https://www.darienps.org/district-information/district-calendar"));

QString decodeHtml(QString value)
{
    return value.replace(QStringLiteral("&amp;"), QStringLiteral("&"))
        .replace(QStringLiteral("&quot;"), QStringLiteral("\""))
        .replace(QStringLiteral("&#39;"), QStringLiteral("'"))
        .replace(QStringLiteral("&nbsp;"), QStringLiteral(" "))
        .replace(QStringLiteral("&lt;"), QStringLiteral("<"))
        .replace(QStringLiteral("&gt;"), QStringLiteral(">"));
}

QString stripTags(QString value)
{
    value.replace(QRegularExpression(QStringLiteral(R"(<[^>]*>)")), QStringLiteral(" "));
    return decodeHtml(value).simplified();
}

int monthNumber(QString month)
{
    month = month.trimmed();
    const QStringList fullNames = {
        QStringLiteral("January"), QStringLiteral("February"), QStringLiteral("March"),
        QStringLiteral("April"), QStringLiteral("May"), QStringLiteral("June"),
        QStringLiteral("July"), QStringLiteral("August"), QStringLiteral("September"),
        QStringLiteral("October"), QStringLiteral("November"), QStringLiteral("December")
    };
    const QStringList shortNames = {
        QStringLiteral("Jan"), QStringLiteral("Feb"), QStringLiteral("Mar"),
        QStringLiteral("Apr"), QStringLiteral("May"), QStringLiteral("Jun"),
        QStringLiteral("Jul"), QStringLiteral("Aug"), QStringLiteral("Sep"),
        QStringLiteral("Oct"), QStringLiteral("Nov"), QStringLiteral("Dec")
    };

    for (int i = 0; i < fullNames.size(); ++i) {
        if (fullNames.at(i).compare(month, Qt::CaseInsensitive) == 0 ||
            shortNames.at(i).compare(month, Qt::CaseInsensitive) == 0)
            return i + 1;
    }
    return 0;
}

} // namespace

Scraper::Scraper(QObject *parent)
    : Scraper(kCalendarUrl, {}, parent)
{
}

Scraper::Scraper(const QUrl &calendarUrl, const QString &dataRoot, QObject *parent)
    : QObject(parent), m_calendarUrl(calendarUrl), m_dataRoot(dataRoot)
{
}

void Scraper::getDayType(const QDate &date, DayTypeCallback callback)
{
    if (!date.isValid() || !callback)
        return;

    if (m_memoryCache.contains(date)) {
        const auto result = m_memoryCache.value(date);
        QTimer::singleShot(0, this, [callback, result]() { callback(result); });
        return;
    }

    const auto cached = cachedDay(date);
    if (!cached.isEmpty()) {
        m_memoryCache.insert(date, cached);
        QTimer::singleShot(0, this, [callback, cached]() { callback(cached); });
        log(QStringLiteral("Cache hit for %1: %2").arg(date.toString(Qt::ISODate), cached));
        return;
    }

    if (m_pending.contains(date)) {
        m_pending[date].append(std::move(callback));
        return;
    }
    m_pending.insert(date, {std::move(callback)});

    QNetworkRequest request(m_calendarUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Jupitr/1.0"));
    auto *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, date]() {
        QString dayType;
        if (reply->error() == QNetworkReply::NoError) {
            const auto html = QString::fromUtf8(reply->readAll());
            dayType = parseDayTypeFromHtml(html, date);
            if (!dayType.isEmpty()) {
                m_memoryCache.insert(date, dayType);
                cacheDay(date, dayType);
                log(QStringLiteral("Parsed day type for %1: %2").arg(date.toString(Qt::ISODate), dayType));
            } else {
                log(QStringLiteral("No DHS day type found for %1").arg(date.toString(Qt::ISODate)));
            }
        } else {
            log(QStringLiteral("Calendar request failed: %1").arg(reply->errorString()));
        }

        const auto callbacks = m_pending.take(date);
        reply->deleteLater();
        for (const auto &callback : callbacks)
            callback(dayType);
    });
}

QString Scraper::parseDayTypeFromHtml(const QString &html, const QDate &targetDate)
{
    if (!targetDate.isValid())
        return {};

    const QRegularExpression dayBoxPattern(
        QStringLiteral(R"(<div\b[^>]*class\s*=\s*["'][^"']*\bfsCalendarDaybox\b[^"']*["'][^>]*>(.*?)(?=<div\b[^>]*class\s*=\s*["'][^"']*\bfsCalendarDaybox\b|$))"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpression monthPattern(
        QStringLiteral(R"(<span\b[^>]*class\s*=\s*["'][^"']*\bfsCalendarMonth\b[^"']*["'][^>]*>\s*([^<]+?)\s*</span>\s*(\d{1,2}))"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpression eventPattern(
        QStringLiteral(R"(<div\b[^>]*class\s*=\s*["'][^"']*\bfsCalendarInfo\b[^"']*["'][^>]*>(.*?)</div>)"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpression anchorPattern(
        QStringLiteral(R"(<a\b([^>]*)>(.*?)</a>)"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpression titlePattern(
        QStringLiteral(R"(\btitle\s*=\s*["']([^"']+)["'])"),
        QRegularExpression::CaseInsensitiveOption);

    auto dayBoxIterator = dayBoxPattern.globalMatch(html);
    while (dayBoxIterator.hasNext()) {
        const auto dayBoxMatch = dayBoxIterator.next();
        const auto boxHtml = dayBoxMatch.captured(1);
        const auto dateMatch = monthPattern.match(boxHtml);
        if (!dateMatch.hasMatch())
            continue;

        const int month = monthNumber(decodeHtml(dateMatch.captured(1)));
        const int day = dateMatch.captured(2).toInt();
        const QDate boxDate(targetDate.year(), month, day);
        if (!boxDate.isValid() || boxDate != targetDate)
            continue;

        auto eventIterator = eventPattern.globalMatch(boxHtml);
        while (eventIterator.hasNext()) {
            const auto eventHtml = eventIterator.next().captured(1);
            if (!stripTags(eventHtml).contains(QStringLiteral("DHS School Calendar"), Qt::CaseInsensitive))
                continue;

            auto anchorIterator = anchorPattern.globalMatch(eventHtml);
            while (anchorIterator.hasNext()) {
                const auto anchor = anchorIterator.next();
                const auto attributes = anchor.captured(1);
                if (!attributes.contains(QStringLiteral("fsCalendarEventTitle"), Qt::CaseInsensitive))
                    continue;

                const auto titleMatch = titlePattern.match(attributes);
                if (titleMatch.hasMatch())
                    return decodeHtml(titleMatch.captured(1).trimmed());

                const auto visibleText = stripTags(anchor.captured(2));
                if (!visibleText.isEmpty() && visibleText.size() < 100)
                    return visibleText;
            }
        }
    }

    return {};
}

QString Scraper::cachePath() const
{
    if (!m_dataRoot.isEmpty())
        return QDir(m_dataRoot).filePath(QStringLiteral("calendar_cache.txt"));
    return QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
        .filePath(QStringLiteral("calendar_cache.txt"));
}

QString Scraper::logPath() const
{
    if (!m_dataRoot.isEmpty())
        return QDir(m_dataRoot).filePath(QStringLiteral("scraper.log"));
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(QStringLiteral("scraper.log"));
}

QString Scraper::cachedDay(const QDate &date) const
{
    QFile file(cachePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream stream(&file);
    const QString prefix = date.toString(Qt::ISODate) + QStringLiteral("|");
    while (!stream.atEnd()) {
        const auto line = stream.readLine();
        if (line.startsWith(prefix))
            return line.mid(prefix.size()).trimmed();
    }
    return {};
}

void Scraper::cacheDay(const QDate &date, const QString &dayType) const
{
    const QFileInfo fileInfo(cachePath());
    if (!QDir().mkpath(fileInfo.absolutePath()))
        return;

    QStringList lines;
    QFile existing(cachePath());
    if (existing.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&existing);
        while (!stream.atEnd())
            lines.append(stream.readLine());
    }

    const QString prefix = date.toString(Qt::ISODate) + QStringLiteral("|");
    lines.removeIf([&prefix](const QString &line) { return line.startsWith(prefix); });
    lines.append(prefix + dayType);

    QSaveFile file(cachePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream stream(&file);
    for (const auto &line : lines)
        stream << line << Qt::endl;
    file.commit();
}

void Scraper::log(const QString &message) const
{
    const QFileInfo fileInfo(logPath());
    if (!QDir().mkpath(fileInfo.absolutePath()))
        return;

    QFile file(logPath());
    if (!file.open(QIODevice::Append | QIODevice::Text))
        return;
    QTextStream stream(&file);
    stream << '[' << QDateTime::currentDateTime().toString(Qt::ISODate) << "] " << message << Qt::endl;
}

} // namespace jupitr
