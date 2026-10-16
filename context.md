# System Context: Luna Language

Deep architectural reference and domain manual for Claude Code and development agents working on the Luna programming language and its ecosystem.

## 1. System Overview

Luna is a high-performance, interpreted and VM-backed scripting language implemented in C11 with supplementary safety verification runtimes written in Rust. It combines a JavaScript-inspired syntax with bare-metal execution performance, SIMD mathematical acceleration, an incremental sub-millisecond garbage collector, and native 2D/3D graphics and audio backends.

Key Characteristics:
- Core Implementation Language: C11 (ISO/IEC 9899:2011) with GNU extensions.
- Verification and Safety Layers: Rust 2021 (`rust/unsafe_rt`, `rust/data_rt`) compiled to static archives (`libluna_memory_rt.a`, `libluna_data_rt.a`).
- Execution Models:
  1. Tree-walk AST Interpreter (`src/interpreter.c`): Direct evaluation of AST nodes.
  2. Bytecode Virtual Machine (`vm/luna_vm.c`): Single-pass compiler emitting bytecode chunks evaluated on an operand stack machine.
- Memory Strategy:
  - AST allocations: Linear bump-allocated arena (`src/arena.c`), zero per-node malloc overhead, reset at execution boundaries.
  - Runtime values: Tracing garbage collector with Snapshot-at-the-Beginning (SATB) write barriers and generational heuristics (`src/gc.c`).
  - Unsafe memory: Explicit non-GC manual memory blocks verified by Rust rule engine.
- Media Engine: Vendored GLFW 3.3 Core Profile renderer, custom 65,536-vertex 2D batch renderer, 3D Blinn-Phong lighting pipeline, and miniaudio sound engine. Zero runtime external package dependencies on standard Linux desktops beyond system OpenGL and X11.

---

## 2. Directory Map and Responsibilities

```
Luna-Language/
├── src/                # Core C11 language engine (Lexer, Parser, AST, Interpreter, GC, Stdlib)
│   ├── arena.c/h       # Linear bump-allocator for O(1) AST allocations
│   ├── ast.c/h         # Abstract Syntax Tree node definitions and lifecycle
│   ├── data_runtime.c  # C-side bindings for Rust structured-data validation
│   ├── env.c/h         # Lexical environments and scope frame management
│   ├── error.c/h       # Diagnostic reporting, line/column tracking
│   ├── file_lib.c/h    # Standard file I/O operations
│   ├── gc.c/h          # Incremental tri-color tracing garbage collector
│   ├── gc_visit.c      # GC root marking and object graph traversal
│   ├── intern.c/h      # String interning pool using djb2 hashing
│   ├── interpreter.c/h # Tree-walk AST execution engine
│   ├── lexer.c/h       # Tokenizer and source scanner
│   ├── library.c/h     # Core runtime built-in function dispatcher
│   ├── list_lib.c/h    # Dynamic array and list manipulation library
│   ├── luna_runtime.c  # High-level engine lifecycle and initialization
│   ├── luna_test.c/h   # Internal test assertion utilities
│   ├── main.c          # CLI entry point, flag parsing, file execution
│   ├── math_lib.c/h    # Standard math functions
│   ├── module_runtime.c# Module resolution and symbol export/import
│   ├── parser.c/h      # Recursive descent parser with operator precedence
│   ├── sand_lib.c      # Cellular automata / sandbox experimental utilities
│   ├── string_lib.c/h  # String formatting, interpolation, and operations
│   ├── time_lib.c/h    # Monotonic timing and benchmarking utilities
│   ├── token.c/h       # Token types and representations
│   ├── unsafe_runtime.c# C-side bindings to Rust unsafe memory validator
│   ├── util.c/h        # General string and memory helper functions
│   ├── value.c/h       # Tagged value system (int, float, string, obj, etc.)
│   └── vec_lib.c/h     # SIMD-accelerated vector and matrix operations
├── include/            # Public C headers
├── vm/                 # Bytecode Compiler and Virtual Machine subsystem
│   ├── luna_chunk.c/h  # Bytecode instruction buffer and constant pool
│   ├── luna_compiler.c # AST-to-bytecode compiler
│   ├── luna_opcode.h   # Opcode definitions for the stack VM
│   ├── luna_vm.c/h     # Virtual Machine evaluation loop and stack
│   └── luna_vm_gc.c    # VM-specific GC integration and stack rooting
├── gui/                # Graphics, 3D, and Audio subsystems
│   ├── gl_backend.c/h  # 2D OpenGL 3.3 batch renderer (65k vertex capacity)
│   ├── gl_backend_3d.c # 3D mesh, camera, and Blinn-Phong lighting engine
│   ├── gui_lib.c/h     # Luna script bindings for 2D graphics
│   ├── gui_lib_3d.c/h  # Luna script bindings for 3D graphics
│   ├── audio_backend.c # Miniaudio integration (streaming, one-shot SFX, FFT)
│   ├── model_backend.c # Wavefront OBJ mesh parsing and upload
│   ├── miniaudio.h     # Vendored miniaudio header
│   ├── stb_image.h     # Vendored image decoding
│   ├── stb_truetype.h  # Vendored TrueType font atlas rasterization
│   └── stb_image_write.h
├── rust/               # Rust validation runtimes
│   ├── unsafe_rt/      # Rust static library enforcing 12 memory safety rules
│   │   ├── Cargo.toml
│   │   └── src/lib.rs
│   └── data_rt/        # Rust static library for structured data (bloc, box, template)
│       ├── Cargo.toml
│       └── src/lib.rs
├── asm/                # Assembly optimizations (ELF64 x86_64)
├── test/               # Test suites (.lu scripts, expectation files)
├── cargame/            # 2D graphics demo application
├── musicplayer/        # Audio engine and visualizer demo application
├── bin/                # Target binary output directory (`bin/luna`)
├── obj/                # Object files and compiler dependency graphs
├── lib/                # Static libraries (`libglfw3.a`, `libluna_*.a`)
├── Makefile            # Project build orchestration
└── test_runner.sh      # Automated bash test suite runner
```

---

## 3. Core Subsystems

### 3.1 Memory Architecture and The Three Domains

Luna enforces a strict three-tier memory architecture to guarantee performance without sacrificing safety:

1. Bump-Allocated AST Arena (`src/arena.c`):
   - All AST nodes (`AstNode`) and token strings created during parsing are allocated inside contiguous memory pages.
   - Allocation is an O(1) pointer bump: `arena->offset += size`.
   - Never individually freed. When execution ends or the parser finishes a batch, the entire arena is reclaimed in one operation.
   - INVARIANT: The GC must never scan or attempt to sweep the AST Arena.

2. Incremental Tri-Color Tracing GC (`src/gc.c`, `src/gc_visit.c`):
   - Manages dynamic runtime values: heap strings, closures, lists, tables, and GC-managed templates.
   - Uses tri-color marking:
     - White: Unvisited objects, candidates for collection.
     - Gray: Visited objects whose child references have not yet been scanned.
     - Black: Visited objects whose child references have been fully scanned.
   - SATB Write Barrier: Any operation that overwrites a reference from an object during the mark phase must capture the overwritten reference and mark it gray to prevent premature collection.
   - Generational Partitioning: Objects surviving initial collection rounds are promoted to older generation tracking to reduce scanning overhead on long-lived data.
   - Worst-case pause target: Under 0.15 milliseconds across benchmark workloads.

3. Unsafe Memory Blocks (`src/unsafe_runtime.c`, `rust/unsafe_rt`):
   - Explicit manual memory accessible via `unsafe { ... }` blocks in Luna scripts.
   - Operations: `alloc`, `store`, `load`, `ptr_offset`, `address_of`, `defer`.
   - Backed by the Rust `unsafe_rt` engine which enforces 12 safety rules (preventing use-after-free, pointer escaping beyond scope, cross-container contamination, and unauthorized GC pointer smuggling).

### 3.2 Parsing and String Interning

- Lexer (`src/lexer.c`): Emits tokens with exact source file, line number, and column tracking.
- String Interning Pool (`src/intern.c`):
  - Identifiers and property keys are hashed using djb2 and interned into a deduplicated global table.
  - Property name lookups and variable equality comparisons can use fast pointer equality (`ptr1 == ptr2`) rather than string comparison (`strcmp`).
- Parser (`src/parser.c`):
  - Recursive descent with Pratt-style precedence for binary expressions.
  - Constant folding at parse time: expressions involving integer or float literals (e.g. `2 + 3 * 4`) are computed during parsing and emitted as pre-folded literal nodes.

### 3.3 Evaluation Engines

- Tree-Walk Interpreter (`src/interpreter.c`):
  - Traverses the AST directly via `eval_node()`.
  - Maintains scope chains via `Environment` structures (`src/env.c`).
  - Supports tail-call optimization for self-recursive functions.
- Bytecode Compiler and VM (`vm/`):
  - Compiles AST into linear instruction chunks (`luna_chunk.c`).
  - Opcode execution is handled by `luna_vm.c` on an evaluation stack.
  - VM integrates with GC root registration via `vm/luna_vm_gc.c`.

### 3.4 Graphics and Audio Subsystem

- 2D Renderer (`gui/gl_backend.c`):
  - Batched dynamic vertex buffer (up to 65,536 vertices per draw call) minimizing OpenGL state transitions.
  - Hardware accelerated primitives: rectangles, lines, circles, polygons, textures, and text.
  - Text rendering using TrueType font atlas generation via `stb_truetype.h`.
- 3D Renderer (`gui/gl_backend_3d.c`):
  - Blinn-Phong lighting with ambient, diffuse, and specular components.
  - Perspective camera, directional and point light calculations.
  - Mesh loading from OBJ format via `gui/model_backend.c`.
- Audio Engine (`gui/audio_backend.c`):
  - Built on miniaudio.
  - Supports multi-channel background streaming, concurrent one-shot SFX playback, and real-time Fast Fourier Transform (FFT) frequency spectrum extraction for audio visualizers.

---

## 4. Build and Toolchain Requirements

- Compiler: `gcc` with `-std=c11` support.
- Assembler: `nasm` with `-f elf64` output.
- Rust Toolchain: `cargo` and `rustc` (2021 edition).
- Static Libraries:
  - `lib/libglfw3.a` (vendored).
  - `lib/libluna_memory_rt.a` (built from `rust/unsafe_rt/`).
  - `lib/libluna_data_rt.a` (built from `rust/data_rt/`).
- System Shared Libraries: `-lGL`, `-lm`, `-lpthread`, `-ldl`, `-lrt`, `-lX11`.

---

## 5. Development Invariants

1. Emojis are strictly banned from all source code, comments, test scripts, commit messages, and documentation.
2. GC barriers must never be skipped when modifying heap references in `src/value.c`, `src/interpreter.c`, or `vm/luna_vm.c`.
3. AST nodes must always be allocated from `src/arena.c`; direct `malloc` of `AstNode` is an architectural defect.
4. Rust FFI symbols must remain C-ABI compatible (`#[no_mangle] pub extern "C"`).
5. All tests in `./test_runner.sh` and `make test` must pass before pushing or finalizing tasks.
