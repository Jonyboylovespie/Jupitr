# Jupitr

Jupitr is a lightweight school-schedule tray application for Darien High School's rotating A–H schedule.

## Features

- Automatic DHS day-type detection from the district calendar
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

Both implementations use the authoritative schedule specification at [shared/schedule/bell-schedule.json](shared/schedule/bell-schedule.json). The platform implementations remain intentionally separate so each can follow native desktop conventions.

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

See the platform documentation for dependencies, testing, installation, and configuration paths.

## Project layout

```text
windows/                 .NET 8 WinForms implementation
linux/                   Qt 6 implementation
shared/schedule/         cross-platform bell schedule
shared/assets/           branding and icons
shared/test-data/        offline parser fixtures
docs/                    platform and architecture notes
```

## Releases

Jupitr uses one semantic version for both platforms. A GitHub release can publish separate Windows and Linux artifacts, for example `Jupitr-Windows-x64.zip` and `Jupitr-Linux-x86_64.tar.gz`.
