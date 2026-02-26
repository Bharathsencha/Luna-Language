# Luna Architecture Documentation

This document provides an exhaustive technical overview of the Luna project, detailing every file, module, and architectural decision.

---

## 1. Core Interpreter (The Engine)

The Luna core is a modular, high-performance tree-walking interpreter implemented in C.

### [Source Files (src/)]

- **src/main.c**: The main entry point. Orchestrates the lifecycle of the interpreter, handles CLI arguments, initializes the global environment, and runs either the REPL or a script file. It also implements the "Auto-Main" feature which automatically executes a `main()` function if defined in the script.
- **src/lexer.c**: Implements the lexical scanner. It converts raw source text into a stream of tokens, handles multi-character operators, complex string literals with escape sequences, and both integer and floating-point numeric formats.
- **src/parser.c**: A recursive-descent parser that builds the Abstract Syntax Tree (AST). It handles operator precedence (using Pratt-parsing logic for expressions), function definitions, control flow structures, and block scoping.
- **src/interpreter.c**: The primary execution engine. It recursively evaluates the AST, managing function calls, variable resolution across scopes, and control flow signals (Return, Break, Continue).
- **src/ast.c**: Defines the structure of AST nodes and provides constructors for various node types (Loops, Assignments, Calls, etc.). It works in tandem with the Memory Arena.
- **src/arena.c**: Implements a contiguous **Memory Arena** for AST nodes. This allows for extremely fast `O(1)` allocations and a single-sweep `arena_reset()` that wipes millions of nodes instantly when the script finishes.
- **src/intern.c**: Implements global String Interning. It maintains a hash set of unique strings, guaranteeing that identical strings share the exact same pointer, unlocking `O(1)` string comparisons.
- **src/value.c**: The core dynamic data system. Every Luna variable is a `Value` struct. This file handles type checking and implements **Reference Counting** for dynamic types like Strings and Lists to provide deterministic, pause-free memory management.
- **src/env.c**: Manages the environment hierarchy (scopes). It handles variable shadowing, local vs. global lookups, and the mapping of identifiers to values.
- **src/error.c**: The unified diagnostic system. It highlights the exact line of code where an error occurred and provides friendly "hints" to help developers fix syntax or logic mistakes.
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
- **include/token.h**: Defines the definitive list of all support keywords and symbols.
- **include/util.h**: General-purpose helper declarations.
- **include/intern.h**: Interface for the String Intern table.
- **include/mystr.h**: A cross-platform polyfill providing `my_strdup` for systems where POSIX functions are unavailable. By implementing a custom string duplication helper, Luna remains ISO C standards-clean and highly portable across embedded and freestanding environments.

---

## 2. Standard Library & Native Bridges

Luna exposes powerful C-implemented functionality to scripts through a standardized native bridge.

- **src/library.c / include/library.h**: The central registry. Every native function (e.g., `print`, `cos`, `append`) is registered here to be reachable from Luna scripts.
- **src/math_lib.c / include/math_lib.h**: Scientific computing suite. Includes trigonometry, logarithms, power functions, and the high-performance **xoroshiro128++** pseudo-random number generator.
- **src/string_lib.c / include/string_lib.h**: Polymorphic string functions (`split`, `join`, `trim`, `replace`, `contains`) designed for high-level scripting ease.
- **src/list_lib.c / include/list_lib.h**: Manages Luna Lists, including hybrid Timsort-style sorting and Fisher-Yates shuffling.
- **src/vec_lib.c / include/vec_lib.h**: Native 2D and 3D vector math, plus matrix multiplication, optimized for game development.
- **src/file_lib.c / include/file_lib.h**: Direct wrapping of standard C file streams for reading, writing, and checking file existence.
- **src/time_lib.c / include/time_lib.h**: Interface for the system clock and monotonic timers.
- **src/sand_lib.c**: A specialized "Sand Grid" native plugin implemented in C for high-speed pixel-physics simulations.

---

## 3. Graphics & Multimedia (The GUI Layer)

The `gui/` directory integrates the **Raylib** library directly into Luna, transforming it into a creative coding platform.

- **gui/gui_lib.c / gui_lib.h**: Implements a vast array of graphics functions (rectangles, circles, textures, custom shaders), audio management (music streams, sound effects, real-time FFT frequency analysis), and hardware input (mouse, keyboard, gamepads).
- **include/raylib.h, include/raymath.h, include/rlgl.h**: The standard Raylib headers provided as the foundation for the graphics backend.

---

## 5. Support Directories

- **docs/**: Full documentation suite, including this architecture guide, module references, and performance tuning notes.
- **demos/**: High-quality, curated Luna programs showcasing complex graphics, audio, and UI.
- **fun/**: A playground of experimental scripts, from retro games to sandbox simulations.
- **benchmark/**: A set of rigorous tests used to measure interpreter speed and memory usage against other scripting languages.
- **assets/**: Centralized storage for media assets (Inter font, game textures, ambient music) used by the built-in demos.
- **bin/**: Destination for compiled binaries (`luna`, `luna2c`) and the project toolchain.

---

## Memory Management Overview

Luna uses a hybrid, deterministic strategy to ensure performance without the overhead of a background Garbage Collector:
1. **Reference Counting**: Used for Strings and Lists to ensure memory is freed the instant it is no longer needed.
2. **Arena Allocation**: Used for AST nodes and dense math arrays to allow for near-instant allocation and singular, bulk deallocation.

---
