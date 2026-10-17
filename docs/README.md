# Luna Documentation

| Start here | |
|---|---|
| [getting-started.md](getting-started.md) | Install, build, run, verify |
| [language/syntax.md](language/syntax.md) | The language in one page |
| [examples/README.md](examples/README.md) | Complete programs: car game, music player, 3D racer, 2D/3D demos |

## Language

| Page | Covers |
|---|---|
| [language/syntax.md](language/syntax.md) | Types, variables, operators, control flow, functions, closures, `defer`, keyword aliases |
| [language/modules.md](language/modules.md) | `use` / `export` / `import`, resolution, caching |
| [language/data-types.md](language/data-types.md) | `bloc`, `box`, `template`, `data` |

## Standard library

| Page | Covers |
|---|---|
| [stdlib/core.md](stdlib/core.md) | `type`, conversions, lists, maps, `range`, `sort`, `clock` |
| [stdlib/string.md](stdlib/string.md) | String functions, interpolation, `format` |
| [stdlib/math.md](stdlib/math.md) | Math, random numbers, SIMD vectors, `mat_mul` |
| [stdlib/file.md](stdlib/file.md) | File I/O |
| [stdlib/gui.md](stdlib/gui.md) | 2D, 3D, input, audio, the sand plugin, constants |

## Internals

| Page | Covers |
|---|---|
| [internals/architecture.md](internals/architecture.md) | Pipeline and a map of every source file |
| [internals/vm.md](internals/vm.md) | Bytecode format, opcodes, frames, limits |
| [internals/gc.md](internals/gc.md) | Collector design, write barrier, tuning variables |
| [internals/arena.md](internals/arena.md) | AST arena |
| [internals/unsafe.md](internals/unsafe.md) | `unsafe` blocks and the 12 safety rules |
| [internals/performance.md](internals/performance.md) | How the speed was built: flags, layout, SIMD |

## Results

| Page | Covers |
|---|---|
| [results/gc-results.md](results/gc-results.md) | GC pauses, throughput and memory vs Go, Java G1, CPython, with history |
| [results/benchmarks.md](results/benchmarks.md) | `mat_mul` and `vec_mul` vs Python, NumPy, C |

## Project

| Page | Covers |
|---|---|
| [testing.md](testing.md) | Test layers and how to add tests |
| [../rust/readme.md](../rust/readme.md) | Rust runtime crates |
