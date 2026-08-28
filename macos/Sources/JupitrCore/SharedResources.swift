import Foundation

public enum SharedResourceError: LocalizedError {
    case missing(String)

    public var errorDescription: String? {
        switch self {
        case .missing(let name):
            return "Could not find the shared Jupitr resource: \(name)"
        }
    }
}

/// Resolves resources from the packaged app first, then from the repository
/// checkout. The latter keeps `swift run` and fixture tests convenient without
/// copying the authoritative files into the macOS target.
public enum SharedResources {
    public static func scheduleData() throws -> Data {
        try data(named: "bell-schedule", fileExtension: "json")
    }

    public static func calendarData() throws -> Data {
        try data(named: "letter-day-calendar", fileExtension: "json")
    }

    public static func logoURL() -> URL? {
        url(named: "jupitr", fileExtension: "svg")
    }

    public static func data(named name: String, fileExtension: String) throws -> Data {
        guard let resourceURL = url(named: name, fileExtension: fileExtension),
              let data = try? Data(contentsOf: resourceURL) else {
            throw SharedResourceError.missing("\(name).\(fileExtension)")
        }
        return data
    }

    public static func url(named name: String, fileExtension: String) -> URL? {
        let bundleCandidates: [URL?] = [
            Bundle.main.url(forResource: name, withExtension: fileExtension),
            Bundle.main.resourceURL?.appendingPathComponent("shared/schedule/\(name).\(fileExtension)"),
            Bundle.main.resourceURL?.appendingPathComponent("\(name).\(fileExtension)")
        ]

        let candidates = bundleCandidates.compactMap { $0 }
            + repositoryCandidates(relativePath: "shared/schedule/\(name).\(fileExtension)")
            + [
                URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
                    .appendingPathComponent("shared/schedule/\(name).\(fileExtension)")
            ]

        return firstExistingURL(candidates)
    }

    private static func repositoryCandidates(relativePath: String) -> [URL] {
        // This source-file-relative fallback is only for an unpackaged
        // checkout. The packaged app resolves from Contents/Resources first.
        let sourceDirectory = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent() // JupitrCore
            .deletingLastPathComponent() // Sources
            .deletingLastPathComponent() // macos
            .deletingLastPathComponent() // repository root

        return [
            sourceDirectory.appendingPathComponent(relativePath),
            sourceDirectory.deletingLastPathComponent().appendingPathComponent(relativePath)
        ]
    }

    private static func firstExistingURL(_ candidates: [URL]) -> URL? {
        let fileManager = FileManager.default
        return candidates.first { fileManager.fileExists(atPath: $0.path) }
    }
}
