# Exposition

A 2D digital painting app written in **C++20** with **[raylib](https://www.raylib.com/)**. Exposition renders a high-resolution canvas, paints with a soft-edged brush (adjustable size, softness and opacity), has a paint bucket that respects the fade of brush strokes, an HSV color picker with a hex field, and undo/redo history.

The project was built as a study of how image editor tools work under the hood: CPU pixel compositing, stroke interpolation, flood fill and a hand-drawn UI on top of raylib, with no UI library.

## Features

- **Soft brush**: size from 1 to 100 px, edge softness from 0 to 100% and opacity from 0 to 100%. The coverage of each pass is combined so the stroke stays clean even when it crosses itself several times, with no artifacts or visible seams.
- **Smooth strokes**: mouse points are interpolated with Catmull-Rom splines and the brush stamps along the curve with spacing proportional to its size.
- **Paint bucket (hold `G` and click)**: flood fill that respects stroke fade, estimating the coverage of each edge pixel to blend the new color with the stroke color instead of leaving a halo of the old color.
- **Color picker**: the `+` button opens a panel with a saturation/brightness square, a hue slider, a preview of the original color (clickable to revert) and a hex field that supports pasting. The 4 most recently used colors are kept next to it.
- **Undo/redo** with up to 20 states.
- **Zoom and pan** on the camera, with HiDPI (Retina) support.
- **Preset resolutions**: 1080×1080, 2048×2048, 1920×1080, 3840×2160, 2480×3508 (A4), 2048×1536, 1080×1920, 3000×2000, 1200×1800 and 2560×1080.
- **Copy the canvas to the clipboard** (macOS only).

## Shortcuts and controls

| Action | Control |
| --- | --- |
| Draw | Left mouse button |
| Fill (bucket) | Hold `G` + left click |
| Brush size | Hold right button and drag **vertically** |
| Brush softness | Hold right button and drag **horizontally** |
| Brush opacity | `Alt` + scroll |
| Zoom | Scroll |
| Pan | `Space` + drag, or middle button + drag |
| Center and fit the camera | `R` |
| Undo | `Cmd/Ctrl` + `Z` |
| Redo | `Cmd/Ctrl` + `Shift` + `Z` or `Cmd/Ctrl` + `Y` |
| Choose resolution (creates a new canvas) | `Cmd/Ctrl` + `R` |
| Copy image to the clipboard | `Cmd/Ctrl` + `S` (macOS) |

The top of the window shows the brush `size | softness% | opacity%` and, in the right corner, the current zoom.

## Libraries

| Library | Version | Use |
| --- | --- | --- |
| [raylib](https://www.raylib.com/) | 6.0 | Window, input, 2D drawing, textures and fonts (`raylib.h`, `raymath.h`, `rlgl.h`) |
| [Inter](https://rsms.me/inter/) | | UI font (`assets/Inter.ttf`) |
| macOS frameworks | | `Cocoa`, `IOKit`, `CoreVideo`, `GLUT` and `OpenGL`, used to link raylib |

Besides that, only the C++ standard library (`<vector>`, `<algorithm>`, `<cmath>`, `<string>`, etc.). Copying to the clipboard uses `osascript`, which ships with macOS.

raylib is already included in the repository under `dependencies/raylib`, so on macOS nothing needs to be installed besides the compiler. Note that the bundled `libraylib.a` is built for macOS (x86_64 + arm64), and the `Makefile` is also macOS-specific (it links against macOS frameworks).

## Folder structure

```
Exposition/
├── assets/
│   └── Inter.ttf                   # UI font
├── dependencies/
│   └── raylib/
│       ├── include/                # raylib.h, raymath.h, rlgl.h
│       └── lib/
│           └── libraylib.a         # static raylib (x86_64 + arm64)
├── src/
│   ├── main.cpp                    # window and main loop
│   ├── canva.h / canva.cpp         # pixels, texture, undo/redo, flood fill
│   ├── brush.h / brush.cpp         # brush, stamps, Catmull-Rom spline
│   ├── exposition.h                # main application class
│   ├── exposition.cpp              # lifecycle, camera, mouse and shortcuts
│   ├── exposition_picker.cpp       # color picker: logic, drawing and hex conversion
│   └── exposition_ui.cpp           # hit-testing and UI rendering
├── Makefile
├── compile_flags.txt               # flags for clangd / editors
└── README.md
```

The `build/` and `bin/` directories (intermediate objects and the executable) are created during compilation and are not versioned.

## Architecture

- **`Canva`** keeps the pixels in a `std::vector<Color>` on the CPU and a `Texture2D` on the GPU, which is only updated when something changed (`dirty` flag). It also holds the undo/redo stacks and implements the flood fill.
- **`Brush`** turns the mouse path into circular stamps. It keeps per-pixel coverage buffers (current pass, previous passes and the time of the last stamp) so that crossing its own stroke does not create artifacts, and applies the global opacity only at composite time, acting as a ceiling for the whole stroke.
- **`Exposition`** is the application class: it reads the mouse and keyboard, controls the 2D camera, decides whether a click goes to the UI or to the canvas, and draws the whole UI. Its implementation is split into three files (`exposition.cpp`, `exposition_picker.cpp`, `exposition_ui.cpp`) to keep each one with a clear responsibility.

## Building and running

Requirements: **macOS**, **clang++** (Xcode Command Line Tools: `xcode-select --install`) and **make**.

```bash
make all       # build the app
make run    # build (if needed) and run
make clean  # remove the intermediate objects in build/
```

Run it from any folder: the font is located in `assets/` relative to the executable path. If `Inter.ttf` is not found, raylib falls back to its default font.

The `Makefile` links **statically** against `dependencies/raylib/lib/libraylib.a`.

> The `Makefile` and the bundled raylib library are macOS-only. To build on another platform (Linux, Windows) you need to download or build raylib for that platform, install its dependencies, and adapt the `Makefile` (compiler, link flags and library path). The clipboard copy function, which currently only exists on macOS, would also need a platform-specific implementation.

## Author

phmuni
