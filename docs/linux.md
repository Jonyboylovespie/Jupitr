# Linux

The Linux build targets Fedora KDE / Wayland first and uses Qt 6 widgets plus the desktop system tray.

## Fedora dependencies

```bash
sudo dnf install gcc-c++ cmake qt6-qtbase-devel qt6-qtsvg-devel
```

## Build and test

```bash
cmake -S linux -B build/linux -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build/linux -j
ctest --test-dir build/linux --output-on-failure
```

Run the tray application with:

```bash
./build/linux/jupitr
```

It starts without a normal main window. Left-click the tray icon to show the popup and choose Quit from the tray menu to exit.

## Configuration, cache, and logs

Qt's XDG-aware standard paths are used. With the normal application name, the locations are approximately:

```text
~/.config/jupitr/schedule.json
~/.cache/jupitr/calendar_cache.txt
~/.local/share/jupitr/scraper.log
```

The exact base directories follow `XDG_CONFIG_HOME`, `XDG_CACHE_HOME`, and `XDG_DATA_HOME` when those variables are set.

## Desktop integration

Install the application and desktop entry for the current user:

```bash
cmake --install build/linux --prefix "$HOME/.local"
```

Log out and back in, or refresh the KDE application launcher, to see Jupitr. To opt into autostart, copy the desktop entry into the XDG autostart directory:

```bash
mkdir -p "$HOME/.config/autostart"
cp linux/resources/jupitr.desktop "$HOME/.config/autostart/"
```

Autostart is optional and is never enabled by the application itself.

## KDE and Wayland notes

The popup is a frameless Qt tool window and uses the tray geometry when the desktop exposes it. Wayland compositors may constrain global window placement; when no tray geometry is available, Jupitr falls back to the active screen's work area.
