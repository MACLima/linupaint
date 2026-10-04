# LinuPaint

A classic Paint for Linux: the tools, menus, shortcuts and mouse behavior of the Paint that shipped
up to Windows XP, in a light native application with a modern look.

LinuPaint is not affiliated with Microsoft. "Paint" is used only to describe compatibility.

## Features (v1.0)

- All 16 classic tools: free-form and rectangular selection, eraser/color eraser, fill, pick color,
  magnifier, pencil, brush (12 tips), airbrush, text, line, curve, rectangle, polygon, ellipse and
  rounded rectangle.
- Left button draws with the primary color, right button with the secondary color; Shift constrains
  lines to 45° and shapes to squares and circles; Esc or the other button cancels.
- Pixel-exact drawing (no antialiasing), like the original. Text can be smoothed.
- Selections: move, copy (Ctrl+drag), stamp (Shift+drag), resize handles, opaque or transparent.
- Image menu: flip/rotate, stretch/skew, invert colors, attributes, clear image.
- Zoom 100–800% with pixel grid and thumbnail, full-screen view.
- Files: BMP (1, 4, 8 and 24 bits), PNG, JPEG and GIF; opens anything Qt can read.
- Clipboard, drag and drop, printing, set as desktop background, recent files.
- Autosave every two minutes and recovery after a crash.
- English, Brazilian Portuguese and Spanish.

## Building

Requirements: CMake 3.21+, a C++20 compiler and Qt 6.4 or newer (Widgets, PrintSupport, Svg,
LinguistTools; DBus on Linux).

On Ubuntu 24.04:

```sh
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-svg-dev qt6-tools-dev \
    qt6-tools-dev-tools qt6-l10n-tools qt6-image-formats-plugins
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
./build/src/app/linupaint
```

On Windows (development only), `scripts\build-windows.cmd` builds and tests with MSVC and Qt
installed through `aqtinstall`.

## Packages

| Format | How |
| --- | --- |
| .deb / .rpm | `cd build && cpack -G DEB` / `cpack -G RPM` |
| Flatpak | `flatpak-builder build-dir packaging/flatpak/io.github.linupaint.LinuPaint.yml` |
| AppImage | `packaging/appimage/build-appimage.sh` |

The CI workflow (`.github/workflows/ci.yml`) builds and tests on Ubuntu 24.04 (Qt 6.4), produces
all packages, runs short fuzzing sessions and publishes releases for `v*` tags.

## Code layout

| Path | Contents |
| --- | --- |
| `src/raster` | Pixel engine: drawing primitives, flood fill, transforms, BMP and GIF encoders. No Qt. |
| `src/core` | Document, undo history, selection and the drawing tools. No Qt. |
| `src/app` | Qt Widgets user interface. |
| `tests` | Catch2 unit tests and Qt Test UI tests. |
| `fuzz` | libFuzzer harnesses. |
| `translations` | Qt Linguist files (`scripts/translations.py` holds the reviewed strings). |

## Translations

Source strings are US English. Brazilian Portuguese and Spanish (Latin American) are maintained by
the team; other languages are welcome through Weblate. After changing user-visible strings run
`lupdate src/app -ts translations/*.ts`.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Contributions require agreeing to the
[Contributor License Agreement](docs/CLA.md).

## License

GNU General Public License, version 3 or later. See [COPYING](COPYING).
