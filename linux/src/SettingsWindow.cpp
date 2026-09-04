#include "SettingsWindow.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace jupitr {
namespace {

const QStringList kDayLetters = {
    QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"), QStringLiteral("D"),
    QStringLiteral("E"), QStringLiteral("F"), QStringLiteral("G"), QStringLiteral("H")
};

} // namespace

SettingsWindow::SettingsWindow(ScheduleConfig &config, QWidget *parent)
    : QWidget(parent), m_config(config)
{
    setWindowTitle(QStringLiteral("Edit Schedule — Jupitr"));
    setMinimumSize(650, 440);
    resize(760, 540);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("Edit Schedule"), this);
    title->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 700;"));
    root->addWidget(title);

    auto *subtitle = new QLabel(QStringLiteral("Enter your classes and lunch wave for each day"), this);
    subtitle->setStyleSheet(QStringLiteral("color: #B8A99A;"));
    root->addWidget(subtitle);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    grid->setColumnMinimumWidth(0, 70);
    for (int block = 0; block < 4; ++block)
        grid->setColumnStretch(block + 1, 1);
    grid->setColumnMinimumWidth(5, 100);

    auto *emptyHeader = new QLabel(this);
    grid->addWidget(emptyHeader, 0, 0);
    for (int block = 0; block < 4; ++block) {
        auto *header = new QLabel(QStringLiteral("Block %1").arg(block + 1), this);
        header->setAlignment(Qt::AlignCenter);
        header->setStyleSheet(QStringLiteral("color: #B8A99A; font-weight: 700;"));
        grid->addWidget(header, 0, block + 1);
    }
    auto *lunchHeader = new QLabel(QStringLiteral("Lunch"), this);
    lunchHeader->setAlignment(Qt::AlignCenter);
    lunchHeader->setStyleSheet(QStringLiteral("color: #B8A99A; font-weight: 700;"));
    grid->addWidget(lunchHeader, 0, 5);

    for (int dayIndex = 0; dayIndex < kDayLetters.size(); ++dayIndex) {
        const auto &day = kDayLetters.at(dayIndex);
        auto *dayLabel = new QLabel(day + QStringLiteral(" Day"), this);
        dayLabel->setStyleSheet(QStringLiteral("color: #FFD23F; font-weight: 700;"));
        grid->addWidget(dayLabel, dayIndex + 1, 0);

        for (int block = 0; block < 4; ++block) {
            auto *input = new QLineEdit(this);
            input->setPlaceholderText(QStringLiteral("Class name"));
            m_classInputs[dayIndex][block] = input;
            grid->addWidget(input, dayIndex + 1, block + 1);
        }

        auto *lunch = new QComboBox(this);
        lunch->addItems({QStringLiteral("None"), QStringLiteral("Wave 1"), QStringLiteral("Wave 2"),
                         QStringLiteral("Wave 3"), QStringLiteral("Wave 4")});
        m_lunchInputs[dayIndex] = lunch;
        grid->addWidget(lunch, dayIndex + 1, 5);

    }
    root->addLayout(grid, 1);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();
    auto *saveButton = new QPushButton(QStringLiteral("Save"), this);
    saveButton->setDefault(true);
    saveButton->setMinimumWidth(110);
    connect(saveButton, &QPushButton::clicked, this, &SettingsWindow::saveAndClose);
    buttonRow->addWidget(saveButton);
    root->addLayout(buttonRow);

    loadValues();
}

void SettingsWindow::loadValues()
{
    const QStringList days = {
        QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"), QStringLiteral("D"),
        QStringLiteral("E"), QStringLiteral("F"), QStringLiteral("G"), QStringLiteral("H")
    };
    for (int dayIndex = 0; dayIndex < days.size(); ++dayIndex) {
        const auto &day = days.at(dayIndex);
        for (int block = 0; block < 4; ++block)
            m_classInputs[dayIndex][block]->setText(m_config.className(day, block));
        const auto wave = m_config.lunchWave(day);
        m_lunchInputs[dayIndex]->setCurrentIndex(wave.value_or(0));
    }
}

void SettingsWindow::saveAndClose()
{
    const QStringList days = {
        QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"), QStringLiteral("D"),
        QStringLiteral("E"), QStringLiteral("F"), QStringLiteral("G"), QStringLiteral("H")
    };
    for (int dayIndex = 0; dayIndex < days.size(); ++dayIndex) {
        QStringList classes;
        for (int block = 0; block < 4; ++block)
            classes.append(m_classInputs[dayIndex][block]->text());
        m_config.setClasses(days.at(dayIndex), classes);
        m_config.setLunchWave(days.at(dayIndex), m_lunchInputs[dayIndex]->currentIndex());
    }

    QString error;
    if (!m_config.save({}, &error)) {
        QMessageBox::critical(this, QStringLiteral("Could not save schedule"), error);
        return;
    }
    close();
}

} // namespace jupitr
