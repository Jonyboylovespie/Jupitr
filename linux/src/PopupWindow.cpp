#include "PopupWindow.h"

#include "Theme.h"

#include <QApplication>
#include <QFocusEvent>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace jupitr {

PopupWindow::PopupWindow(ScheduleConfig &config, std::function<void()> openSettings, QWidget *parent)
    : QWidget(parent), m_config(config), m_openSettings(std::move(openSettings))
{
    // A tray activation on Wayland does not carry a Wayland input serial, so
    // a Qt::Popup cannot acquire the required popup grab. A tool window keeps
    // the tray launch reliable; focus/application deactivation below provides
    // the same click-away dismissal behavior.
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_X11NetWmWindowTypeDropDownMenu);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setFixedWidth(380);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("popupCard"));
    card->setStyleSheet(QStringLiteral(
        "#popupCard { background: %1; border: 1px solid %2; border-radius: 16px; }")
            .arg(Theme::darkBackground().name(), Theme::hoverBackground().name()));
    outerLayout->addWidget(card);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(8);

    auto *header = new QHBoxLayout;
    m_dayLabel = new QLabel(QStringLiteral("Loading..."), card);
    m_dayLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: 700; font-size: 15px;").arg(Theme::cream().name()));
    header->addWidget(m_dayLabel);
    header->addStretch();

    auto *settingsButton = new QPushButton(QStringLiteral("⚙"), card);
    settingsButton->setToolTip(QStringLiteral("Edit schedule"));
    settingsButton->setFixedSize(30, 30);
    settingsButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: %1; border: none; font-size: 17px; padding: 0; }"
        "QPushButton:hover { background: %2; border-radius: 5px; }")
            .arg(Theme::muted().name(), Theme::hoverBackground().name()));
    connect(settingsButton, &QPushButton::clicked, this, [this]() {
        hide();
        if (m_openSettings)
            m_openSettings();
    });
    header->addWidget(settingsButton);
    layout->addLayout(header);

    m_timerLabel = new QLabel(QStringLiteral("--:--"), card);
    QFont timerFont(QStringLiteral("Noto Sans"), 28, QFont::Bold);
    m_timerLabel->setFont(timerFont);
    m_timerLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::orange().name()));
    layout->addWidget(m_timerLabel);

    m_subtitleLabel = new QLabel(card);
    m_subtitleLabel->setWordWrap(true);
    m_subtitleLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(Theme::yellow().name()));
    layout->addWidget(m_subtitleLabel);

    auto *separator = new QFrame(card);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Plain);
    separator->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::hoverBackground().name()));
    layout->addWidget(separator);

    auto *scrollArea = new QScrollArea(card);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setMaximumHeight(430);
    scrollArea->setStyleSheet(QStringLiteral("background: transparent;"));

    auto *scheduleWidget = new QWidget(scrollArea);
    scheduleWidget->setStyleSheet(QStringLiteral("background: transparent;"));
    m_scheduleLayout = new QVBoxLayout(scheduleWidget);
    m_scheduleLayout->setContentsMargins(0, 0, 0, 0);
    m_scheduleLayout->setSpacing(2);
    scrollArea->setWidget(scheduleWidget);
    layout->addWidget(scrollArea);

    m_updateTimer.setInterval(1000);
    connect(&m_updateTimer, &QTimer::timeout, this, &PopupWindow::refreshData);
}

void PopupWindow::setDayType(const QString &dayType)
{
    m_dayType = dayType.isEmpty() ? QStringLiteral("Unknown") : dayType;
}

void PopupWindow::refreshData()
{
    const auto now = QTime::currentTime();
    const auto date = QDate::currentDate();
    m_dayLabel->setText(QStringLiteral("%1  ·  %2").arg(date.toString(QStringLiteral("ddd, MMM d")), m_dayType));

    const auto blocks = BellSchedule::blocksForDayType(m_dayType);
    const auto lunchLetter = BellSchedule::lunchDayLetter(m_dayType);
    QVector<LunchPeriod> lunches;
    auto addLunch = [this, &lunches](std::optional<int> wave) {
        if (!wave.has_value() || std::any_of(lunches.cbegin(), lunches.cend(), [wave](const LunchPeriod &period) {
                return period.wave == *wave;
            }))
            return;
        const auto info = BellSchedule::lunchInfo(wave, m_dayType);
        if (info.has_value())
            lunches.append({*info, *wave});
    };
    if (lunchLetter.has_value()) {
        addLunch(m_config.lunchWave(*lunchLetter));
        addLunch(m_config.additionalLunchWave(*lunchLetter));
    }
    const auto current = BellSchedule::currentBlock(now, m_dayType);

    updateMainStatus(now, blocks, current, lunches);

    const auto items = buildScheduleItems(blocks, lunches);
    bool sameItems = items.size() == m_lastItems.size() && m_lastDayType == m_dayType;
    if (sameItems) {
        for (int i = 0; i < items.size(); ++i) {
            const auto &a = items.at(i);
            const auto &b = m_lastItems.at(i);
            if (a.start != b.start || a.end != b.end || a.name != b.name ||
                a.lunch != b.lunch || a.afterLunch != b.afterLunch || a.className != b.className) {
                sameItems = false;
                break;
            }
        }
    }

    if (!sameItems) {
        rebuildSchedule(items, now);
        m_lastItems = items;
        m_lastDayType = m_dayType;
    } else {
        updateScheduleHighlights(items, now);
    }

    adjustSize();
}

void PopupWindow::updateMainStatus(const QTime &now,
                                   const QVector<TimeBlock> &blocks,
                                   const CurrentBlockResult &current,
                                   const QVector<LunchPeriod> &lunches)
{
    auto setStatus = [this](const QString &timer, const QColor &color, const QString &subtitle) {
        m_timerLabel->setText(timer);
        m_timerLabel->setStyleSheet(QStringLiteral("color: %1;").arg(color.name()));
        m_subtitleLabel->setText(subtitle);
    };

    if (current.current.has_value()) {
        const auto &block = *current.current;
        const auto dayLetter = BellSchedule::configDayLetter(current.index, m_dayType);
        const auto configIndex = BellSchedule::configBlockIndex(current.index, m_dayType);
        const auto rawClass = dayLetter.has_value() && configIndex.has_value()
            ? m_config.className(*dayLetter, *configIndex)
            : QString();

        const auto activeLunch = std::find_if(lunches.cbegin(), lunches.cend(), [&now](const LunchPeriod &period) {
            return now >= period.info.start && now < period.info.end;
        });
        if (activeLunch != lunches.cend()) {
            setStatus(BellSchedule::formatRemaining(now.secsTo(activeLunch->info.end)),
                      Theme::yellow(), QStringLiteral("Lunch Wave %1!").arg(activeLunch->wave));
            return;
        }

        const LunchPeriod *nextLunch = nullptr;
        for (const auto &period : lunches) {
            if (period.info.start > now && (!nextLunch || period.info.start < nextLunch->info.start))
                nextLunch = &period;
        }
        if (nextLunch && nextLunch->info.start < block.end) {
            setStatus(BellSchedule::formatRemaining(now.secsTo(nextLunch->info.start)),
                      Theme::yellow(),
                      QStringLiteral("Lunch Wave %1 starts at %2")
                          .arg(nextLunch->wave)
                          .arg(BellSchedule::formatTime(nextLunch->info.start)));
            return;
        }

        const auto mini = BellSchedule::currentMini(now, m_dayType);
        if (mini.has_value() && rawClass.contains(QLatin1Char('/'))) {
            const auto miniName = mini->current->name;
            const auto display = miniClassName(rawClass, miniName);
            setStatus(BellSchedule::formatRemaining(mini->remainingSeconds),
                      mini->remainingSeconds < 5 * 60 ? Theme::yellow() : Theme::orange(),
                      QStringLiteral("Current: %1 (%2) — ends %3")
                          .arg(display.isEmpty() ? miniName : display,
                               miniName,
                               BellSchedule::formatTime(mini->current->end)));
            return;
        }

        const auto display = formattedClassName(rawClass);
        setStatus(BellSchedule::formatRemaining(current.remainingSeconds),
                  current.remainingSeconds < 5 * 60 ? Theme::yellow() : Theme::orange(),
                  QStringLiteral("Current: %1 — ends %2")
                      .arg(display.isEmpty() ? block.name : display,
                           BellSchedule::formatTime(block.end)));
        return;
    }

    const auto activeLunch = std::find_if(lunches.cbegin(), lunches.cend(), [&now](const LunchPeriod &period) {
        return now >= period.info.start && now < period.info.end;
    });
    if (activeLunch != lunches.cend()) {
        setStatus(BellSchedule::formatRemaining(now.secsTo(activeLunch->info.end)),
                  Theme::yellow(), QStringLiteral("Lunch Wave %1!").arg(activeLunch->wave));
    } else if (!blocks.isEmpty() && now < blocks.first().start) {
        setStatus(BellSchedule::formatRemaining(now.secsTo(blocks.first().start)),
                  Theme::muted(),
                  QStringLiteral("%1 starts at %2")
                      .arg(blocks.first().name, BellSchedule::formatTime(blocks.first().start)));
    } else {
        setStatus(QStringLiteral("Done"), Theme::muted(), QStringLiteral("School is over"));
    }
}

QVector<PopupWindow::ScheduleItem> PopupWindow::buildScheduleItems(
    const QVector<TimeBlock> &blocks,
    const QVector<LunchPeriod> &lunches) const
{
    QVector<ScheduleItem> items;

    for (int applicationIndex = 0; applicationIndex < blocks.size(); ++applicationIndex) {
        const auto &block = blocks.at(applicationIndex);
        const auto configIndex = BellSchedule::configBlockIndex(applicationIndex, m_dayType);
        const auto dayLetter = BellSchedule::configDayLetter(applicationIndex, m_dayType);
        const auto rawClass = configIndex.has_value() && dayLetter.has_value()
            ? m_config.className(*dayLetter, *configIndex)
            : QString();
        const bool hasMinis = !rawClass.trimmed().isEmpty() && rawClass.contains(QLatin1Char('/'));

        const auto minis = configIndex.has_value()
            ? BellSchedule::minisForApplicationBlock(applicationIndex, m_dayType)
            : QVector<MiniBlock>();
        if (hasMinis && minis.size() >= 2) {
            const auto parts = rawClass.split(QLatin1Char('/'));
            for (int miniIndex = 0; miniIndex < minis.size(); ++miniIndex) {
                const auto &mini = minis.at(miniIndex);
                items.append({mini.start, mini.end,
                              QStringLiteral("%1 (%2)").arg(block.name, mini.name),
                              false, false, parts.value(miniIndex).trimmed()});
            }
        } else {
            items.append({block.start, block.end, block.name, false, false, rawClass});
        }
    }

    for (const auto &lunch : lunches) {
        QVector<ScheduleItem> adjusted;
        for (const auto &item : std::as_const(items)) {
            if (item.lunch || item.end <= lunch.info.start || item.start >= lunch.info.end) {
                adjusted.append(item);
                continue;
            }
            if (item.start < lunch.info.start) {
                auto before = item;
                before.end = lunch.info.start;
                adjusted.append(before);
            }
            if (item.end > lunch.info.end) {
                auto after = item;
                after.start = lunch.info.end;
                after.afterLunch = true;
                adjusted.append(after);
            }
        }
        adjusted.append({lunch.info.start, lunch.info.end,
                         QStringLiteral("Lunch Wave %1").arg(lunch.wave), true, false, {}});
        items = std::move(adjusted);
    }

    std::sort(items.begin(), items.end(), [](const ScheduleItem &a, const ScheduleItem &b) {
        if (a.start != b.start)
            return a.start < b.start;
        return a.lunch && !b.lunch;
    });

    return items;
}

void PopupWindow::rebuildSchedule(const QVector<ScheduleItem> &items, const QTime &now)
{
    clearScheduleRows();
    for (int i = 0; i < items.size(); ++i) {
        const auto &item = items.at(i);
        const bool current = now >= item.start && now < item.end;
        const bool advisory = item.name.contains(QStringLiteral("Advisory"), Qt::CaseInsensitive);
        m_scheduleLayout->addWidget(createScheduleRow(item, current, advisory, i % 2 == 0));
    }
    m_scheduleLayout->addStretch();
}

void PopupWindow::updateScheduleHighlights(const QVector<ScheduleItem> &items, const QTime &now)
{
    for (int i = 0; i < items.size(); ++i) {
        auto *layoutItem = m_scheduleLayout->itemAt(i);
        auto *row = layoutItem ? qobject_cast<QFrame *>(layoutItem->widget()) : nullptr;
        if (!row)
            continue;

        const auto &item = items.at(i);
        const bool current = now >= item.start && now < item.end;
        const bool evenRow = i % 2 == 0;
        const auto background = current ? Theme::hoverBackground()
                                       : (evenRow ? Theme::cardBackground() : Theme::alternateCardBackground());
        row->setStyleSheet(QStringLiteral("QFrame { background: %1; border-radius: 5px; }").arg(background.name()));

        auto *rowLayout = qobject_cast<QHBoxLayout *>(row->layout());
        auto *name = rowLayout && rowLayout->count() > 0
            ? qobject_cast<QLabel *>(rowLayout->itemAt(0)->widget())
            : nullptr;
        if (name) {
            const bool advisory = item.name.contains(QStringLiteral("Advisory"), Qt::CaseInsensitive);
            name->setStyleSheet(QStringLiteral("color: %1; font-weight: %2;")
                                     .arg(item.lunch ? Theme::yellow().name()
                                                     : (advisory ? Theme::muted().name()
                                                                 : (current ? Theme::cream().name() : Theme::muted().name())))
                                     .arg(current ? QStringLiteral("700") : QStringLiteral("400")));
        }
    }
}

void PopupWindow::clearScheduleRows()
{
    while (auto *item = m_scheduleLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

QWidget *PopupWindow::createScheduleRow(const ScheduleItem &item, bool current, bool advisory, bool evenRow) const
{
    auto *row = new QFrame;
    row->setFixedHeight(item.lunch ? 29 : 37);
    const auto background = current ? Theme::hoverBackground()
                                   : (evenRow ? Theme::cardBackground() : Theme::alternateCardBackground());
    row->setStyleSheet(QStringLiteral("QFrame { background: %1; border-radius: 5px; }").arg(background.name()));

    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(current ? 12 : 9, 2, 8, 2);
    layout->setSpacing(6);

    auto *name = new QLabel(item.lunch ? item.name : (item.afterLunch ? item.name + QStringLiteral(" (cont.)") : item.name), row);
    name->setMinimumWidth(110);
    name->setStyleSheet(QStringLiteral("color: %1; font-weight: %2;")
                            .arg(item.lunch ? Theme::yellow().name()
                                            : (advisory ? Theme::muted().name()
                                                        : (current ? Theme::cream().name() : Theme::muted().name())))
                            .arg(current ? QStringLiteral("700") : QStringLiteral("400")));
    layout->addWidget(name);

    auto *time = new QLabel(QStringLiteral("%1 – %2")
                                .arg(BellSchedule::formatTime(item.start), BellSchedule::formatTime(item.end)), row);
    time->setMinimumWidth(108);
    time->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(item.lunch ? Theme::yellowLight().name() : Theme::muted().name()));
    layout->addWidget(time);

    if (!item.lunch && !advisory && !item.className.trimmed().isEmpty()) {
        auto *classLabel = new QLabel(item.className, row);
        classLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        classLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(current ? Theme::yellow().name() : Theme::cream().name()));
        layout->addWidget(classLabel, 1);
    } else {
        layout->addStretch(1);
    }

    return row;
}

QString PopupWindow::classForBlock(int applicationBlockIndex, const QString &dayType) const
{
    const auto letter = BellSchedule::configDayLetter(applicationBlockIndex, dayType);
    const auto index = BellSchedule::configBlockIndex(applicationBlockIndex, dayType);
    return letter.has_value() && index.has_value() ? m_config.className(*letter, *index) : QString();
}

QString PopupWindow::miniClassName(const QString &rawName, const QString &miniLabel)
{
    const auto parts = rawName.split(QLatin1Char('/'));
    if (miniLabel == QStringLiteral("M1"))
        return parts.value(0).trimmed();
    if (miniLabel == QStringLiteral("M2"))
        return parts.value(1).trimmed();
    return {};
}

QString PopupWindow::formattedClassName(const QString &rawName)
{
    if (rawName.trimmed().isEmpty())
        return {};
    if (!rawName.contains(QLatin1Char('/')))
        return rawName;

    const auto parts = rawName.split(QLatin1Char('/'));
    const auto first = parts.value(0).trimmed();
    const auto second = parts.value(1).trimmed();
    if (first.isEmpty() && second.isEmpty())
        return QStringLiteral("—");
    if (first.isEmpty())
        return QStringLiteral("M2: %1").arg(second);
    if (second.isEmpty())
        return QStringLiteral("M1: %1").arg(first);
    return QStringLiteral("%1 · %2").arg(first, second);
}

void PopupWindow::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    // A child control (notably the settings button) can receive focus between
    // mouse press and release. Wait for that transfer to settle so hiding the
    // popup does not swallow the child's click.
    QTimer::singleShot(0, this, [this]() {
        if (!isVisible())
            return;
        QWidget *focused = QApplication::focusWidget();
        if (focused && (focused == this || isAncestorOf(focused)))
            return;
        hide();
    });
}

void PopupWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_updateTimer.start();
    refreshData();
}

void PopupWindow::hideEvent(QHideEvent *event)
{
    m_updateTimer.stop();
    QWidget::hideEvent(event);
}

} // namespace jupitr
