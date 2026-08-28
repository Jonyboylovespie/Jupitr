#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
MACOS_ROOT="$REPO_ROOT/macos"
CONFIGURATION="${CONFIGURATION:-release}"
DIST_DIR="${DIST_DIR:-$REPO_ROOT/dist/macos}"
APP_PATH="$DIST_DIR/Jupitr.app"

if ! command -v swift >/dev/null 2>&1; then
    echo "Swift is required to build Jupitr for macOS." >&2
    exit 1
fi

VERSION="$(tr -d '[:space:]' < "$REPO_ROOT/VERSION")"
BIN_PATH="$(swift build --package-path "$MACOS_ROOT" --configuration "$CONFIGURATION" --product Jupitr --show-bin-path)"
swift build --package-path "$MACOS_ROOT" --configuration "$CONFIGURATION" --product Jupitr >/dev/null

rm -rf "$APP_PATH"
mkdir -p "$APP_PATH/Contents/MacOS" "$APP_PATH/Contents/Resources"

cp "$BIN_PATH/Jupitr" "$APP_PATH/Contents/MacOS/Jupitr"
sed "s/__JUPITR_VERSION__/$VERSION/g" \
    "$MACOS_ROOT/Resources/Info.plist.template" \
    > "$APP_PATH/Contents/Info.plist"
cp "$REPO_ROOT/shared/schedule/bell-schedule.json" "$APP_PATH/Contents/Resources/bell-schedule.json"
cp "$REPO_ROOT/shared/schedule/letter-day-calendar.json" "$APP_PATH/Contents/Resources/letter-day-calendar.json"
cp "$REPO_ROOT/shared/assets/jupitr.svg" "$APP_PATH/Contents/Resources/jupitr.svg"

chmod +x "$APP_PATH/Contents/MacOS/Jupitr"

if [[ -n "${CODESIGN_IDENTITY:-}" ]]; then
    codesign --force --deep --options runtime --sign "$CODESIGN_IDENTITY" "$APP_PATH"
fi

echo "Built $APP_PATH (version $VERSION)"
