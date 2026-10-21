# Known Issues

Open problems of the current AOT toolchain, roughly by impact. This file is git-ignored in this
repository, so it is a working list, not a record.

## Language

| # | Issue | Repro |
|---|---|---|
| 1 | `const` is not enforced: reassigning a constant is accepted and changes the value | `const a = 1`, then `a = 2`, then `print(a)` prints `2` |
| 2 | A top-level `func` exists only once execution reaches it, so calling it above its definition is a `Name Error` | `print(f())` before `func f() { return 1 }` |
| 3 | The variable of a C-style `for` loop is shared by all iterations, so closures created in the body all see its final value (`for-in` and `while` bodies are per-iteration) | `for (let i = 0; i < 3; i++) { append(fs, func() { return i }) }`, then `fs[0]()` is `3` |
| 4 | No exponent or hex literals (`1e3`, `0xFF`) and no semicolons between statements | `let x = 1e3` is a syntax error |
| 5 | A `data` value prints with its internal tag: `print(Vec2(1, 2))` shows `{"__tag": Vec2, ...}`, and `type()` of it is `map` | `data Vec2 { x, y }` |
| 6 | `print` with several arguments puts each on its own line | `print(1, 2)` |
| 7 | `for-in` does not accept a map | `for (let k in {"a": 1}) { }` is a `Type Error`; use `map_keys` |
| 8 | An import that cannot be found is reported when the `use` statement runs, not at compile time | `use "nope.lu"` |

## Runtime and library

| # | Issue |
|---|---|
| 9 | The collector is stop-the-world and conservative, with no incremental or generational mode and no compaction (see [internals/gc.md](internals/gc.md)) |
| 10 | The R3 error message says `ptr_add()` even though the function is `ptr_offset()` |
| 11 | `vec_add` and the other `vec_*` calls silently truncate to the shorter list, and return `null` for arguments that are not lists. `mat_mul` prints `Runtime Error: Matrix dimension mismatch` and returns `null` instead of stopping the program |
| 12 | Appending to a dense list gives the confusing message `append() argument 1 must be a list, got list` |
| 13 | `split` drops empty pieces (`split("a,,b", ",")` has two items); strings are byte strings, so `len("héllo")` counts bytes and `to_upper` changes ASCII only |
| 14 | `write` to a file opened with `"r"` is silently ignored |

## Toolchain

| # | Issue |
|---|---|
| 15 | `luna` has no installed location. It finds `compiler/runtime` and `compiler/build_runtime.sh` through `LUNA_HOME`, or the current directory when that is not set. Run from another directory without `LUNA_HOME` and the build fails with `./compiler/build_runtime.sh: not found` |
| 16 | `luna build` and `luna run` write generated IR to `C/` in the current directory, and the runtime object to `$LUNA_HOME/C/luna_rt_native.o` |
| 17 | `build_runtime.sh` calls `clang` directly; only the final link honors `CC` |
| 18 | The seeds in `bootstrap/` must be refreshed with `make seed` after any change to `compiler/` or `compiler/runtime/`; `make` only warns when they are stale |
| 19 | Performance has not been tuned: the runtime is not inlined into the program, numeric loops call the runtime for every operation, and programs build at clang `-O1`. One data point: `fib(32)` takes about 0.13 s on the development machine. See [plan-aot.md](plan-aot.md) workstream D |
| 20 | No GUI, 3D or audio, and no REPL. Graphics are planned to return through foreign calls to GLFW and OpenGL |
| 21 | Linux only, tested on x86-64. The collector uses `pthread_getattr_np`, and the runtime reads `/dev/urandom` and calls `clock_gettime` |
