#!/bin/bash
# Installs the Arks Sonic 22 Audio Unit from the GitHub Actions artifact.
#
# Usage (in Terminal, from the unzipped artifact folder):   bash install_mac.sh
#
# It unpacks "Arks Sonic 22.component.zip", installs the AU for the current user, gives it an
# ad-hoc signature, drops the quarantine flag, and refreshes the Audio Unit cache.

set -euo pipefail
cd "$(dirname "$0")"

PLUGIN_NAME="Arks Sonic 22"
ZIP="$PLUGIN_NAME.component.zip"
DST="$HOME/Library/Audio/Plug-Ins/Components"

if [ ! -f "$ZIP" ]; then
    echo "Can't find $ZIP next to this script."
    exit 1
fi

echo "== Unpacking"
rm -rf "$PLUGIN_NAME.component"
ditto -x -k "$ZIP" .

echo "== Installing to $DST"
mkdir -p "$DST"
rm -rf "$DST/$PLUGIN_NAME.component"
cp -R "$PLUGIN_NAME.component" "$DST/"

codesign --force --deep --sign - "$DST/$PLUGIN_NAME.component"
xattr -dr com.apple.quarantine "$DST/$PLUGIN_NAME.component" 2>/dev/null || true
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo
echo "Installed: $DST/$PLUGIN_NAME.component"
echo "Open Logic Pro (it rescans Audio Units at launch) and look under"
echo "Audio FX > Audio Units > Arks Plugin > $PLUGIN_NAME."
