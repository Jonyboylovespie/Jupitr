# Architecture

Jupitr has three native UI implementations and one authoritative set of platform-neutral resources.

```text
Official DHS one-page letter-day calendar
        ↓
Packaged shared calendar data
        ↓
     Day type
        ↓
Schedule engine ← user configuration
        ↓
   Tray / popup / settings UI
```

The Windows implementation is C#/.NET 8/WinForms under `windows/`. The Linux implementation is C++20/Qt 6 under `linux/`. The macOS implementation is Swift/AppKit/SwiftUI under `macos/`. They do not share source code; this avoids forcing any platform into another platform's UI or lifecycle model.

The schedule engine in each implementation reads `shared/schedule/bell-schedule.json`. The file contains regular, advisory, and first-day/all-periods block times, mini-block times, and all four lunch waves. Day types come from `shared/schedule/letter-day-calendar.json`, transcribed from the one-page calendar linked by the DHS Calendars and Bell Schedules page. Windows embeds both resources and copies them into publish output, Linux embeds them in the Qt resource system, and macOS copies them into `Contents/Resources`.

Calendar lookups are local and deterministic. Dates not listed in the official letter-day calendar, such as weekends, holidays, and exam-only days, produce the existing unknown-day state without contacting the former district-calendar website.

Configuration is platform-native: Windows keeps the existing `%LOCALAPPDATA%\JupitrApp\schedule.json` location for compatibility, Linux uses Qt's XDG-aware standard paths, and macOS uses `~/Library/Application Support/Jupitr/schedule.json`. The optional `AdditionalLunchWaves` map is backward-compatible and is used only when Block 3 contains a mini pair without a Free period.
