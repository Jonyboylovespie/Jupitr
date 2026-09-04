#include "ScheduleConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace jupitr {
namespace {

const QStringList kDayLetters = {
    QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"), QStringLiteral("D"),
    QStringLiteral("E"), QStringLiteral("F"), QStringLiteral("G"), QStringLiteral("H")
};

void ensureDefaults(ScheduleConfig &config)
{
    for (const auto &day : kDayLetters) {
        if (!config.classes().contains(day))
            config.setClasses(day, {QString(), QString(), QString(), QString()});
        if (!config.lunchWaves().contains(day))
            config.setLunchWave(day, 1);
    }
}

} // namespace

ScheduleConfig ScheduleConfig::createDefault()
{
    ScheduleConfig config;
    for (const auto &day : kDayLetters) {
        config.m_classes.insert(day, {QString(), QString(), QString(), QString()});
        config.m_lunchWaves.insert(day, 1);
    }
    return config;
}

QString ScheduleConfig::defaultPath()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (directory.isEmpty())
        directory = QDir::homePath() + QStringLiteral("/.config/jupitr");
    return QDir(directory).filePath(QStringLiteral("schedule.json"));
}

ScheduleConfig ScheduleConfig::load(const QString &path)
{
    const QString actualPath = path.isEmpty() ? defaultPath() : path;
    QFile file(actualPath);
    if (!file.open(QIODevice::ReadOnly))
        return createDefault();

    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return createDefault();

    ScheduleConfig config;
    const auto root = document.object();
    const auto classes = root.value(QStringLiteral("Classes")).toObject();
    for (auto it = classes.constBegin(); it != classes.constEnd(); ++it) {
        QStringList values;
        for (const auto &value : it.value().toArray())
            values.append(value.toString());
        config.m_classes.insert(it.key(), values);
    }

    const auto lunchWaves = root.value(QStringLiteral("LunchWaves")).toObject();
    for (auto it = lunchWaves.constBegin(); it != lunchWaves.constEnd(); ++it)
        config.m_lunchWaves.insert(it.key(), it.value().toInt(1));


    ensureDefaults(config);
    return config;
}

bool ScheduleConfig::save(const QString &path, QString *error) const
{
    const QString actualPath = path.isEmpty() ? defaultPath() : path;
    const QFileInfo fileInfo(actualPath);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        if (error)
            *error = QStringLiteral("Could not create %1").arg(fileInfo.absolutePath());
        return false;
    }

    QJsonObject classes;
    for (auto it = m_classes.constBegin(); it != m_classes.constEnd(); ++it) {
        QJsonArray values;
        for (const auto &value : it.value())
            values.append(value);
        classes.insert(it.key(), values);
    }

    QJsonObject lunchWaves;
    for (auto it = m_lunchWaves.constBegin(); it != m_lunchWaves.constEnd(); ++it)
        lunchWaves.insert(it.key(), it.value());


    QJsonObject root;
    root.insert(QStringLiteral("Classes"), classes);
    root.insert(QStringLiteral("LunchWaves"), lunchWaves);

    QSaveFile file(actualPath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

QString ScheduleConfig::className(const QString &dayLetter, int blockIndex) const
{
    const auto values = m_classes.value(dayLetter);
    return blockIndex >= 0 && blockIndex < values.size() ? values.at(blockIndex) : QString();
}

std::optional<int> ScheduleConfig::lunchWave(const QString &dayLetter) const
{
    if (!m_lunchWaves.contains(dayLetter))
        return std::nullopt;

    const int wave = m_lunchWaves.value(dayLetter);
    if (wave < 1 || wave > 4)
        return std::nullopt;
    return wave;
}

bool ScheduleConfig::usesWindPhysicsLunch(const QString &blockThreeClass)
{
    const auto parts = blockThreeClass.split(QLatin1Char('/'));
    return parts.size() == 2 &&
        parts.at(0).trimmed().compare(QStringLiteral("Wind"), Qt::CaseInsensitive) == 0 &&
        parts.at(1).trimmed().compare(QStringLiteral("Physics"), Qt::CaseInsensitive) == 0;
}

void ScheduleConfig::setClasses(const QString &dayLetter, const QStringList &classes)
{
    m_classes.insert(dayLetter, classes);
}

void ScheduleConfig::setLunchWave(const QString &dayLetter, int wave)
{
    m_lunchWaves.insert(dayLetter, wave);
}

} // namespace jupitr
