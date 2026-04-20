# Luna Installation & Setup Guide 

This document provides step-by-step instructions for setting up the Luna development environment.

---

## 1. Core Build Tools

Luna requires a standard C build stack:

```bash
sudo apt update
sudo apt install gcc build-essential git cmake nasm
```

For the Zig-based internal test suite, install Zig as well:

```bash
sudo apt install zig
```

## 2. Graphics Dependencies

Luna uses its own OpenGL 3.3 rendering backend with a **statically linked** GLFW library (already vendored in `lib/libglfw3.a`). You only need the system-level graphics headers and X11 development libraries:

```bash
sudo apt install libx11-dev libgl1-mesa-dev
```

> **Note:** Luna previously used Raylib as its graphics backend. That dependency has been fully removed and replaced with a custom OpenGL + GLFW backend. You do **not** need to install Raylib.

### Vendored Libraries 

The following single-header libraries are already included in the `gui/` directory — no download or install is required:

| Library | File | Source | Purpose |
| :--- | :--- | :--- | :--- |
| **GLFW 3.3.10** | `lib/libglfw3.a`, `include/glfw3.h` | [github.com/glfw/glfw](https://github.com/glfw/glfw/releases/tag/3.3.10) | Window creation, OpenGL context, input handling |
| **stb_image** | `gui/stb_image.h` | [github.com/nothings/stb](https://github.com/nothings/stb/blob/master/stb_image.h) | Image loading (PNG, JPG, BMP, etc.) |
| **stb_truetype** | `gui/stb_truetype.h` | [github.com/nothings/stb](https://github.com/nothings/stb/blob/master/stb_truetype.h) | TrueType font rasterization |
| **stb_image_write** | `gui/stb_image_write.h` | [github.com/nothings/stb](https://github.com/nothings/stb/blob/master/stb_image_write.h) | Screenshot saving (PNG) |
| **miniaudio** | `gui/miniaudio.h` | [github.com/mackron/miniaudio](https://github.com/mackron/miniaudio) | Audio playback, streaming, PCM capture |

---

## 3. Building Luna

Once the core build tools and graphics dependencies are installed, clone the Luna repository and run:

```bash
make
```

The Makefile automatically uses parallel compilation (`-j$(nproc)`) and links against the vendored static GLFW library plus system OpenGL, X11, pthreads, and libdl.

---

## 4. Verification

Run the core test suite to verify the interpreter works:

```bash
bin/luna test/test_core.lu
```

Run the internal Zig test suite to verify lexer/parser/interpreter/error internals:

```bash
make zig-test
```

To verify that the graphics backend works, run one of the demo scripts:

```bash
bin/luna fun/meadow.lu
```

A window should open with an animated scene. Press Escape or close the window to exit.

## 5. Test Workflow

Luna now has two separate checks during development:

- `make test`
  Runs the existing Luna script tests and golden-output checks.
- `make zig-test`
  Runs host-side Zig tests against the internal C APIs for the lexer, parser, interpreter, and diagnostics.

Use both when changing syntax, AST construction, evaluation logic, or error reporting.
