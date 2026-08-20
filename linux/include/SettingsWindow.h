#pragma once

#include "ScheduleConfig.h"

#include <QWidget>

#include <array>

class QComboBox;
class QLineEdit;

namespace jupitr {

class SettingsWindow final : public QWidget {
    Q_OBJECT

public:
    explicit SettingsWindow(ScheduleConfig &config, QWidget *parent = nullptr);

private:
    void saveAndClose();
    void loadValues();

    ScheduleConfig &m_config;
    std::array<std::array<QLineEdit *, 4>, 8> m_classInputs{};
    std::array<QComboBox *, 8> m_lunchInputs{};
};

} // namespace jupitr
