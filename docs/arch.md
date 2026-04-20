# Luna Architecture Documentation

This document provides an exhaustive technical overview of the Luna project, detailing every file, module, and architectural decision.


## 1. Core Interpreter (The Engine)

The Luna core is a modular, high-performance tree-walking interpreter implemented in C.

### [Source Files (src/)]

- **src/main.c**: The main entry point. Orchestrates the lifecycle of the interpreter, handles CLI arguments, initializes the global environment, and runs either the REPL or a script file. It also implements the "Auto-Main" feature which automatically executes a `main()` function if defined in the script.
- **src/lexer.c**: Implements the lexical scanner. It converts raw source text into a stream of tokens, handles multi-character operators, complex string literals with escape sequences, and both integer and floating-point numeric formats. Uses an inline 24-byte buffer in the `Token` struct to avoid heap allocation for the vast majority of tokens (operators, keywords, identifiers, numbers, char literals). Only long string literals fall through to `malloc`.
- **src/parser.c**: A recursive-descent parser that builds the Abstract Syntax Tree (AST). It handles operator precedence (using Pratt-parsing logic for expressions), function definitions, anonymous function literals, control flow structures, block scoping, `use` module statements, legacy `import` compatibility, `export` declarations, `data` declarations, map literals such as `{"key": value}`, and template strings with embedded Luna expressions. Includes a constant folding pass that evaluates binary operations on literals at parse time, eliminating unnecessary AST nodes.
- **src/interpreter.c**: The primary execution engine. It recursively evaluates the AST, managing function calls, variable resolution across scopes, closure values with captured scope snapshots, control flow signals (Return, Break, Continue), module loading, scope-level deferred cleanup calls, `data` constructor instantiation, map key indexing, and string interpolation on evaluated expressions. Includes tail-call optimization for self-recursive functions, converting O(n) stack growth into O(1) constant space.
- **src/ast.c**: Defines the structure of AST nodes and provides constructors for various node types (Loops, Assignments, Calls, etc.). It works in tandem with the Memory Arena.
- **src/arena.c**: Implements a contiguous **Memory Arena** for AST nodes. This allows for extremely fast `O(1)` allocations and a single-sweep `arena_reset()` that wipes millions of nodes instantly when the script finishes.
- **src/intern.c**: Implements the global string-intern table. Parser, AST, environment, library registration, and map/data-tag paths all feed repeated identifier and key strings through this table so equal text reuses one canonical pointer. That cuts duplicate allocations and makes hot-path name/key comparisons pointer-fast after interning.
- **src/value.c**: The core dynamic data system. Every Luna variable is a `Value` struct. This file handles type checks, runtime string/list/map helpers, and the object layouts traced by Luna's current GC runtime.
- **src/gc.c / src/gc_visit.c**: Luna's active tracing GC implementation. This is the current runtime heap manager for strings, lists, dense lists, maps, closures, and GC-owned backing storage.
- **src/env.c**: Manages the environment hierarchy (scopes). It handles variable shadowing, local vs. global lookups, and the mapping of identifiers to values.
- **src/error.c**: The unified diagnostic system. It highlights the exact line of code where an error occurred and provides friendly "hints" to help developers fix syntax or logic mistakes.
- **src/unsafe_runtime.c**: The C-side bridge for Luna's manual-memory feature set. It owns the live pointer metadata visible to the interpreter, forwards rule checks into the Rust unsafe runtime, frees raw `Value` buffers, and reports rule failures back through Luna's normal error pipeline.
- **src/luna_runtime.c**: A reusable runtime entry layer for host-side tooling and subsystem tests. It lets external test code parse and execute Luna source without going through the CLI in `main.c`.
- **src/luna_test.c**: A thin testing bridge that exposes stable helper functions for host-side tests, such as lexing a full source buffer, inspecting AST node kinds, and reading runtime values safely from non-C test code.
- **src/token.c**: Maps internal token enums to human-readable strings for debugging and error reporting.
- **src/util.c**: General file system and string utilities used across the core engine.

### [Header Files (include/)]

- **include/ast.h**: Defines `AstNode`, `NodeList`, and node creation prototypes.
- **include/lexer.h**: Defines `Lexer` state and `lexer_init` functions.
- **include/parser.h**: Defines the `Parser` state and grammar entry points.
- **include/interpreter.h**: Defines the `interpret()` interface.
- **include/value.h**: The most critical header; defines the `Value` struct, `ValueType` enums, and ref-counting macros.
- **include/env.h**: Defines the `Env` struct for scope management.
- **include/arena.h**: Interface for the AST bulk-memory allocator.
- **include/luna_error.h**: Defines all internal error codes and reporting macros.
- **include/luna_runtime.h**: Public runtime helpers for embedding Luna in tests and tools.
- **include/luna_test.h**: Public testing helpers used by the Zig test suite to inspect tokens, AST nodes, values, and structured errors.
- **include/token.h**: Defines the definitive list of all supported keywords and symbols, plus the `Token` struct with its inline lexeme buffer and the `token_str()` accessor.
- **include/util.h**: General-purpose helper declarations.
- **include/intern.h**: Interface for the String Intern table.
- **include/gc.h**: Public GC entry points and tracing hooks shared by the runtime and host-side helpers.
- **include/memory.h**: C declarations for the Rust unsafe-memory rule runtime exported from `rust/unsafe_rt`.
- **include/unsafe_runtime.h**: Interpreter-facing helpers that wrap the lower-level memory runtime and present it as Luna values and runtime checks.
- **include/file_lib.h / string_lib.h / list_lib.h / vec_lib.h / math_lib.h / time_lib.h / library.h**: Public declarations for native standard-library registration and the major built-in library families.
- **include/mystr.h**: A cross-platform polyfill providing `my_strdup` for systems where POSIX functions are unavailable. By implementing a custom string duplication helper, Luna remains ISO C standards-clean and highly portable across embedded and freestanding environments.


## 2. Standard Library & Native Bridges

Luna exposes powerful C-implemented functionality to scripts through a standardized native bridge.

- **src/library.c / include/library.h**: The central registry. Every native function (e.g., `print`, `cos`, `append`, `map_get`, `map_values`, `range`) is registered here to be reachable from Luna scripts.
- **src/math_lib.c / include/math_lib.h**: Scientific computing suite. Includes trigonometry, logarithms, power functions, and the high-performance **xoroshiro128++** pseudo-random number generator.
- **src/string_lib.c / include/string_lib.h**: Polymorphic string functions (`split`, `join`, `trim`, `replace`, `contains`) designed for high-level scripting ease.
- **src/list_lib.c / include/list_lib.h**: Manages Luna Lists, including hybrid Timsort-style sorting (with a single pre-allocated scratch buffer to avoid per-merge malloc/free), Fisher-Yates shuffling, utility helpers such as `find()` and `remove()`, and higher-order builtins like `map()`, `filter()`, and `reduce()`.
- **src/vec_lib.c / include/vec_lib.h**: Native 2D and 3D vector math, plus matrix multiplication, optimized for game development.
- **src/file_lib.c / include/file_lib.h**: Direct wrapping of standard C file streams for reading, writing, and checking file existence.
- **src/time_lib.c / include/time_lib.h**: Interface for the system clock and monotonic timers.
- **src/sand_lib.c**: A specialized "Sand Grid" native plugin implemented in C for high-speed pixel-physics simulations.


## 3. Graphics & Multimedia (The GUI Layer)

The `gui/` directory implements Luna's own rendering and audio backends, transforming it into a creative coding platform. Luna originally used **Raylib** for graphics but has since replaced it with a custom OpenGL 3.3 + GLFW backend and a miniaudio-based audio backend, removing the Raylib dependency entirely.

### Rendering Backend

- **gui/gl_backend.h / gl_backend.c**: Luna's custom OpenGL 3.3 Core Profile renderer built on top of **GLFW** for window/input management. It implements:
  - A high-throughput **batch quad renderer** (VBO + VAO, up to 65,536 vertices per batch) with automatic flushing.
  - Embedded GLSL 330 vertex and fragment shaders compiled at runtime.
  - Shape drawing: filled/outlined rectangles (with optional corner rounding via triangle fans), circles, lines, and 4-corner gradient rectangles.
  - Texture loading via [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h) (single-header, public domain).
  - Font rendering via [stb_truetype](https://github.com/nothings/stb/blob/master/stb_truetype.h) (single-header, public domain) with a baked font atlas and GL_TEXTURE_SWIZZLE for alpha-only rendering. Falls back to a system default font if none is specified.
  - Off-screen rendering via OpenGL Framebuffer Objects (FBOs) for render-to-texture workflows.
  - Screenshot capture via [stb_image_write](https://github.com/nothings/stb/blob/master/stb_image_write.h) (single-header, public domain) with `glReadPixels`.
  - 2D camera transforms (offset, target, rotation, zoom) applied as a model-view matrix.
  - Keyboard and mouse input mapped through GLFW callbacks with key-repeat and scroll support.

- **include/glfw3.h / lib/libglfw3.a**: [GLFW 3.3.10](https://github.com/glfw/glfw/releases/tag/3.3.10) built from source as a **static library**, so users have zero runtime dependencies beyond system OpenGL and X11.
- **gui/stb_image.h, gui/stb_truetype.h, gui/stb_image_write.h**: Single-header libraries from the [stb collection](https://github.com/nothings/stb) by Sean Barrett. Vendored directly into the repository — no install step needed.

### Audio Backend

- **gui/audio_backend.h / audio_backend.c**: Luna's audio system built on [miniaudio](https://github.com/mackron/miniaudio) (single-header, public domain). It provides:
  - Music streaming (`MA_SOUND_FLAG_STREAM`) with play, pause, resume, seek, and duration queries.
  - One-shot sound effects (`MA_SOUND_FLAG_DECODE`) loaded entirely into memory.
  - A PCM ring-buffer capture node for real-time FFT frequency analysis, used by Luna's `get_music_fft()` visualizer function.
  - Decoder-based time length queries for formats supported by miniaudio (MP3, WAV, FLAC, etc.).

- **gui/miniaudio.h**: [miniaudio v0.11](https://github.com/mackron/miniaudio) by David Reid. Single-header audio library vendored directly into the repository.

### GUI Bridge

- **gui/gui_lib.c / gui_lib.h**: The bridge between Luna scripts and the C backends. Every Luna GUI function (e.g., `init_window`, `draw_rectangle`, `load_texture`, `play_music_stream`) is implemented here by converting Luna `Value` types into native calls to `gl_*` and `audio_*` functions. It also manages ID-based pools for textures, fonts, music streams, sounds, images, and render textures.

## 4. Rust Static Runtimes

Luna now has two Rust crates that build to static `.a` archives and are copied into `lib/` by the top-level `Makefile`.

- **rust/unsafe_rt/**: The shipped unsafe-memory rule runtime. It validates pointer operations, allocation metadata, defer stacks, and unsafe-block rules, then exposes a small C ABI through `include/memory.h`.
- **rust/data_rt/**: The new structured-data prototype runtime. It currently implements early runtime building blocks for `bloc`, `box`, and `template`: first-byte kind tags, bloc inline layout validation, fixed-size manual box buffers, schema-backed template descriptors, and a pure-Rust template object model used for tests and iteration before interpreter integration.
- **rust/readme.md**: High-level design notes for both Rust runtimes, build outputs, and how the static archives fit into Luna's native build.

## 5. Support Directories

- **docs/**: Full documentation suite, including this architecture guide, memory notes, GC notes, and the structured-data design document.
- **luna/**: Design notebooks, TODO logs, and changelog history for in-progress runtime work. `Data strucutres.txt` is the design source for the current `bloc` / `box` / `template` plan.
- **rust/**: Rust runtime crates and their design notes.
- **zig-test/**: A second testing layer built in Zig. It imports Luna's C APIs directly and checks lexer output, parser AST shape, interpreter state changes, and exact error diagnostics.
- **cargame/** and **musicplayer/**: Larger showcase applications that exercise Luna's GUI, audio, input, and asset pipelines.
- **fun/**: A playground of experimental scripts, from retro games to sandbox simulations.
- **benchmark/**: A set of rigorous tests used to measure interpreter speed and memory usage against other scripting languages.
- **assets/**: Centralized storage for media assets (Inter font, game textures, ambient music) used by the built-in demos.
- **bin/**: Destination for compiled binaries (`luna`, `luna2c`) and the project toolchain.


## Testing Layers

Luna now has two complementary testing layers:

1. **Language-level Luna tests** in `test/` and `test_runner.sh`
   These verify observable language behavior using Luna scripts, `assert()`, and golden output files.
2. **Host-side Zig tests** in `zig-test/`
   These verify internal engine structure directly, which is useful when a new feature breaks tokenization, AST shape, interpreter state, or error line reporting.

This split is intentional:

- Luna tests answer: "Is the language behavior still correct?"
- Zig tests answer: "Which subsystem broke, and exactly where?"

## Memory Management Overview

Luna uses a hybrid runtime memory model:
1. **Tracing GC**: The active runtime heap is managed by the tracing collector in `src/gc.c`.
2. **Arena Allocation**: AST nodes and parser-owned scratch storage still use the arena for fast bulk lifetime management.
3. **Unsafe Runtime**: Raw pointer buffers created inside `unsafe` blocks stay outside GC ownership and are tracked separately.
