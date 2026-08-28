# Windows

## Requirements

- .NET 8 SDK
- Windows 10 or newer with WinForms support

## Build and run

From the repository root:

```powershell
dotnet restore windows/JupitrApp.csproj
dotnet build windows/JupitrApp.csproj -c Release
dotnet run --project windows/JupitrApp.csproj
```

The application starts in the notification area. Left-click the tray icon to open the schedule popup; use the gear button to edit classes and lunch waves.

## Configuration

Existing Windows configuration remains at:

```text
%LOCALAPPDATA%\JupitrApp\schedule.json
```

The shared bell schedule and official one-page letter-day calendar are embedded in the executable and copied into published output under `shared\schedule`, so the application does not depend on the repository being present at runtime or on the former district-calendar website.

## Packaging

For a framework-dependent x64 publish:

```powershell
dotnet publish windows/JupitrApp.csproj -c Release -r win-x64 --self-contained false -o publish/windows
```

Include the published directory as the Windows release artifact. The schedule specification is included automatically.
