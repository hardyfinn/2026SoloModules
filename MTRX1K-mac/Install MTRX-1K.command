#!/bin/bash
# Installs MTRX-1K (by GPR Music Project) on this Mac for the current user.
# Double-click it in Finder. It copies the AU and VST3 plugins and the app,
# clears the "downloaded from the internet" flag so macOS lets them load,
# and makes the folder where your Matrix-1000 firmware files go.

cd "$(dirname "$0")" || exit 1
set -e

AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
APP_DIR="$HOME/Applications"
DATA_DIR="$HOME/Library/Application Support/GPR Music Project/MTRX-1K"

mkdir -p "$AU_DIR" "$VST3_DIR" "$APP_DIR" "$DATA_DIR/ROM"

echo "Installing MTRX-1K..."
rm -rf "$AU_DIR/MTRX-1K.component" "$VST3_DIR/MTRX-1K.vst3" "$APP_DIR/MTRX-1K.app"
cp -R "MTRX-1K.component" "$AU_DIR/"
cp -R "MTRX-1K.vst3" "$VST3_DIR/"
cp -R "MTRX-1K.app" "$APP_DIR/"

for item in "$AU_DIR/MTRX-1K.component" "$VST3_DIR/MTRX-1K.vst3" "$APP_DIR/MTRX-1K.app"; do
  xattr -dr com.apple.quarantine "$item" 2>/dev/null || true
done

# Ask macOS to look at its Audio Units again, so Live sees the new one.
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "Done."
echo "  AU   -> $AU_DIR/MTRX-1K.component"
echo "  VST3 -> $VST3_DIR/MTRX-1K.vst3"
echo "  App  -> $APP_DIR/MTRX-1K.app"
echo ""
echo "Now put your Matrix-1000 firmware .bin files in the ROM folder that is opening."
open "$DATA_DIR/ROM"
