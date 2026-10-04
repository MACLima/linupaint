#!/usr/bin/env bash
# Builds LinuPaint-x86_64.AppImage with linuxdeploy and its Qt plugin. Run from the repository root.
set -euo pipefail

BUILD=${BUILD:-build-appimage}
cmake -S . -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DLINUPAINT_BUILD_TESTS=OFF
cmake --build "$BUILD"
DESTDIR="$PWD/$BUILD/AppDir" cmake --install "$BUILD"

TOOLS="$PWD/$BUILD/tools"
mkdir -p "$TOOLS"
for tool in linuxdeploy-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage; do
    if [ ! -x "$TOOLS/$tool" ]; then
        base=https://github.com/linuxdeploy
        repo=${tool%-x86_64.AppImage}
        curl -fsSL -o "$TOOLS/$tool" "$base/$repo/releases/download/continuous/$tool"
        chmod +x "$TOOLS/$tool"
    fi
done

export QMAKE=${QMAKE:-$(command -v qmake6 || command -v qmake)}
export EXTRA_QT_PLUGINS="svg;imageformats"
export OUTPUT=LinuPaint-x86_64.AppImage
APPIMAGE_EXTRACT_AND_RUN=1 "$TOOLS/linuxdeploy-x86_64.AppImage" \
    --appdir "$BUILD/AppDir" \
    --desktop-file packaging/io.github.linupaint.LinuPaint.desktop \
    --icon-file "$BUILD/AppDir/usr/share/icons/hicolor/scalable/apps/io.github.linupaint.LinuPaint.svg" \
    --plugin qt --output appimage
