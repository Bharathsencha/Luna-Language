# Architecture

Luna is a C11 program (`bin/luna`) with two Rust static libraries linked in. A script goes
through this pipeline:

```
source -> lexer -> parser (AST in arena) -> compiler -> bytecode VM
                                                         |-- GC heap        (src/gc.c)
                                                         |-- unsafe runtime (src/unsafe_runtime.c + rust/unsafe_rt)
                                                         |-- data runtime   (src/data_runtime.c + rust/data_rt)
                                                         '-- native builtins (src/*_lib.c, gui/)
```

## Front end

| File | Role |
|---|---|
| `src/main.c` | CLI entry: runs a file or starts the REPL. Calls a zero-argument `main()` automatically after the top level. Prints `LUNA_GC_STATS` on exit when that variable is set |
| `src/lexer.c` | Tokens, with a 24-byte inline lexeme buffer so most tokens need no heap allocation. Comments start with `#` or `//`. The keyword aliases are mapped here |
| `src/token.c` | Token names for diagnostics |
| `src/parser.c` | Recursive descent with Pratt-style expression precedence. Folds constants at parse time |
| `src/ast.c` | Node constructors, plus call classification: builtins such as `append` and `load` are tagged at parse time |
| `src/arena.c` | Bump allocator for all AST nodes; one reset frees the whole tree. See [arena.md](arena.md) |
| `src/intern.c` | Global intern table, so identifiers and map keys compare by pointer |
| `src/error.c` | Diagnostics with source line, caret and hint |

## Execution

| File | Role |
|---|---|
| `src/luna_compiler.c` | AST to register bytecode |
| `src/luna_vm.c` | The VM. See [vm.md](vm.md) |
| `src/luna_chunk.c` | Bytecode chunk storage |
| `src/luna_vm_gc.c` | VM roots for the collector |
| `src/module_runtime.c` | `use` / `import` path resolution (script-relative, through `realpath`), cycle detection, run-once module cache |
| `src/env.c` | Environments (hash table of interned names) for globals and modules |
| `src/interpreter.c` | Legacy tree-walking interpreter, used by the embedding API and native callbacks (`LUNA_USE_INTERPRETER=1` runs a whole script on it) |
| `src/luna_runtime.c`, `src/luna_test.c` | Embedding and test API used by `zig-test/` |

## Values and memory

| File | Role |
|---|---|
| `src/value.c`, `include/value.h` | The `Value` tagged union: null, int, float, bool, char, string, list, dense list, map, function and closure, native function, file handle, bloc, box, template, pointer |
| `src/gc.c`, `src/gc_visit.c` | Tracing collector. See [gc.md](gc.md) |
| `src/unsafe_runtime.c` | C side of `unsafe { }`: allocation, load and store, defer; forwards every check to `rust/unsafe_rt`. See [unsafe.md](unsafe.md) |
| `src/data_runtime.c` | C side of `bloc` / `box` / `template` validation, backed by `rust/data_rt`. See [language/data-types.md](../language/data-types.md) |

## Standard library

All builtins are registered in `env_register_stdlib` (`src/library.c`).

| File | Builtins |
|---|---|
| `src/library.c` | Core: `assert`, `type`, `int`, `float`, `range`, `map_*`, `debug`, unsafe entry points |
| `src/string_lib.c` | String functions, `format`, conversions |
| `src/list_lib.c` | `sort` (merge sort with insertion sort for small runs), `ssort`, `shuffle` (Fisher-Yates), `map`, `filter`, `reduce`, `dense_list` |
| `src/math_lib.c` | Math, and the xoroshiro128++ random generator |
| `src/vec_lib.c` | SIMD `vec_*` and OpenMP `mat_mul` |
| `src/file_lib.c` | File I/O |
| `src/time_lib.c`, `asm/time.asm` | `clock()`, using a raw `clock_gettime` syscall written in NASM |
| `src/sand_lib.c` | Falling-sand grid plugin (`sand_*`) |

## Graphics and audio (`gui/`)

| File | Role |
|---|---|
| `gui/gl_backend.c` | OpenGL 3.3 core renderer on GLFW: 65,536-vertex batch, shapes, textures (`stb_image`), fonts (`stb_truetype`), framebuffers, screenshots (`stb_image_write`), 2D camera, input |
| `gui/gl_backend_3d.c` | 3D: one Blinn-Phong shader, up to 8 lights, fog, gamma, transform stack, billboards, sorted translucency |
| `gui/model_backend.c` | OBJ + MTL loading (`vendor/tinyobj_loader_c.h`), material colors baked into vertices |
| `gui/audio_backend.c` | miniaudio: streamed music, in-memory sounds, PCM capture for `get_music_fft` |
| `gui/gui_lib.c`, `gui/gui_lib_3d.c` | Builtin wrappers: convert `Value` arguments and manage the ID pools for textures, fonts, sounds, images, models |

GLFW 3.3.10 is linked statically from `lib/libglfw3.a` (header in `include/glfw3.h`). The stb
headers and miniaudio are vendored in `gui/`. At runtime the only system dependencies are
OpenGL and X11.

## Rust runtimes (`rust/`)

| Crate | Output | Role |
|---|---|---|
| `rust/unsafe_rt` | `lib/libluna_memory_rt.a` | Pointer metadata and the 12 unsafe rules; C ABI in `include/memory.h` |
| `rust/data_rt` | `lib/libluna_data_rt.a` | Kind tags, bloc layout limits, box sizes, template schemas |

Both crates are built by `make` with `cargo build --release` and copied into `lib/`. More detail
is in [rust/readme.md](../../rust/readme.md).

## Repository layout

| Path | Contents |
|---|---|
| `src/`, `include/` | C core, VM and headers |
| `gui/` | Graphics and audio backends, vendored single-header libraries |
| `rust/` | Rust runtimes |
| `asm/` | NASM sources |
| `lib/`, `vendor/` | Prebuilt GLFW, Rust outputs, `tinyobj_loader_c.h` |
| `test/`, `test_runner.sh` | Luna script tests |
| `zig-test/` | Host-side Zig tests against the C API |
| `assets/` | Shared fonts, models, and the generated track cache |
| `3D/` | Standalone 3D showcase |
| `docs/` | Documentation and example programs |
