import Combine
import Foundation
import JupitrCore

enum StatusTone {
    case orange
    case yellow
    case muted
}

struct ScheduleStatus {
    let timer: String
    let subtitle: String
    let tone: StatusTone
}

struct ScheduleItem: Equatable, Identifiable {
    let startMinute: Int
    let endMinute: Int
    let name: String
    let className: String
    let isLunch: Bool
    let isAfterLunch: Bool
    let isMini: Bool
    let isCurrent: Bool

    var id: String {
        "\(startMinute)-\(endMinute)-\(name)-\(className)-\(isLunch)-\(isAfterLunch)-\(isMini)"
    }
}

private struct ConfiguredLunch {
    let info: BellSchedule.LunchInfo
    let wave: Int
}

@MainActor
final class JupitrModel: NSObject, ObservableObject {
    let schedule: BellSchedule

    @Published private(set) var now = Date()
    @Published private(set) var dayType = "Loading…"
    @Published var config: ScheduleConfig

    private let scraper: CalendarScraper
    private var updateTimer: Timer?
    private var dayRefreshTask: Task<Void, Never>?
    private var checkedDateKey: String?
    private var attemptedDateKey: String?
    private var popupVisible = false

    init(schedule: BellSchedule? = nil, scraper: CalendarScraper = CalendarScraper()) {
        do {
            if let schedule {
                self.schedule = schedule
            } else {
                self.schedule = try BellSchedule.load()
            }
        } catch {
            fatalError("Jupitr could not load shared/schedule/bell-schedule.json: \(error.localizedDescription)")
        }

        self.scraper = scraper
        self.config = ScheduleConfig.load()
        super.init()

        if !FileManager.default.fileExists(atPath: AppPaths.configURL.path) {
            try? config.save()
        }

        restartTimer()
        refreshDayTypeIfNeeded(force: true)
    }

    deinit {
        updateTimer?.invalidate()
        dayRefreshTask?.cancel()
    }

    var headerText: String {
        "\(Self.headerFormatter.string(from: now))  ·  \(dayType)"
    }

    var status: ScheduleStatus {
        guard BellSchedule.extractDayLetter(dayType) != nil || BellSchedule.isAllPeriodsDay(dayType) else {
            return ScheduleStatus(
                timer: "—",
                subtitle: dayType == "Loading…" ? "Checking the DHS calendar…" : "Schedule unavailable",
                tone: .muted
            )
        }

        let seconds = secondsSinceMidnight(now)
        let blocks = schedule.blocks(for: dayType)
        let lunches = configuredLunches(for: dayType)
        let current = schedule.currentBlock(at: seconds, dayType: dayType)

        if let block = current.block {
            let rawClass = className(forApplicationBlockIndex: current.index, dayType: dayType)

            if let lunch = lunches.first(where: {
                isWithin(seconds, startMinute: $0.info.startMinute, endMinute: $0.info.endMinute)
            }) {
                let lunchName = lunch.wave == 0 ? "Lunch" : "Lunch Wave \(lunch.wave)"
                return ScheduleStatus(
                    timer: BellSchedule.formatRemaining(lunch.info.endMinute * 60 - seconds),
                    subtitle: "\(lunchName)!",
                    tone: .yellow
                )
            }

            if let lunch = lunches
                .filter({ $0.info.startMinute * 60 > seconds && $0.info.startMinute < block.endMinute })
                .min(by: { $0.info.startMinute < $1.info.startMinute }) {
                let lunchName = lunch.wave == 0 ? "Lunch" : "Lunch Wave \(lunch.wave)"
                return ScheduleStatus(
                    timer: BellSchedule.formatRemaining(lunch.info.startMinute * 60 - seconds),
                    subtitle: "\(lunchName) starts at \(BellSchedule.formatTime(lunch.info.startMinute))",
                    tone: .yellow
                )
            }

            if !BellSchedule.isAllPeriodsDay(dayType),
               schedule.configBlockIndex(applicationBlockIndex: current.index, dayType: dayType) == 2,
               ScheduleConfig.usesWindPhysicsLunch(rawClass) {
                let minis = schedule.minis(forApplicationBlockIndex: current.index, dayType: dayType)
                if minis.count >= 2 {
                    let classParts = splitMiniClasses(rawClass)
                    let physics = classParts[safe: 1] ?? "Physics"
                    if isWithin(seconds, startMinute: minis[1].startMinute, endMinute: minis[1].endMinute) {
                        let remaining = minis[1].endMinute * 60 - seconds
                        return ScheduleStatus(
                            timer: BellSchedule.formatRemaining(remaining),
                            subtitle: "Current: \(physics) (\(minis[1].name)) — ends \(BellSchedule.formatTime(minis[1].endMinute))",
                            tone: remaining < 5 * 60 ? .yellow : .orange
                        )
                    }

                    if seconds >= minis[0].endMinute * 60 && seconds < minis[1].startMinute * 60 {
                        return ScheduleStatus(
                            timer: BellSchedule.formatRemaining(minis[1].startMinute * 60 - seconds),
                            subtitle: "\(physics) (\(minis[1].name)) starts at \(BellSchedule.formatTime(minis[1].startMinute))",
                            tone: .muted
                        )
                    }
                }

                if let waveFour = schedule.lunchInfo(wave: 4, dayType: dayType),
                   isWithin(seconds, startMinute: waveFour.startMinute, endMinute: waveFour.endMinute) {
                    let wind = splitMiniClasses(rawClass)[safe: 0] ?? "Wind"
                    let remaining = waveFour.endMinute * 60 - seconds
                    return ScheduleStatus(
                        timer: BellSchedule.formatRemaining(remaining),
                        subtitle: "Current: \(wind) — ends \(BellSchedule.formatTime(waveFour.endMinute))",
                        tone: remaining < 5 * 60 ? .yellow : .orange
                    )
                }
            }

            // Preserve the existing behavior: when a configured class is
            // written as "Class 1 / Class 2", the active mini owns the timer.
            if rawClass.contains("/"),
               let mini = schedule.currentMini(at: seconds, dayType: dayType) {
                let display = miniClassName(rawClass, miniName: mini.mini.name)
                return ScheduleStatus(
                    timer: BellSchedule.formatRemaining(mini.remainingSeconds),
                    subtitle: "Current: \(display.isEmpty ? mini.mini.name : display) (\(mini.mini.name)) — ends \(BellSchedule.formatTime(mini.mini.endMinute))",
                    tone: mini.remainingSeconds < 5 * 60 ? .yellow : .orange
                )
            }

            let display = formattedClassName(rawClass)
            return ScheduleStatus(
                timer: BellSchedule.formatRemaining(current.remainingSeconds),
                subtitle: "Current: \(display.isEmpty ? block.name : display) — ends \(BellSchedule.formatTime(block.endMinute))",
                tone: current.remainingSeconds < 5 * 60 ? .yellow : .orange
            )
        }

        if let lunch = lunches.first(where: {
            isWithin(seconds, startMinute: $0.info.startMinute, endMinute: $0.info.endMinute)
        }) {
            let lunchName = lunch.wave == 0 ? "Lunch" : "Lunch Wave \(lunch.wave)"
            return ScheduleStatus(
                timer: BellSchedule.formatRemaining(lunch.info.endMinute * 60 - seconds),
                subtitle: "\(lunchName)!",
                tone: .yellow
            )
        }

        if let firstBlock = blocks.first, seconds < firstBlock.startMinute * 60 {
            return ScheduleStatus(
                timer: BellSchedule.formatRemaining(firstBlock.startMinute * 60 - seconds),
                subtitle: "\(firstBlock.name) starts at \(BellSchedule.formatTime(firstBlock.startMinute))",
                tone: .muted
            )
        }

        return ScheduleStatus(timer: "Done", subtitle: "School is over", tone: .muted)
    }

    var scheduleItems: [ScheduleItem] {
        guard BellSchedule.extractDayLetter(dayType) != nil || BellSchedule.isAllPeriodsDay(dayType) else { return [] }

        let seconds = secondsSinceMidnight(now)
        let blocks = schedule.blocks(for: dayType)
        let lunches = configuredLunches(for: dayType)
        var items: [ScheduleItem] = []

        for (applicationIndex, block) in blocks.enumerated() {
            let configIndex = schedule.configBlockIndex(
                applicationBlockIndex: applicationIndex,
                dayType: dayType
            )
            let dayLetter = BellSchedule.configDayLetter(
                applicationBlockIndex: applicationIndex,
                dayType: dayType
            )
            let rawClass = configIndex.flatMap { index in
                dayLetter.map { config.className(for: $0, blockIndex: index) }
            } ?? ""
            let hasMinis = !rawClass.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty && rawClass.contains("/")

            let minis = configIndex.map { _ in
                schedule.minis(forApplicationBlockIndex: applicationIndex, dayType: dayType)
            } ?? []
            if !BellSchedule.isAllPeriodsDay(dayType),
               ScheduleConfig.usesWindPhysicsLunch(rawClass), configIndex == 2,
               minis.count >= 2,
               let waveFour = schedule.lunchInfo(wave: 4, dayType: dayType) {
                let classParts = splitMiniClasses(rawClass)
                items.append(ScheduleItem(
                    startMinute: minis[1].startMinute,
                    endMinute: minis[1].endMinute,
                    name: "\(block.name) (\(minis[1].name))",
                    className: classParts[safe: 1] ?? "",
                    isLunch: false,
                    isAfterLunch: false,
                    isMini: true,
                    isCurrent: isWithin(seconds, startMinute: minis[1].startMinute, endMinute: minis[1].endMinute)
                ))
                items.append(ScheduleItem(
                    startMinute: waveFour.startMinute,
                    endMinute: waveFour.endMinute,
                    name: "\(block.name) (Wave 4)",
                    className: classParts[safe: 0] ?? "",
                    isLunch: false,
                    isAfterLunch: false,
                    isMini: false,
                    isCurrent: isWithin(seconds, startMinute: waveFour.startMinute, endMinute: waveFour.endMinute)
                ))
            } else if hasMinis, !minis.isEmpty {
                let classParts = splitMiniClasses(rawClass)
                for (miniIndex, mini) in minis.enumerated() {
                    items.append(ScheduleItem(
                        startMinute: mini.startMinute,
                        endMinute: mini.endMinute,
                        name: "\(block.name) (\(mini.name))",
                        className: classParts[safe: miniIndex] ?? "",
                        isLunch: false,
                        isAfterLunch: false,
                        isMini: true,
                        isCurrent: isWithin(seconds, startMinute: mini.startMinute, endMinute: mini.endMinute)
                    ))
                }
            } else {
                items.append(ScheduleItem(
                    startMinute: block.startMinute,
                    endMinute: block.endMinute,
                    name: block.name,
                    className: rawClass,
                    isLunch: false,
                    isAfterLunch: false,
                    isMini: false,
                    isCurrent: isWithin(seconds, startMinute: block.startMinute, endMinute: block.endMinute)
                ))
            }
        }

        for lunch in lunches {
            var adjusted: [ScheduleItem] = []
            for item in items {
                if item.isLunch || item.endMinute <= lunch.info.startMinute || item.startMinute >= lunch.info.endMinute {
                    adjusted.append(item)
                    continue
                }
                if !item.isMini, !lunch.info.classSegments.isEmpty {
                    for segment in lunch.info.classSegments {
                        let startMinute = max(item.startMinute, segment.startMinute)
                        let endMinute = min(item.endMinute, segment.endMinute)
                        guard startMinute < endMinute else { continue }

                        adjusted.append(ScheduleItem(
                            startMinute: startMinute,
                            endMinute: endMinute,
                            name: item.name,
                            className: item.className,
                            isLunch: false,
                            isAfterLunch: item.startMinute < lunch.info.startMinute && startMinute >= lunch.info.endMinute,
                            isMini: false,
                            isCurrent: isWithin(seconds, startMinute: startMinute, endMinute: endMinute)
                        ))
                    }
                    continue
                }
                if item.startMinute < lunch.info.startMinute {
                    adjusted.append(ScheduleItem(
                        startMinute: item.startMinute,
                        endMinute: lunch.info.startMinute,
                        name: item.name,
                        className: item.className,
                        isLunch: false,
                        isAfterLunch: item.isAfterLunch,
                        isMini: item.isMini,
                        isCurrent: isWithin(seconds, startMinute: item.startMinute, endMinute: lunch.info.startMinute)
                    ))
                }
                if item.endMinute > lunch.info.endMinute {
                    adjusted.append(ScheduleItem(
                        startMinute: lunch.info.endMinute,
                        endMinute: item.endMinute,
                        name: item.name,
                        className: item.className,
                        isLunch: false,
                        isAfterLunch: true,
                        isMini: item.isMini,
                        isCurrent: isWithin(seconds, startMinute: lunch.info.endMinute, endMinute: item.endMinute)
                    ))
                }
            }
            adjusted.append(ScheduleItem(
                startMinute: lunch.info.startMinute,
                endMinute: lunch.info.endMinute,
                name: lunch.wave == 0 ? "Lunch" : "Lunch Wave \(lunch.wave)",
                className: "",
                isLunch: true,
                isAfterLunch: false,
                isMini: false,
                isCurrent: isWithin(seconds, startMinute: lunch.info.startMinute, endMinute: lunch.info.endMinute)
            ))
            items = adjusted
        }

        return items.sorted {
            $0.startMinute == $1.startMinute ? ($0.isLunch && !$1.isLunch) : $0.startMinute < $1.startMinute
        }
    }

    var tooltipText: String {
        let currentStatus = status
        return currentStatus.subtitle == "School is over"
            ? "Jupitr — School is over"
            : "Jupitr — \(currentStatus.subtitle) (\(currentStatus.timer) left)"
    }

    @discardableResult
    func saveConfig(_ newConfig: ScheduleConfig) -> String? {
        do {
            try newConfig.save()
            config = newConfig
            return nil
        } catch {
            return error.localizedDescription
        }
    }

    func setPopupVisible(_ visible: Bool) {
        guard popupVisible != visible else { return }
        popupVisible = visible
        now = Date()
        if visible {
            let key = Self.dateKey(for: now)
            if checkedDateKey != key {
                refreshDayTypeIfNeeded(force: true)
            }
        }
        restartTimer()
    }

    private func restartTimer() {
        updateTimer?.invalidate()
        let interval: TimeInterval = popupVisible ? 1 : 30
        let timer = Timer(timeInterval: interval, target: self, selector: #selector(tick), userInfo: nil, repeats: true)
        updateTimer = timer
        RunLoop.main.add(timer, forMode: .common)
    }

    @objc private func tick() {
        now = Date()
        refreshDayTypeIfNeeded()
    }

    private func refreshDayTypeIfNeeded(force: Bool = false) {
        let date = Date()
        let key = Self.dateKey(for: date)
        if !force {
            guard key != checkedDateKey, key != attemptedDateKey, dayRefreshTask == nil else { return }
        } else {
            dayRefreshTask?.cancel()
            dayRefreshTask = nil
        }

        attemptedDateKey = key
        if dayType != "Unknown" {
            dayType = "Loading…"
        }
        let scraper = scraper
        dayRefreshTask = Task { [weak self] in
            let result = await scraper.dayType(for: date)
            guard !Task.isCancelled else { return }
            self?.receiveDayType(result, forKey: key)
        }
    }

    private func receiveDayType(_ result: String?, forKey key: String) {
        dayRefreshTask = nil
        guard key == Self.dateKey(for: Date()) else {
            refreshDayTypeIfNeeded(force: true)
            return
        }

        if let result {
            dayType = result
            checkedDateKey = key
        } else {
            dayType = "Unknown"
        }
    }

    private func configuredLunches(for dayType: String) -> [ConfiguredLunch] {
        guard let dayLetter = BellSchedule.lunchDayLetter(dayType) else { return [] }
        let blockThreeClass = config.className(for: dayLetter, blockIndex: 2)
        if !BellSchedule.isAllPeriodsDay(dayType),
           ScheduleConfig.usesWindPhysicsLunch(blockThreeClass) {
            let blocks = schedule.blocks(for: dayType)
            for applicationIndex in blocks.indices {
                guard BellSchedule.configDayLetter(applicationBlockIndex: applicationIndex, dayType: dayType) == dayLetter,
                      schedule.configBlockIndex(applicationBlockIndex: applicationIndex, dayType: dayType) == 2 else {
                    continue
                }

                let minis = schedule.minis(forApplicationBlockIndex: applicationIndex, dayType: dayType)
                guard let firstMini = minis.first else { return [] }
                return [ConfiguredLunch(
                    info: BellSchedule.LunchInfo(
                        label: "Lunch",
                        startMinute: firstMini.startMinute,
                        endMinute: firstMini.endMinute
                    ),
                    wave: 0
                )]
            }
            return []
        }

        guard let wave = config.lunchWave(for: dayLetter),
              let info = schedule.lunchInfo(wave: wave, dayType: dayType) else { return [] }
        return [ConfiguredLunch(info: info, wave: wave)]
    }

    private func className(forApplicationBlockIndex index: Int, dayType: String) -> String {
        guard let letter = BellSchedule.configDayLetter(applicationBlockIndex: index, dayType: dayType),
              let configIndex = schedule.configBlockIndex(
                applicationBlockIndex: index,
                dayType: dayType
              ) else {
            return ""
        }
        return config.className(for: letter, blockIndex: configIndex)
    }

    private func secondsSinceMidnight(_ date: Date) -> Int {
        let components = Calendar.current.dateComponents([.hour, .minute, .second], from: date)
        return (components.hour ?? 0) * 3600 + (components.minute ?? 0) * 60 + (components.second ?? 0)
    }

    private func isWithin(_ seconds: Int, startMinute: Int, endMinute: Int) -> Bool {
        seconds >= startMinute * 60 && seconds < endMinute * 60
    }

    private func splitMiniClasses(_ value: String) -> [String] {
        value.split(separator: "/", omittingEmptySubsequences: false)
            .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }
    }

    private func miniClassName(_ rawName: String, miniName: String) -> String {
        let parts = splitMiniClasses(rawName)
        switch miniName {
        case "M1": return parts[safe: 0] ?? ""
        case "M2": return parts[safe: 1] ?? ""
        default: return ""
        }
    }

    private func formattedClassName(_ rawName: String) -> String {
        let trimmed = rawName.trimmingCharacters(in: .whitespacesAndNewlines)
        guard trimmed.contains("/") else { return trimmed }
        let parts = splitMiniClasses(trimmed)
        let first = parts[safe: 0] ?? ""
        let second = parts[safe: 1] ?? ""
        if first.isEmpty && second.isEmpty { return "—" }
        if first.isEmpty { return "M2: \(second)" }
        if second.isEmpty { return "M1: \(first)" }
        return "\(first) · \(second)"
    }

    private static func dateKey(for date: Date) -> String {
        let components = Calendar.current.dateComponents([.year, .month, .day], from: date)
        return String(format: "%04d-%02d-%02d", components.year ?? 0, components.month ?? 0, components.day ?? 0)
    }

    private static let headerFormatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.dateFormat = "EEE, MMM d"
        return formatter
    }()
}

private extension Array {
    subscript(safe index: Index) -> Element? {
        indices.contains(index) ? self[index] : nil
    }
}
