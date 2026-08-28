import Foundation

public enum AppPaths {
    public static var applicationSupportDirectory: URL {
        let base = FileManager.default.urls(
            for: .applicationSupportDirectory,
            in: .userDomainMask
        ).first ?? URL(fileURLWithPath: NSHomeDirectory())

        return base.appendingPathComponent("Jupitr", isDirectory: true)
    }

    public static var configURL: URL {
        applicationSupportDirectory.appendingPathComponent("schedule.json")
    }

    public static func ensureParentDirectory(for url: URL) throws {
        try FileManager.default.createDirectory(
            at: url.deletingLastPathComponent(),
            withIntermediateDirectories: true
        )
    }
}
