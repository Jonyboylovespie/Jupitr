# Architecture

Jupitr has three native UI implementations and one authoritative set of platform-neutral resources.

```text
District calendar
        ↓
     Scraper ───────→ date cache and log
        ↓
     Day type
        ↓
Schedule engine ← user configuration
        ↓
   Tray / popup / settings UI
```

The Windows implementation is C#/.NET 8/WinForms under `windows/`. The Linux implementation is C++20/Qt 6 under `linux/`. The macOS implementation is Swift/AppKit/SwiftUI under `macos/`. They do not share source code; this avoids forcing any platform into another platform's UI or lifecycle model.

The schedule engine in each implementation reads `shared/schedule/bell-schedule.json`. The file contains regular and advisory block times, mini-block times, and all four lunch waves. The Windows build embeds it and copies it into publish output; the Linux build embeds it in the Qt resource system; the macOS packaging script copies it into `Contents/Resources`.

Calendar HTML parsing is separated from networking in all implementations. Saved HTML fixtures under `shared/test-data/calendar/` keep parser tests offline and deterministic. Network failures result in an understandable unknown day state rather than a tray-app crash.

Configuration is platform-native: Windows keeps the existing `%LOCALAPPDATA%\JupitrApp\schedule.json` location for compatibility, Linux uses Qt's XDG-aware standard paths, and macOS uses `~/Library/Application Support/Jupitr/schedule.json`.
