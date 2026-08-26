import Foundation

public struct BellSchedule: Sendable {
    public struct TimeBlock: Hashable, Sendable {
        public let name: String
        public let startMinute: Int
        public let endMinute: Int
        public let minis: [MiniBlock]

        public init(name: String, startMinute: Int, endMinute: Int, minis: [MiniBlock] = []) {
            self.name = name
            self.startMinute = startMinute
            self.endMinute = endMinute
            self.minis = minis
        }
    }

    public struct MiniBlock: Hashable, Sendable {
        public let name: String
        public let startMinute: Int
        public let endMinute: Int
    }

    public struct LunchInfo: Hashable, Sendable {
        public let label: String
        public let startMinute: Int
        public let endMinute: Int
    }

    public struct CurrentBlock: Sendable {
        public let block: TimeBlock?
        public let remainingSeconds: Int
        public let index: Int

        public init(block: TimeBlock?, remainingSeconds: Int, index: Int) {
            self.block = block
            self.remainingSeconds = remainingSeconds
            self.index = index
        }
    }

    public struct CurrentMini: Sendable {
        public let mini: MiniBlock
        public let remainingSeconds: Int
    }

    public enum LoadError: LocalizedError {
        case invalidDocument
        case invalidTime(String)

        public var errorDescription: String? {
            switch self {
            case .invalidDocument:
                return "The shared bell schedule is not a valid JSON document."
            case .invalidTime(let value):
                return "The shared bell schedule contains an invalid time: \(value)."
            }
        }
    }

    private struct RawDocument: Decodable {
        let regular: RawDefinition
        let advisory: RawDefinition
        let allPeriods: RawDefinition
    }

    private struct RawDefinition: Decodable {
        let blocks: [RawBlock]
        let lunchWaves: [String: RawLunch]
    }

    private struct RawBlock: Decodable {
        let name: String
        let start: String
        let end: String
        let minis: [RawMini]
    }

    private struct RawMini: Decodable {
        let name: String
        let start: String
        let end: String
    }

    private struct RawLunch: Decodable {
        let name: String
        let start: String
        let end: String
    }

    private let regular: RawSchedule
    private let advisory: RawSchedule
    private let allPeriods: RawSchedule

    public init(data: Data) throws {
        let decoder = JSONDecoder()
        guard let document = try? decoder.decode(RawDocument.self, from: data) else {
            throw LoadError.invalidDocument
        }
        self.regular = try Self.convert(document.regular)
        self.advisory = try Self.convert(document.advisory)
        self.allPeriods = try Self.convert(document.allPeriods)
    }

    public static func load() throws -> BellSchedule {
        try BellSchedule(data: SharedResources.scheduleData())
    }

    public func blocks(for dayType: String) -> [TimeBlock] {
        definition(for: dayType).blocks
    }

    public func minis(for academicBlockIndex: Int, advisory: Bool) -> [MiniBlock] {
        let blocks = advisory ? self.advisory.blocks : regular.blocks
        var academicIndex = 0
        for block in blocks {
            if block.name.localizedCaseInsensitiveContains("Advisory") {
                continue
            }
            if academicIndex == academicBlockIndex {
                return block.minis
            }
            academicIndex += 1
        }
        return []
    }

    public func minis(forApplicationBlockIndex index: Int, dayType: String) -> [MiniBlock] {
        let blocks = definition(for: dayType).blocks
        guard blocks.indices.contains(index) else { return [] }
        return blocks[index].minis
    }

    public func currentBlock(at secondsSinceMidnight: Int, dayType: String) -> CurrentBlock {
        let blocks = blocks(for: dayType)
        for (index, block) in blocks.enumerated() {
            let start = block.startMinute * 60
            let end = block.endMinute * 60
            if secondsSinceMidnight >= start && secondsSinceMidnight < end {
                return CurrentBlock(
                    block: block,
                    remainingSeconds: max(0, end - secondsSinceMidnight),
                    index: index
                )
            }
        }
        return CurrentBlock(block: nil, remainingSeconds: 0, index: -1)
    }

    public func currentMini(
        at secondsSinceMidnight: Int,
        dayType: String
    ) -> CurrentMini? {
        let blocks = blocks(for: dayType)
        for (applicationIndex, block) in blocks.enumerated() {
            let blockStart = block.startMinute * 60
            let blockEnd = block.endMinute * 60
            guard secondsSinceMidnight >= blockStart && secondsSinceMidnight < blockEnd else {
                continue
            }

            for mini in minis(forApplicationBlockIndex: applicationIndex, dayType: dayType) {
                let start = mini.startMinute * 60
                let end = mini.endMinute * 60
                if secondsSinceMidnight >= start && secondsSinceMidnight < end {
                    return CurrentMini(mini: mini, remainingSeconds: max(0, end - secondsSinceMidnight))
                }
            }
            return nil
        }
        return nil
    }

    public func lunchInfo(wave: Int?, advisory: Bool) -> LunchInfo? {
        guard let wave, (1...4).contains(wave) else { return nil }
        let lunches = (advisory ? self.advisory : regular).lunches
        return lunches[wave]
    }

    public func lunchInfo(wave: Int?, dayType: String) -> LunchInfo? {
        guard let wave, (1...4).contains(wave) else { return nil }
        return definition(for: dayType).lunches[wave]
    }

    public static func isAdvisoryDay(_ dayType: String) -> Bool {
        dayType.localizedCaseInsensitiveContains("Advisory")
    }

    public static func isAllPeriodsDay(_ dayType: String) -> Bool {
        dayType.localizedCaseInsensitiveContains("All periods meet") ||
            dayType.localizedCaseInsensitiveContains("First Day of Classes")
    }

    public static func extractDayLetter(_ dayType: String) -> String? {
        dayLetters.first { dayType.range(of: "\($0) Day", options: .caseInsensitive) != nil }
    }

    public static func configDayLetter(applicationBlockIndex: Int, dayType: String) -> String? {
        if isAllPeriodsDay(dayType) {
            if (0..<4).contains(applicationBlockIndex) { return "A" }
            if (4..<8).contains(applicationBlockIndex) { return "B" }
            return nil
        }
        return extractDayLetter(dayType)
    }

    public static func lunchDayLetter(_ dayType: String) -> String? {
        isAllPeriodsDay(dayType) ? "B" : extractDayLetter(dayType)
    }

    public func configBlockIndex(
        applicationBlockIndex: Int,
        dayType: String
    ) -> Int? {
        let blocks = blocks(for: dayType)
        guard blocks.indices.contains(applicationBlockIndex) else { return nil }
        if Self.isAllPeriodsDay(dayType) {
            return applicationBlockIndex % 4
        }
        guard !blocks[applicationBlockIndex].name.localizedCaseInsensitiveContains("Advisory") else {
            return nil
        }

        let advisoryCount = blocks[..<applicationBlockIndex].filter {
            $0.name.localizedCaseInsensitiveContains("Advisory")
        }.count
        return applicationBlockIndex - advisoryCount
    }

    public static func formatRemaining(_ seconds: Int) -> String {
        let value = max(0, seconds)
        let hours = value / 3600
        let minutes = (value % 3600) / 60
        let remainder = value % 60
        if hours > 0 {
            return "\(hours)h \(minutes)m \(remainder)s"
        }
        if minutes > 0 {
            return "\(minutes)m \(remainder)s"
        }
        return "\(remainder)s"
    }

    public static func formatTime(_ minutesSinceMidnight: Int) -> String {
        let hour24 = max(0, minutesSinceMidnight) / 60
        let minute = max(0, minutesSinceMidnight) % 60
        let suffix = hour24 >= 12 ? "PM" : "AM"
        let hour12 = hour24 % 12 == 0 ? 12 : hour24 % 12
        return String(format: "%d:%02d %@", hour12, minute, suffix)
    }

    private struct RawSchedule: Sendable {
        let blocks: [TimeBlock]
        let lunches: [Int: LunchInfo]
    }

    private func definition(for dayType: String) -> RawSchedule {
        if Self.isAllPeriodsDay(dayType) { return allPeriods }
        return Self.isAdvisoryDay(dayType) ? advisory : regular
    }

    private static func convert(_ definition: RawDefinition) throws -> RawSchedule {
        let blocks = try definition.blocks.map { rawBlock in
            TimeBlock(
                name: rawBlock.name,
                startMinute: try parseTime(rawBlock.start),
                endMinute: try parseTime(rawBlock.end),
                minis: try rawBlock.minis.map { rawMini in
                    MiniBlock(
                        name: rawMini.name,
                        startMinute: try parseTime(rawMini.start),
                        endMinute: try parseTime(rawMini.end)
                    )
                }
            )
        }

        var lunches: [Int: LunchInfo] = [:]
        for (key, rawLunch) in definition.lunchWaves {
            guard let wave = Int(key), (1...4).contains(wave) else { continue }
            lunches[wave] = LunchInfo(
                label: rawLunch.name,
                startMinute: try parseTime(rawLunch.start),
                endMinute: try parseTime(rawLunch.end)
            )
        }
        return RawSchedule(blocks: blocks, lunches: lunches)
    }

    private static func parseTime(_ value: String) throws -> Int {
        let components = value.split(separator: ":")
        guard components.count == 2,
              let hour = Int(components[0]),
              let minute = Int(components[1]),
              (0...23).contains(hour),
              (0...59).contains(minute) else {
            throw LoadError.invalidTime(value)
        }
        return hour * 60 + minute
    }
}

private let dayLetters = ["A", "B", "C", "D", "E", "F", "G", "H"]
