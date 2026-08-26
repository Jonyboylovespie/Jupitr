import Foundation

public struct ScheduleConfig: Codable, Equatable, Sendable {
    public static let dayLetters = ["A", "B", "C", "D", "E", "F", "G", "H"]

    public var classes: [String: [String]]
    public var lunchWaves: [String: Int]
    public var additionalLunchWaves: [String: Int]

    public init(
        classes: [String: [String]] = [:],
        lunchWaves: [String: Int] = [:],
        additionalLunchWaves: [String: Int] = [:]
    ) {
        self.classes = classes
        self.lunchWaves = lunchWaves
        self.additionalLunchWaves = additionalLunchWaves
        ensureDays()
    }

    public func className(for dayLetter: String, blockIndex: Int) -> String {
        guard let values = classes[dayLetter], values.indices.contains(blockIndex) else {
            return ""
        }
        return values[blockIndex]
    }

    public func lunchWave(for dayLetter: String) -> Int? {
        guard let wave = lunchWaves[dayLetter], (1...4).contains(wave) else {
            return nil
        }
        return wave
    }

    public func additionalLunchWave(for dayLetter: String) -> Int? {
        guard Self.supportsAdditionalLunch(className(for: dayLetter, blockIndex: 2)),
              let wave = additionalLunchWaves[dayLetter], (1...4).contains(wave) else {
            return nil
        }
        return wave
    }

    public static func supportsAdditionalLunch(_ blockThreeClass: String) -> Bool {
        blockThreeClass.contains("/") &&
            blockThreeClass.range(of: "Free", options: .caseInsensitive) == nil
    }

    public mutating func setClass(_ value: String, for dayLetter: String, blockIndex: Int) {
        guard (0..<4).contains(blockIndex) else { return }
        var values = classes[dayLetter] ?? Array(repeating: "", count: 4)
        if values.count < 4 {
            values.append(contentsOf: Array(repeating: "", count: 4 - values.count))
        }
        values[blockIndex] = value
        classes[dayLetter] = values
        if blockIndex == 2 && !Self.supportsAdditionalLunch(value) {
            additionalLunchWaves[dayLetter] = 0
        }
    }

    public mutating func setLunchWave(_ wave: Int, for dayLetter: String) {
        lunchWaves[dayLetter] = min(max(wave, 0), 4)
    }

    public mutating func setAdditionalLunchWave(_ wave: Int, for dayLetter: String) {
        additionalLunchWaves[dayLetter] = min(max(wave, 0), 4)
    }

    public static func createDefault() -> ScheduleConfig {
        ScheduleConfig()
    }

    public static func load(from url: URL = AppPaths.configURL) -> ScheduleConfig {
        do {
            let data = try Data(contentsOf: url)
            var config = try JSONDecoder().decode(ScheduleConfig.self, from: data)
            config.ensureDays()
            return config
        } catch {
            return createDefault()
        }
    }

    public func save(to url: URL = AppPaths.configURL) throws {
        var normalized = self
        normalized.ensureDays()
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        let data = try encoder.encode(normalized)
        try AppPaths.ensureParentDirectory(for: url)
        try data.write(to: url, options: .atomic)
    }

    private mutating func ensureDays() {
        for day in Self.dayLetters {
            if classes[day] == nil {
                classes[day] = Array(repeating: "", count: 4)
            } else if var values = classes[day], values.count < 4 {
                values.append(contentsOf: Array(repeating: "", count: 4 - values.count))
                classes[day] = values
            }

            if lunchWaves[day] == nil {
                lunchWaves[day] = 1
            }
            if additionalLunchWaves[day] == nil {
                additionalLunchWaves[day] = 0
            }
        }
    }

    public init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        classes = try container.decodeIfPresent([String: [String]].self, forKey: .classes) ?? [:]
        lunchWaves = try container.decodeIfPresent([String: Int].self, forKey: .lunchWaves) ?? [:]
        additionalLunchWaves = try container.decodeIfPresent([String: Int].self, forKey: .additionalLunchWaves) ?? [:]
        ensureDays()
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(classes, forKey: .classes)
        try container.encode(lunchWaves, forKey: .lunchWaves)
        try container.encode(additionalLunchWaves, forKey: .additionalLunchWaves)
    }

    private enum CodingKeys: String, CodingKey {
        case classes = "Classes"
        case lunchWaves = "LunchWaves"
        case additionalLunchWaves = "AdditionalLunchWaves"
    }
}
