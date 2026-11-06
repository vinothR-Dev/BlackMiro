# BlackMiro

An original, free, offline-first infinite whiteboard built with **Qt 6 / QML / C++20**.
No account, no login, no subscription — every feature is available to everyone.

> **Repo description:** Free, open whiteboard in the style of Miro, built with Qt 6 / QML / C++ for Windows, macOS, Linux, Android and the web. Sticky notes, shapes, smart arrows, mind maps, frames, templates, comments and live collaboration, plus lossless export to PDF/SVG/PNG/TIFF/WebP. No subscription, no password.

## Download (v1.0.0)

| Platform | File | How to use |
|---|---|---|
| Windows 10/11 (64-bit) | `BlackMiro-1.0.0-windows-portable.zip` | Unzip anywhere, double-click `BlackMiro.exe`. Nothing to install. |
| Android 9+ (64-bit ARM) | `BlackMiro-1.0.0-android-arm64.apk` | Allow "install unknown apps", open the APK. |
| Web | `BlackMiro-1.0.0-web.zip` | Put the files on any static web server and open `index.html`. |
| macOS / Linux | build from source (below) | |

## Features (v1.0)

**Board**
- Infinite canvas: pan, zoom to cursor, pinch zoom, fit to screen, start view, mouse or trackpad mode
- Dot or line grid, snap to grid, smart alignment guides
- Gap measurements while dragging, with snapping to equal gaps and to the gap of neighbouring pairs
- Sticky notes, text, shapes (built-in plus a shape library), icons, emoji, pen, highlighter, smart pen, eraser, lasso, frames, images
- Comments pinned to the board, cursor chat (`/`)

**Connecting things**
- Blue connect dots around a selected object: drag one to draw an arrow, click one to add a connected copy (mind-map branching)
- Arrows that stay attached: straight, elbow or curved; solid, dashed or dotted; thickness, colour, start and end arrows, swap direction, label

**Editing**
- Floating toolbars above the selection (hidden while you drag); image toolbar with flip, crop, alt text and download original
- Right-click menus matched to Miro: arrange and layers, lock, group, link to object or URL, copy as image, save as template, Info (owner, created, last modified)
- Undo/redo, copy/paste (also images and text from other apps), duplicate, Alt+drag copy, keyboard shortcuts (press **F1** for the list)

**Files and sharing**
- Native `.board` file (ZIP: JSON plus the original image files, byte for byte) with version history
- Private boards protected by a password; sign-in by email (no password) is asked only when you save or share
- Live collaboration: Share creates an invite link and copies it for you ("New link" makes a fresh one); shared cursors, selections and edits, end-to-end encrypted
- Templates, including your own; Saved files library
- System / Light / Dark theme in one click

## Export

Formats: **BOARD, PDF, SVG, PNG, TIFF, WebP, JPEG, AVIF\*, CSV, Markdown, JSON** — any combination,
or all at once into one ZIP (one folder per format). Scope: whole board, selection, or each frame.

| Compression | Meaning | Raster | Embedded images | `.board` |
|---|---|---|---|---|
| **Low** | Lossless, best quality | 600 DPI, 16-bit PNG/TIFF, lossless WebP, JPEG 100 (4:4:4) | Original bytes; PDF uses Flate, never JPEG | Full history |
| **Medium** | Average quality | 300 DPI, 8-bit, JPEG/WebP 85 | Resized to 150 DPI, re-encoded at 85 | Full history |
| **High** | Low quality, smallest | 150 DPI, 256-colour PNG/TIFF, JPEG/WebP 60 | Resized to 96 DPI, re-encoded at 60 | History removed |
| **Custom** | Your DPI / quality / bit depth | — | — | — |

PDF and SVG are always vector (text stays text). \*AVIF appears only when an AVIF image plugin is installed.

## Build

Requires Qt 6.8+ (developed on 6.11.1) and CMake 3.21+.

**Windows (MSVC):**

```bat
scripts\build-windows.bat
scripts\run-tests-windows.bat
```

**Linux / macOS:**

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.11.1/<kit> -DCMAKE_BUILD_TYPE=Release
cmake --build build && ./build/tst_export
```

**Web (WebAssembly)** — needs the Emscripten SDK matching your Qt (4.0.7 for Qt 6.11):

```bat
scripts\build-web.bat
python -m http.server 8080 --directory build\web
```

Then open http://localhost:8080/BlackMiro.html. Host the four files in `build\web`
(`.html`, `.js`, `.wasm`, `qtloader.js`) on any static web server. In the browser, Open / Save /
Insert image use the browser's own file dialogs, and Export produces a single download
(a ZIP when several files are produced).

**Android** — needs the Android SDK, NDK r27c and a JDK 17+ (Android Studio's bundled one works):

```bat
scripts\build-android.bat
adb install -r build\android\BlackMiro.apk
```

The APK is for 64-bit ARM phones (Android 9+) and is signed with your local Android debug key,
so it can be side-loaded. Sign with your own release keystore before publishing to a store.
On Android, Export opens the system "Save as" picker and writes one file (ZIP for several).

Touch: one finger draws / moves / selects (drag on empty space pans), two fingers pinch-zoom and
pan, double-tap edits text.

## Releases and GitHub Pages

Every push to `main` runs [.github/workflows/release.yml](.github/workflows/release.yml) on GitHub Actions:

1. Builds Windows, web and Android, and runs the tests.
2. Puts the Windows portable zip, the Android APK and the web zip on the **Releases** page, under the tag `v<version>`. The version comes from `project(BlackMiro VERSION …)` in `CMakeLists.txt`. Pushing again with the same version replaces that release's files; raise the version to make a new release.
3. Publishes **GitHub Pages**: a download page ([site/index.html](site/index.html)) at the root and the web app at `/app/`.

One-time setup on GitHub: **Settings → Pages → Build and deployment → Source: GitHub Actions**.

To make the same files on your own PC: run the three build scripts, then `scripts\package-release.bat <version>` (output in `dist\`).

## Layout

```
src/core   UI-free engine: model, undo, renderer, .board format, exporter, SVG writer
src/app    QML-facing C++: BoardCanvas (input + painting), ExportController (threaded export)
qml        UI: main window, tool bar, properties panel, export dialog
tests      Export / lossless round-trip tests
```

## Licensing

Qt is used under LGPL-3.0 (dynamic linking on desktop/Android). The WebAssembly build links Qt
statically: distribute it under GPL-3.0, or provide relinkable object files per LGPL.
The app also uses a few Qt private APIs (ZIP reader/writer, browser file access), so it must be
rebuilt when Qt is upgraded.
