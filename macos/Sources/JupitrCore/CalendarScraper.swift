import Foundation

public actor CalendarScraper {
    private struct CalendarDocument: Decodable {
        let days: [String: String]
    }

    private let days: [String: String]

    public init() {
        days = (try? Self.parseDayTypes(from: SharedResources.calendarData())) ?? [:]
    }

    public func dayType(for date: Date) -> String? {
        days[Self.dateKey(for: date)]
    }

    public nonisolated static func parseDayTypes(from data: Data) throws -> [String: String] {
        try JSONDecoder().decode(CalendarDocument.self, from: data).days
    }

    private nonisolated static func dateKey(for date: Date) -> String {
        let components = Calendar.current.dateComponents([.year, .month, .day], from: date)
        return String(
            format: "%04d-%02d-%02d",
            components.year ?? 0,
            components.month ?? 0,
            components.day ?? 0
        )
    }
}
