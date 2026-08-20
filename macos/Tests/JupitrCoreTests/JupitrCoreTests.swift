import Foundation
import XCTest
@testable import JupitrCore

final class JupitrCoreTests: XCTestCase {
    func testSharedScheduleUsesExpectedTimes() throws {
        let schedule = try BellSchedule.load()
        let regular = schedule.blocks(for: "C Day")

        XCTAssertEqual(regular.count, 4)
        XCTAssertEqual(regular[0].startMinute, 7 * 60 + 40)
        XCTAssertEqual(regular[0].endMinute, 9 * 60 + 6)
        XCTAssertEqual(regular[2].startMinute, 11 * 60 + 4)

        let minis = schedule.minis(for: 0, advisory: false)
        XCTAssertEqual(minis.count, 2)
        XCTAssertEqual(minis[1].startMinute, 8 * 60 + 26)
        XCTAssertEqual(minis[1].endMinute, 9 * 60 + 6)
    }

    func testCurrentBlockAndMiniLookup() throws {
        let schedule = try BellSchedule.load()
        let current = schedule.currentBlock(at: 8 * 3600, dayType: "A Day")
        XCTAssertEqual(current.block?.name, "Block 1")
        XCTAssertEqual(current.index, 0)
        XCTAssertEqual(current.remainingSeconds, 66 * 60)

        let mini = schedule.currentMini(at: 11 * 3600 + 10 * 60, dayType: "A Day")
        XCTAssertEqual(mini?.mini.name, "M1")
        XCTAssertEqual(mini?.remainingSeconds, 18 * 60)
    }

    func testAdvisoryMappingAndLunch() throws {
        let schedule = try BellSchedule.load()
        let dayType = "D Day - Advisory Schedule"
        XCTAssertEqual(schedule.blocks(for: dayType).count, 5)
        XCTAssertNil(schedule.configBlockIndex(applicationBlockIndex: 1, dayType: dayType))
        XCTAssertEqual(schedule.configBlockIndex(applicationBlockIndex: 2, dayType: dayType), 1)

        let mini = schedule.currentMini(at: 9 * 3600 + 50 * 60, dayType: dayType)
        XCTAssertEqual(mini?.mini.name, "M1")
        XCTAssertEqual(mini?.mini.endMinute, 10 * 60 + 13)

        let lunch = schedule.lunchInfo(wave: 3, advisory: true)
        XCTAssertEqual(lunch?.startMinute, 11 * 60 + 54)
        XCTAssertEqual(lunch?.endMinute, 12 * 60 + 22)
        XCTAssertNil(schedule.lunchInfo(wave: nil, advisory: false))
    }

    func testFormattingAndDayLetters() {
        XCTAssertEqual(BellSchedule.formatRemaining(3723), "1h 2m 3s")
        XCTAssertEqual(BellSchedule.formatRemaining(125), "2m 5s")
        XCTAssertEqual(BellSchedule.formatTime(14 * 60 + 20), "2:20 PM")
        XCTAssertEqual(BellSchedule.extractDayLetter("Wed, Aug 19 · C Day"), "C")
        XCTAssertNil(BellSchedule.extractDayLetter("Unknown"))
    }

    func testScraperFixtures() throws {
        let normal = try fixture(named: "normal-a-day.html")
        let advisory = try fixture(named: "advisory-day.html")
        let noDHS = try fixture(named: "no-dhs-entry.html")
        let malformed = try fixture(named: "malformed.html")

        XCTAssertEqual(
            CalendarScraper.parseDayType(from: normal, targetDate: date(2026, 8, 19)),
            "C Day"
        )
        XCTAssertEqual(
            CalendarScraper.parseDayType(from: advisory, targetDate: date(2026, 8, 20)),
            "D Day - Advisory Schedule"
        )
        XCTAssertNil(CalendarScraper.parseDayType(from: noDHS, targetDate: date(2026, 8, 21)))
        XCTAssertNil(CalendarScraper.parseDayType(from: malformed, targetDate: date(2026, 8, 19)))
    }

    func testScraperUsesCache() async throws {
        let directory = try temporaryDirectory()
        let cache = directory.appendingPathComponent("calendar_cache.txt")
        try "2026-08-22|Cached C Day\n".write(to: cache, atomically: true, encoding: .utf8)

        let scraper = CalendarScraper(
            calendarURL: URL(string: "http://127.0.0.1:1/")!,
            dataRoot: directory
        )
        let result = await scraper.dayType(for: date(2026, 8, 22))
        XCTAssertEqual(result, "Cached C Day")
    }

    func testConfigurationRoundTrip() throws {
        let directory = try temporaryDirectory()
        let path = directory.appendingPathComponent("schedule.json")

        var config = ScheduleConfig.createDefault()
        config.setClass("AP Physics / Spanish", for: "C", blockIndex: 0)
        config.setClass("English", for: "C", blockIndex: 1)
        config.setLunchWave(4, for: "C")
        try config.save(to: path)

        let loaded = ScheduleConfig.load(from: path)
        XCTAssertEqual(loaded.className(for: "C", blockIndex: 0), "AP Physics / Spanish")
        XCTAssertEqual(loaded.className(for: "C", blockIndex: 1), "English")
        XCTAssertEqual(loaded.lunchWave(for: "C"), 4)
        XCTAssertEqual(loaded.lunchWave(for: "A"), 1)
    }

    private func fixture(named name: String) throws -> String {
        let url = try XCTUnwrap(SharedResources.fixtureURL(named: name))
        return try String(contentsOf: url, encoding: .utf8)
    }

    private func date(_ year: Int, _ month: Int, _ day: Int) -> Date {
        Calendar(identifier: .gregorian).date(from: DateComponents(year: year, month: month, day: day))!
    }

    private func temporaryDirectory() throws -> URL {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("JupitrCoreTests-\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        addTeardownBlock {
            try? FileManager.default.removeItem(at: url)
        }
        return url
    }
}
