#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

#include <optional>

namespace jupitr {

class ScheduleConfig final {
public:
    static ScheduleConfig load(const QString &path = {});
    static ScheduleConfig createDefault();
    static QString defaultPath();

    bool save(const QString &path = {}, QString *error = nullptr) const;

    QString className(const QString &dayLetter, int blockIndex) const;
    std::optional<int> lunchWave(const QString &dayLetter) const;
    static bool usesWindPhysicsLunch(const QString &blockThreeClass);
    void setClasses(const QString &dayLetter, const QStringList &classes);
    void setLunchWave(const QString &dayLetter, int wave);

    const QMap<QString, QStringList> &classes() const { return m_classes; }
    const QMap<QString, int> &lunchWaves() const { return m_lunchWaves; }

private:
    QMap<QString, QStringList> m_classes;
    QMap<QString, int> m_lunchWaves;
};

} // namespace jupitr
