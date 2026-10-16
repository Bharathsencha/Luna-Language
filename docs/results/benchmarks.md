# Native Math Benchmarks

Throughput of Luna's native vector and matrix builtins against Python, NumPy and C. GC
measurements are on a separate page: [gc-results.md](gc-results.md).

Machine: AMD Ryzen 7 7435HS, 24 GB DDR5, Linux Mint. C was built with
`gcc -O3 -march=native`. These runs predate the bytecode VM. They time the native builtins
themselves, which the VM does not change.

## Matrix multiplication, 300x300

| Implementation | Time (s) | vs Python |
|---|---:|---:|
| Python (nested lists) | 2.90 | 1x |
| NumPy | 0.013 | 223x |
| C, single thread, `-O3` | 0.006 | 483x |
| Luna `mat_mul` | 0.003 | 967x |

`mat_mul` beats single-threaded C because it splits rows across cores with OpenMP. A
multithreaded C version would match it.

## Element-wise vector multiply, 1M floats

| Implementation | Time (s) | vs Python |
|---|---:|---:|
| Python (list comprehension) | 0.18 | 1x |
| NumPy | 0.0019 | 95x |
| C, `-O3` | 0.001 | 180x |
| Luna `vec_mul` on `dense_list` | 0.001 | 180x |

Luna matches C because both compile to the same auto-vectorized AVX loop. The prerequisite is
`dense_list`, which stores raw doubles contiguously instead of boxed `Value`s. See
[internals/performance.md](../internals/performance.md) for how the layout got there.

## Variable lookup loop, 1M iterations

This is the weak case: 1M iterations of a local lookup plus integer add, inside a function.

| Implementation | Time (s) |
|---|---:|
| Python | 0.050 |
| Luna, tree-walking interpreter | 0.39 |
| C | about 0 (loop folded at compile time) |

This was measured on the old tree-walking interpreter. The bytecode VM keeps locals in
registers in the call frame, and these numbers have not been re-measured on it.

## Reproducing

The benchmark scripts (`benchmark/luna_bench.lu`, `vector_luna.lu`, `env_luna.lu`, and the Zig
counterparts run by `make run-comp`) are on `main`, not `luna-beta`.
