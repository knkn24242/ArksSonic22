#!/bin/bash
# Builds the AU (for Logic Pro) on macOS, installs it for the current user, and validates it with auval.
#
# Usage (in Terminal, from this folder):   bash build_mac.sh
#
# Requirements: Xcode 12.4+ or its Command Line Tools (needs macOS 10.15.4+), CMake, git.
# On older Macs (e.g. macOS 10.13) build with GitHub Actions instead (.github/workflows/build-mac.yml).

set -euo pipefail
cd "$(dirname "$0")"

PLUGIN_NAME="Arks Sonic 22"
BUILD_DIR="build-mac"

echo "== Checking tools"
if ! xcode-select -p >/dev/null 2>&1; then
    echo "Xcode Command Line Tools not found. Run:  xcode-select --install"
    exit 1
fi
if ! command -v cmake >/dev/null 2>&1; then
    echo "CMake not found. Install it with Homebrew:  brew install cmake"
    echo "(or download from https://cmake.org/download/)"
    exit 1
fi
if ! command -v git >/dev/null 2>&1; then
    echo "git not found (it comes with the Xcode Command Line Tools)."
    exit 1
fi

echo "== Getting JUCE 9.0.3"
if [ ! -f external/JUCE/CMakeLists.txt ]; then
    git clone --depth 1 --branch 9.0.3 https://github.com/juce-framework/JUCE.git external/JUCE
fi

echo "== Configuring (Universal: Apple Silicon + Intel)"
cmake -B "$BUILD_DIR" -G "Unix Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13 \
      -DJUCE_DIR="$PWD/external/JUCE" \
      -DAS22_BUILD_TESTS=OFF

echo "== Building AU (the first build takes a while)"
cmake --build "$BUILD_DIR" --config Release --target ArksSonic22_AU -j "$(sysctl -n hw.ncpu)"

AU_SRC="$BUILD_DIR/ArksSonic22_artefacts/Release/AU/$PLUGIN_NAME.component"
AU_DST="$HOME/Library/Audio/Plug-Ins/Components"

echo "== Installing"
mkdir -p "$AU_DST"
rm -rf "$AU_DST/$PLUGIN_NAME.component"
cp -R "$AU_SRC" "$AU_DST/"

# Locally built bundle: give it an ad-hoc signature and drop any quarantine flag.
codesign --force --deep --sign - "$AU_DST/$PLUGIN_NAME.component"
xattr -dr com.apple.quarantine "$AU_DST/$PLUGIN_NAME.component" 2>/dev/null || true

# Make macOS pick up the new/updated Audio Unit.
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo "== Validating AU (auval)"
if auval -v aufx Ss22 Arks; then
    echo
    echo "AU installed: $AU_DST/$PLUGIN_NAME.component"
    echo "Open Logic Pro (it rescans Audio Units at launch) and look under"
    echo "Audio FX > Audio Units > Arks Plugin > $PLUGIN_NAME."
else
    echo
    echo "auval reported a problem. Copy the output above when asking for help."
    exit 1
fi
