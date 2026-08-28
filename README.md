# Jupitr

Jupitr is a lightweight school-schedule tray application for Darien High School's rotating A–H schedule.

## Features

- DHS day types from the official one-page letter-day calendar
- Current-class countdown and full daily schedule
- Advisory-day support
- Lunch-wave support, including no-lunch configuration
- Mini-block support using `Class 1 / Class 2`
- Personal schedule configuration
- Native system-tray interfaces

## Platforms

### Windows

C# / .NET 8 / WinForms. See [Windows setup](docs/windows.md).

### Linux

C++ / Qt 6, with Fedora KDE / Wayland as the primary target. See [Linux setup](docs/linux.md).

### macOS

Swift 6 with AppKit and SwiftUI. See [macOS setup](docs/macos.md).

All implementations use the authoritative bell schedule at [shared/schedule/bell-schedule.json](shared/schedule/bell-schedule.json) and the DHS one-page calendar data at [shared/schedule/letter-day-calendar.json](shared/schedule/letter-day-calendar.json). The platform implementations remain intentionally separate so each can follow native desktop conventions.

## Building

Windows:

```powershell
dotnet build windows/JupitrApp.csproj -c Release
```

Linux:

```bash
cmake -S linux -B build/linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux -j
```

macOS:

```bash
swift test --package-path macos
bash macos/scripts/build-app.sh
```

See the platform documentation for dependencies, testing, installation, and configuration paths.

## Project layout

```text
windows/                 .NET 8 WinForms implementation
linux/                   Qt 6 implementation
macos/                   Swift/AppKit/SwiftUI implementation
shared/schedule/         cross-platform bell schedule and letter-day calendar
shared/assets/           branding and icons
docs/                    platform and architecture notes
```

## Releases

Jupitr uses one semantic version for every platform. A GitHub release can publish separate artifacts such as `Jupitr-Windows-x64.zip`, `Jupitr-Linux-x86_64.tar.gz`, and `Jupitr-macOS.zip`.
