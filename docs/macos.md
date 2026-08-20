# macOS

The macOS implementation is a native Swift 6 menu-bar application. It uses an AppKit `NSStatusItem` and `NSPopover`, with SwiftUI views for the schedule popup and settings editor.

## Requirements

- macOS 13 or newer
- Xcode Command Line Tools or Xcode with Swift 6 support

## Build and test

From the repository root on macOS:

```bash
swift test --package-path macos
bash macos/scripts/build-app.sh
open dist/macos/Jupitr.app
```

The package can also be run directly during development:

```bash
swift run --package-path macos Jupitr
```

The packaged app is an `LSUIElement` utility: it appears in the menu bar and does not create a Dock icon or ordinary main window.

## Configuration, cache, and logs

Jupitr uses native macOS application directories:

```text
~/Library/Application Support/Jupitr/schedule.json
~/Library/Caches/Jupitr/calendar_cache.txt
~/Library/Application Support/Jupitr/scraper.log
```

The schedule file is shared from `shared/schedule/bell-schedule.json` during development and copied into the app bundle during packaging. It is never duplicated as platform-specific schedule data.

## Signing and release builds

The local build is unsigned by default. For a Developer ID build, provide a signing identity:

```bash
CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)" \
  bash macos/scripts/build-app.sh
```

The resulting app can then be notarized using the release account's normal `notarytool` workflow. Credentials are intentionally not stored in this repository.
