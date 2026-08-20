import Foundation

public actor CalendarScraper {
    public static let defaultCalendarURL = URL(
        string: "https://www.darienps.org/district-information/district-calendar"
    )!

    private let calendarURL: URL
    private let dataRoot: URL?
    private var memoryCache: [String: String] = [:]

    public init(
        calendarURL: URL = CalendarScraper.defaultCalendarURL,
        dataRoot: URL? = nil
    ) {
        self.calendarURL = calendarURL
        self.dataRoot = dataRoot
    }

    public func dayType(for date: Date) async -> String? {
        let key = Self.dateKey(for: date)
        if let cached = memoryCache[key] {
            return cached
        }

        if let cached = readCachedDay(forKey: key) {
            memoryCache[key] = cached
            log("Cache hit for \(key): \(cached)")
            return cached
        }

        do {
            var request = URLRequest(url: calendarURL)
            request.setValue(
                "Mozilla/5.0 (Macintosh; Intel Mac OS X) AppleWebKit/537.36 Jupitr/1.0",
                forHTTPHeaderField: "User-Agent"
            )

            log("Fetching calendar for \(key)")
            let (data, response) = try await URLSession.shared.data(for: request)
            if let httpResponse = response as? HTTPURLResponse,
               !(200..<300).contains(httpResponse.statusCode) {
                log("Calendar request returned HTTP \(httpResponse.statusCode)")
                return nil
            }

            let html = String(decoding: data, as: UTF8.self)
            log("Got HTML (\(html.count) characters)")
            guard let result = Self.parseDayType(from: html, targetDate: date) else {
                log("No DHS day type found for \(key)")
                return nil
            }

            memoryCache[key] = result
            writeCachedDay(key: key, value: result)
            log("Parsed day type for \(key): \(result)")
            return result
        } catch {
            log("Calendar request failed: \(error.localizedDescription)")
            return nil
        }
    }

    /// Parses a saved district-calendar page without performing network I/O.
    /// The fixture-driven entry point keeps parser behavior deterministic.
    public nonisolated static func parseDayType(from html: String, targetDate: Date) -> String? {
        let dayBoxPattern = try! NSRegularExpression(
            pattern: #"<div\b[^>]*class\s*=\s*["'][^"']*\bfsCalendarDaybox\b[^"']*["'][^>]*>(.*?)(?=<div\b[^>]*class\s*=\s*["'][^"']*\bfsCalendarDaybox\b|$)"#,
            options: [.caseInsensitive, .dotMatchesLineSeparators]
        )
        let monthPattern = try! NSRegularExpression(
            pattern: #"<span\b[^>]*class\s*=\s*["'][^"']*\bfsCalendarMonth\b[^"']*["'][^>]*>\s*([^<]+?)\s*</span>\s*(\d{1,2})"#,
            options: [.caseInsensitive, .dotMatchesLineSeparators]
        )
        let eventPattern = try! NSRegularExpression(
            pattern: #"<div\b[^>]*class\s*=\s*["'][^"']*\bfsCalendarInfo\b[^"']*["'][^>]*>(.*?)</div>"#,
            options: [.caseInsensitive, .dotMatchesLineSeparators]
        )
        let anchorPattern = try! NSRegularExpression(
            pattern: #"<a\b([^>]*)>(.*?)</a>"#,
            options: [.caseInsensitive, .dotMatchesLineSeparators]
        )
        let titlePattern = try! NSRegularExpression(
            pattern: #"\btitle\s*=\s*["']([^"']+)["']"#,
            options: [.caseInsensitive]
        )

        let targetComponents = Calendar.current.dateComponents([.year, .month, .day], from: targetDate)
        let fullRange = NSRange(html.startIndex..<html.endIndex, in: html)

        for dayBox in dayBoxPattern.matches(in: html, range: fullRange) {
            guard let boxRange = Range(dayBox.range(at: 1), in: html) else { continue }
            let boxHTML = String(html[boxRange])
            let boxRangeNS = NSRange(boxHTML.startIndex..<boxHTML.endIndex, in: boxHTML)
            guard let dateMatch = monthPattern.firstMatch(in: boxHTML, range: boxRangeNS),
                  let monthRange = Range(dateMatch.range(at: 1), in: boxHTML),
                  let dayRange = Range(dateMatch.range(at: 2), in: boxHTML),
                  let day = Int(boxHTML[dayRange]),
                  let month = monthNumber(String(boxHTML[monthRange])) else {
                continue
            }

            guard month == targetComponents.month, day == targetComponents.day else { continue }

            for eventMatch in eventPattern.matches(in: boxHTML, range: boxRangeNS) {
                guard let eventRange = Range(eventMatch.range(at: 1), in: boxHTML) else { continue }
                let eventHTML = String(boxHTML[eventRange])
                let eventText = stripTagsAndDecode(eventHTML)
                guard eventText.localizedCaseInsensitiveContains("DHS School Calendar") else {
                    continue
                }

                let eventNSRange = NSRange(eventHTML.startIndex..<eventHTML.endIndex, in: eventHTML)
                for anchorMatch in anchorPattern.matches(in: eventHTML, range: eventNSRange) {
                    guard let attributesRange = Range(anchorMatch.range(at: 1), in: eventHTML),
                          let visibleRange = Range(anchorMatch.range(at: 2), in: eventHTML) else {
                        continue
                    }

                    let attributes = String(eventHTML[attributesRange])
                    guard attributes.range(of: "fsCalendarEventTitle", options: .caseInsensitive) != nil else {
                        continue
                    }

                    if let titleMatch = titlePattern.firstMatch(in: attributes, range: NSRange(attributes.startIndex..<attributes.endIndex, in: attributes)),
                       let titleRange = Range(titleMatch.range(at: 1), in: attributes) {
                        let title = decodeHTML(String(attributes[titleRange])).trimmingCharacters(in: .whitespacesAndNewlines)
                        if !title.isEmpty { return title }
                    }

                    let visibleText = stripTagsAndDecode(String(eventHTML[visibleRange]))
                    if !visibleText.isEmpty && visibleText.count < 100 {
                        return visibleText
                    }
                }
            }
        }

        return nil
    }

    private var cacheURL: URL {
        if let dataRoot {
            return dataRoot.appendingPathComponent("calendar_cache.txt")
        }
        return AppPaths.calendarCacheURL
    }

    private var logURL: URL {
        if let dataRoot {
            return dataRoot.appendingPathComponent("scraper.log")
        }
        return AppPaths.scraperLogURL
    }

    private func readCachedDay(forKey key: String) -> String? {
        guard let contents = try? String(contentsOf: cacheURL, encoding: .utf8) else { return nil }
        let prefix = "\(key)|"
        return contents
            .split(whereSeparator: { $0.isNewline })
            .first { $0.hasPrefix(prefix) }
            .map { String($0.dropFirst(prefix.count)).trimmingCharacters(in: .whitespacesAndNewlines) }
    }

    private func writeCachedDay(key: String, value: String) {
        do {
            try AppPaths.ensureParentDirectory(for: cacheURL)
            let existing = (try? String(contentsOf: cacheURL, encoding: .utf8)) ?? ""
            let prefix = "\(key)|"
            var lines = existing.split(whereSeparator: { $0.isNewline }).map(String.init)
            lines.removeAll { $0.hasPrefix(prefix) }
            lines.append(prefix + value)
            try (lines.joined(separator: "\n") + "\n").write(to: cacheURL, atomically: true, encoding: .utf8)
        } catch {
            // A cache failure should never make the tray application unusable.
        }
    }

    private func log(_ message: String) {
        do {
            try AppPaths.ensureParentDirectory(for: logURL)
            let formatter = ISO8601DateFormatter()
            formatter.formatOptions = [.withInternetDateTime, .withSpaceBetweenDateAndTime]
            let line = "[\(formatter.string(from: Date()))] \(message)\n"
            if FileManager.default.fileExists(atPath: logURL.path) {
                let handle = try FileHandle(forWritingTo: logURL)
                try handle.seekToEnd()
                try handle.write(contentsOf: Data(line.utf8))
                try handle.close()
            } else {
                try Data(line.utf8).write(to: logURL, options: .atomic)
            }
        } catch {
            // Logging is best-effort and must not surface as an application error.
        }
    }

    private static func dateKey(for date: Date) -> String {
        let components = Calendar.current.dateComponents([.year, .month, .day], from: date)
        return String(format: "%04d-%02d-%02d", components.year ?? 0, components.month ?? 0, components.day ?? 0)
    }

}

private func monthNumber(_ value: String) -> Int? {
    let names = [
        ("January", "Jan"), ("February", "Feb"), ("March", "Mar"),
        ("April", "Apr"), ("May", "May"), ("June", "Jun"),
        ("July", "Jul"), ("August", "Aug"), ("September", "Sep"),
        ("October", "Oct"), ("November", "Nov"), ("December", "Dec")
    ]
    return names.firstIndex {
        $0.0.caseInsensitiveCompare(value.trimmingCharacters(in: .whitespacesAndNewlines)) == .orderedSame ||
        $0.1.caseInsensitiveCompare(value.trimmingCharacters(in: .whitespacesAndNewlines)) == .orderedSame
    }.map { $0 + 1 }
}

private func stripTagsAndDecode(_ value: String) -> String {
    let tagPattern = try! NSRegularExpression(pattern: #"<[^>]*>"#, options: [.dotMatchesLineSeparators])
    let range = NSRange(value.startIndex..<value.endIndex, in: value)
    let stripped = tagPattern.stringByReplacingMatches(in: value, range: range, withTemplate: " ")
    return decodeHTML(stripped).split(whereSeparator: { $0.isWhitespace }).joined(separator: " ")
}

private func decodeHTML(_ value: String) -> String {
    var result = value
        .replacingOccurrences(of: "&amp;", with: "&")
        .replacingOccurrences(of: "&quot;", with: "\"")
        .replacingOccurrences(of: "&#39;", with: "'")
        .replacingOccurrences(of: "&nbsp;", with: " ")
        .replacingOccurrences(of: "&lt;", with: "<")
        .replacingOccurrences(of: "&gt;", with: ">")

    let numericPattern = try! NSRegularExpression(pattern: #"&#(?:x([0-9a-fA-F]+)|([0-9]+));"#)
    let range = NSRange(result.startIndex..<result.endIndex, in: result)
    let matches = numericPattern.matches(in: result, range: range).reversed()
    for match in matches {
        guard let fullRange = Range(match.range, in: result) else { continue }
        var scalarValue: UInt32?
        if let hexRange = Range(match.range(at: 1), in: result) {
            scalarValue = UInt32(result[hexRange], radix: 16)
        } else if let decimalRange = Range(match.range(at: 2), in: result) {
            scalarValue = UInt32(result[decimalRange])
        }
        if let scalarValue, let scalar = UnicodeScalar(scalarValue) {
            result.replaceSubrange(fullRange, with: String(scalar))
        }
    }
    return result
}
