#include "Scraper.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <utility>

namespace jupitr {
namespace {

QJsonObject loadDays()
{
    QFile file(QStringLiteral(":/schedule/letter-day-calendar.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return {};
    return document.object().value(QStringLiteral("days")).toObject();
}

const QJsonObject &days()
{
    static const QJsonObject value = loadDays();
    return value;
}

} // namespace

Scraper::Scraper(QObject *parent)
    : QObject(parent)
{
}

void Scraper::getDayType(const QDate &date, DayTypeCallback callback)
{
    if (!callback)
        return;

    const auto result = dayTypeForDate(date);
    QTimer::singleShot(0, this, [callback = std::move(callback), result]() {
        callback(result);
    });
}

QString Scraper::dayTypeForDate(const QDate &date)
{
    if (!date.isValid())
        return {};
    return days().value(date.toString(Qt::ISODate)).toString();
}

} // namespace jupitr
