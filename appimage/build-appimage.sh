#!/usr/bin/env bash
# Build the Bejeweled Blitz Live x86_64 AppImage (Re-Jeweled Launcher + game set).
# Layout mirrors the shipped house AppImages: launcher, game exe, the
# matched runtime libs and the xctd import helper together in usr/bin (the
# launcher's program-set search finds them next to itself). No game data
# is bundled. The game exe and its libs are a matched set (one SDK build) -
# do not mix builds (a mixed set froze at boot on 2026-10-12).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT="$(cd "$HERE/.." && pwd)"
# GAME_RUN: a folder holding the matched set bejeweled + librexruntime.so +
# librexgpu-xenos.so (see README.md, "Build").
GAME_RUN="${GAME_RUN:-$PROJECT/out/run}"
LAUNCHER_BIN="${LAUNCHER_BIN:-$PROJECT/tp-launcher/build/ReJeweledLauncher}"
XCTD_BIN="${XCTD_BIN:-$PROJECT/tools/xctd/build/xctd}"
APPDIR="$HERE/AppDir"
OUTPUT="${OUTPUT:-$HERE/ReJeweled-x86_64.AppImage}"
TOOLS="${TOOLS:-$HOME/tools}"  # holds linuxdeploy, linuxdeploy-plugin-qt, appimagetool

for f in "$LAUNCHER_BIN" "$XCTD_BIN" "$GAME_RUN/bejeweled" \
         "$GAME_RUN/librexruntime.so" "$GAME_RUN/librexgpu-xenos.so"; do
  [[ -f "$f" ]] || { echo "Missing input: $f" >&2; exit 1; }
done

rm -rf "$APPDIR"
install -d "$APPDIR/usr/bin"
install -m755 "$LAUNCHER_BIN" "$APPDIR/usr/bin/ReJeweledLauncher"
install -m755 "$GAME_RUN/bejeweled" "$APPDIR/usr/bin/bejeweled"
install -m755 "$GAME_RUN/librexruntime.so" "$APPDIR/usr/bin/librexruntime.so"
install -m755 "$GAME_RUN/librexgpu-xenos.so" "$APPDIR/usr/bin/librexgpu-xenos.so"
install -m755 "$XCTD_BIN" "$APPDIR/usr/bin/xctd"

# Attribution pack ships inside the AppImage, plus the xctd source
# (it links a modified libmspack lzxd.c - LGPL-2.1).
install -d "$APPDIR/usr/share/licenses/rejeweled" "$APPDIR/usr/share/rejeweled/xctd-src"
install -m644 "$HERE/LICENSE" "$HERE/CREDITS.md" \
  "$APPDIR/usr/share/licenses/rejeweled/"
cp -r "$HERE/licenses" "$APPDIR/usr/share/licenses/rejeweled/"
install -m644 "$PROJECT/tools/xctd/README.md" "$PROJECT/tools/xctd/xctd.cpp" \
  "$PROJECT/tools/xctd/lzxd_xmem.c" "$APPDIR/usr/share/rejeweled/xctd-src/"

cat > "$HERE/rejeweled.desktop" <<'DESK'
[Desktop Entry]
Type=Application
Name=Re-Jeweled Launcher
Comment=Launcher for the Bejeweled Blitz Live Linux recompilation
Exec=ReJeweledLauncher
Icon=rejeweled
Categories=Game;
Terminal=false
DESK
convert "$PROJECT/tp-launcher/assets-bejeweled/icon.png" -resize 512x512 "$HERE/rejeweled.png"

export QMAKE="$(command -v qmake6 || command -v qmake)"
export PATH="$TOOLS:$PATH"

"$TOOLS/linuxdeploy" \
  --appdir "$APPDIR" \
  --executable "$APPDIR/usr/bin/ReJeweledLauncher" \
  --executable "$APPDIR/usr/bin/bejeweled" \
  --desktop-file "$HERE/rejeweled.desktop" \
  --icon-file "$HERE/rejeweled.png" \
  --plugin qt

ARCH=x86_64 "$TOOLS/appimagetool" "$APPDIR" "$OUTPUT"
chmod 777 "$OUTPUT"
echo "Created: $OUTPUT"
