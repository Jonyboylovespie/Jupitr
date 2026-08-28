#pragma once

#include <QDate>
#include <QObject>
#include <QString>

#include <functional>

namespace jupitr {

class Scraper final : public QObject {
    Q_OBJECT

public:
    using DayTypeCallback = std::function<void(const QString &dayType)>;

    explicit Scraper(QObject *parent = nullptr);

    void getDayType(const QDate &date, DayTypeCallback callback);
    static QString dayTypeForDate(const QDate &date);
};

} // namespace jupitr
