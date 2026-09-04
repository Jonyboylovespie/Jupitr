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
        XCTAssertEqual(regular[2].startMinute, 10 * 60 + 48)
        XCTAssertEqual(regular[2].endMinute, 12 * 60 + 46)
        XCTAssertEqual(regular[3].startMinute, 12 * 60 + 54)

        let waveFour = schedule.lunchInfo(wave: 4, advisory: false)
        XCTAssertEqual(waveFour?.classSegments.count, 1)
        XCTAssertEqual(waveFour?.classSegments.first?.startMinute, 10 * 60 + 48)
        XCTAssertEqual(waveFour?.classSegments.first?.endMinute, 12 * 60 + 14)

        let waveTwo = schedule.lunchInfo(wave: 2, advisory: false)
        XCTAssertEqual(waveTwo?.classSegments.count, 2)
        XCTAssertEqual(waveTwo?.classSegments.last?.startMinute, 11 * 60 + 50)
        XCTAssertEqual(waveTwo?.classSegments.last?.endMinute, 12 * 60 + 46)

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

    func testAllPeriodsScheduleMapping() throws {
        let schedule = try BellSchedule.load()
        let dayType = "Special \"A\" Day Schedule-All Periods Meet"
        XCTAssertTrue(BellSchedule.isAllPeriodsDay(dayType))
        XCTAssertTrue(BellSchedule.isAllPeriodsDay("First Day of Classes: All periods meet"))

        let blocks = schedule.blocks(for: dayType)
        XCTAssertEqual(blocks.count, 8)
        XCTAssertEqual(blocks[0].name, "Period 2")
        XCTAssertEqual(blocks[4].name, "Period 1")
        XCTAssertEqual(blocks[4].startMinute, 10 * 60 + 40)
        XCTAssertEqual(blocks[7].endMinute, 14 * 60 + 20)

        XCTAssertEqual(BellSchedule.configDayLetter(applicationBlockIndex: 0, dayType: dayType), "A")
        XCTAssertEqual(BellSchedule.configDayLetter(applicationBlockIndex: 4, dayType: dayType), "B")
        XCTAssertEqual(schedule.configBlockIndex(applicationBlockIndex: 0, dayType: dayType), 0)
        XCTAssertEqual(schedule.configBlockIndex(applicationBlockIndex: 4, dayType: dayType), 0)
        XCTAssertEqual(BellSchedule.lunchDayLetter(dayType), "B")

        let mini = schedule.currentMini(at: 12 * 3600 + 35 * 60, dayType: dayType)
        XCTAssertEqual(mini?.mini.name, "M2")
        XCTAssertEqual(mini?.mini.endMinute, 12 * 60 + 50)

        let lunch = schedule.lunchInfo(wave: 4, dayType: dayType)
        XCTAssertEqual(lunch?.startMinute, 11 * 60 + 35)
        XCTAssertEqual(lunch?.endMinute, 12 * 60 + 5)
    }

    func testFormattingAndDayLetters() {
        XCTAssertEqual(BellSchedule.formatRemaining(3723), "1h 2m 3s")
        XCTAssertEqual(BellSchedule.formatRemaining(125), "2m 5s")
        XCTAssertEqual(BellSchedule.formatTime(14 * 60 + 20), "2:20 PM")
        XCTAssertEqual(BellSchedule.extractDayLetter("Wed, Aug 19 · C Day"), "C")
        XCTAssertNil(BellSchedule.extractDayLetter("Unknown"))
    }

    func testOfficialLetterDayCalendar() async throws {
        let data = try SharedResources.calendarData()
        let days = try CalendarScraper.parseDayTypes(from: data)
        XCTAssertEqual(days["2026-08-26"], "A Day - First Day of Classes")
        XCTAssertTrue(BellSchedule.isAllPeriodsDay(days["2026-08-26"] ?? ""))
        XCTAssertEqual(days["2026-08-28"], "C Day - Advisory Schedule")
        XCTAssertEqual(days["2027-06-01"], "G Day")

        let calendar = CalendarScraper()
        let advisoryDay = await calendar.dayType(for: date(2026, 8, 28))
        let noSchoolDay = await calendar.dayType(for: date(2026, 11, 3))
        let outsideCalendar = await calendar.dayType(for: date(2028, 1, 1))
        XCTAssertEqual(advisoryDay, "C Day - Advisory Schedule")
        XCTAssertNil(noSchoolDay)
        XCTAssertNil(outsideCalendar)
    }

    func testConfigurationRoundTrip() throws {
        let directory = try temporaryDirectory()
        let path = directory.appendingPathComponent("schedule.json")

        var config = ScheduleConfig.createDefault()
        config.setClass("AP Physics / Spanish", for: "C", blockIndex: 0)
        config.setClass("English", for: "C", blockIndex: 1)
        config.setLunchWave(4, for: "C")
        config.setClass("Wind/Physics", for: "H", blockIndex: 2)
        config.setClass("Free/Health", for: "A", blockIndex: 2)
        try config.save(to: path)

        let loaded = ScheduleConfig.load(from: path)
        XCTAssertEqual(loaded.className(for: "C", blockIndex: 0), "AP Physics / Spanish")
        XCTAssertEqual(loaded.className(for: "C", blockIndex: 1), "English")
        XCTAssertEqual(loaded.lunchWave(for: "C"), 4)
        XCTAssertEqual(loaded.lunchWave(for: "A"), 1)
        XCTAssertTrue(ScheduleConfig.usesWindPhysicsLunch("Wind/Physics"))
        XCTAssertTrue(ScheduleConfig.usesWindPhysicsLunch(" wind / PHYSICS "))
        XCTAssertFalse(ScheduleConfig.usesWindPhysicsLunch("Free/Health"))
        XCTAssertFalse(ScheduleConfig.usesWindPhysicsLunch("Chemistry/Physics"))
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
