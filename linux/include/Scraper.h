#pragma once

#include <QDate>
#include <QHash>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>

namespace jupitr {

class Scraper final : public QObject {
    Q_OBJECT

public:
    using DayTypeCallback = std::function<void(const QString &dayType)>;

    explicit Scraper(QObject *parent = nullptr);
    Scraper(const QUrl &calendarUrl, const QString &dataRoot, QObject *parent = nullptr);

    void getDayType(const QDate &date, DayTypeCallback callback);

    // Pure parser entry point used by fixture-based tests and future offline
    // diagnostics. An empty result means no matching DHS entry was found.
    static QString parseDayTypeFromHtml(const QString &html, const QDate &targetDate);

private:
    struct PendingRequest {
        QDate date;
        QList<DayTypeCallback> callbacks;
    };

    QString cachePath() const;
    QString logPath() const;
    QString cachedDay(const QDate &date) const;
    void cacheDay(const QDate &date, const QString &dayType) const;
    void log(const QString &message) const;

    QNetworkAccessManager m_network;
    QUrl m_calendarUrl;
    QString m_dataRoot;
    QHash<QDate, QString> m_memoryCache;
    QHash<QDate, QList<DayTypeCallback>> m_pending;
};

} // namespace jupitr
