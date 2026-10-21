# Luna Documentation

Luna is an ahead-of-time compiler: Luna source goes to LLVM IR, and clang builds a native
binary. The compiler and the runtime of compiled programs are both written in Luna.

| Start here | |
|---|---|
| [getting-started.md](getting-started.md) | Requirements, build, `luna build` / `run` / `emit` |
| [language/syntax.md](language/syntax.md) | The language in one page |
| [known-issues.md](known-issues.md) | Open problems of the current toolchain |
| [plan-aot.md](plan-aot.md) | Where the project is going: the plan for the AOT compiler |

## Language

| Page | Covers |
|---|---|
| [language/syntax.md](language/syntax.md) | Types, variables, operators, control flow, functions, closures, `defer`, keyword aliases |
| [language/modules.md](language/modules.md) | `use` / `export` / `import`, resolution, run-once rule |
| [language/data-types.md](language/data-types.md) | `bloc`, `box`, `template`, `data` |

## Standard library

These are the builtin functions. They need no import.

| Page | Covers |
|---|---|
| [stdlib/core.md](stdlib/core.md) | `type`, conversions, lists, maps, `range`, `sort`, `clock`, `args`, `exit`, `system` |
| [stdlib/string.md](stdlib/string.md) | String functions, interpolation, `format` |
| [stdlib/math.md](stdlib/math.md) | Math, random numbers, dense lists and vector math, `mat_mul` |
| [stdlib/file.md](stdlib/file.md) | File I/O |

## Internals

| Page | Covers |
|---|---|
| [internals/architecture.md](internals/architecture.md) | The pipeline from source to binary, the value layout, and what each part of the repository does |
| [internals/lunac.md](internals/lunac.md) | The compiler in detail: modules, the native dialect used to write the runtime, LLVM IR design, bootstrap |
| [internals/gc.md](internals/gc.md) | The runtime's garbage collector |
| [internals/unsafe.md](internals/unsafe.md) | `unsafe` blocks and the 12 safety rules |

## Project

| Page | Covers |
|---|---|
| [testing.md](testing.md) | The test runner, expected output files, how to add a test |
