import Foundation

public enum AppPaths {
    public static var applicationSupportDirectory: URL {
        let base = FileManager.default.urls(
            for: .applicationSupportDirectory,
            in: .userDomainMask
        ).first ?? URL(fileURLWithPath: NSHomeDirectory())

        return base.appendingPathComponent("Jupitr", isDirectory: true)
    }

    public static var cacheDirectory: URL {
        let base = FileManager.default.urls(
            for: .cachesDirectory,
            in: .userDomainMask
        ).first ?? URL(fileURLWithPath: NSHomeDirectory()).appendingPathComponent("Library/Caches")

        return base.appendingPathComponent("Jupitr", isDirectory: true)
    }

    public static var configURL: URL {
        applicationSupportDirectory.appendingPathComponent("schedule.json")
    }

    public static var calendarCacheURL: URL {
        cacheDirectory.appendingPathComponent("calendar_cache.txt")
    }

    public static var scraperLogURL: URL {
        applicationSupportDirectory.appendingPathComponent("scraper.log")
    }

    public static func ensureParentDirectory(for url: URL) throws {
        try FileManager.default.createDirectory(
            at: url.deletingLastPathComponent(),
            withIntermediateDirectories: true
        )
    }
}
