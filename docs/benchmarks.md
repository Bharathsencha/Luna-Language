# Luna Native Performance Benchmarks

Luna vs C vs Python

_Test Environment: AMD Ryzen 7 7435HS, 24GB DDR5, Linux Mint. All C compiled with `gcc -O3 -march=native`._

---

## 1. Matrix Multiplication (300×300)

Luna delegates matrix multiplication to a natively optimized C-bridge using dense array internals. Pre-flattening Memory Arenas and OpenMP multi-threading vastly out-scale typical nested operations.

**Original Luna Benchmark (vs Python & NumPy):**

| Implementation | Time (s) | Speedup vs Python |
|----------------|----------|-------------------|
| Native Python | ~1.93s | 1x (Baseline) |
| NumPy C-Extension | ~0.013s | 148x |
| **Luna Native Bridge** | **~0.005s** | **386x** |

**Full Comparison (including C):**

| Implementation | Time (s) | vs Python |
|----------------|----------|-----------|
| Native Python | ~2.90s | 1x |
| NumPy C-Extension | ~0.013s | 223x |
| **C Native (-O3)** | **~0.006s** | **483x** |
| **Luna Native Bridge** | **~0.003s** | **967x** |

> Luna beats C here because `mat_mul()` uses OpenMP multi-threading internally. Single-threaded C would be closer to Luna's time.

**Run it yourself:**
```bash
# Luna
./bin/luna benchmark/luna_bench.lu

# C
gcc -O3 -march=native -o /tmp/matrix_c benchmark/matrix_native_c.c && /tmp/matrix_c

# Python
python3 benchmark/python_native.py
```

---

## 2. Vector Multiplication (1M Elements)

Luna leverages OpenMP SIMD macros and GCC auto-vectorization (AVX-512 capable) embedded in the native C bridge.

**Original Luna Benchmark (vs Python & NumPy):**

| Implementation | Time (s) | Speedup vs Python |
|----------------|----------|-------------------|
| Native Python | ~0.115s | 1x (Baseline) |
| NumPy SIMD | ~0.0019s | **60x faster** |
| **Luna SIMD (inline)** | **~0.00087s** | **132x faster** |

**Full Comparison (including C):**

| Implementation | Time (s) | vs Python |
|----------------|----------|-----------|
| Native Python | ~0.18s | 1x |
| NumPy SIMD | ~0.0019s | 95x |
| **C Native (-O3)** | **~0.001s** | **180x** |
| **Luna SIMD (inline)** | **~0.001s** | **180x** |

> Luna matches C because both compile down to the same AVX/SIMD instructions via GCC auto-vectorization. For a detailed breakdown of the architectural optimizations required to beat NumPy, see [Performance Architecture](performance_architecture.md).

**Run it yourself:**
```bash
# Luna
./bin/luna benchmark/vector_luna.lu

# C
gcc -O3 -march=native -o /tmp/vector_c benchmark/vector_native_c.c && /tmp/vector_c

# Python
python3 benchmark/vector_native.py
```

---

## 3. Environment Variable Lookups (1M Iterations)

Loop benchmark: 1M iterations of variable lookups + integer addition inside a function call.

**Original Interpreter Benchmark (Hash Table vs Python):**
Luna uses a djb2 Hash Table to look up variables in its scoping environment.

| Implementation | Time (s) | Relative Speed vs Luna |
|----------------|----------|------------------------|
| Luna Hash Table | ~0.40s | 1x (Baseline) |
| Python Native | ~0.028s | 14.2x faster |

> **Insight:** Python is highly optimized for local scope dictionary resolution with C-level dictionaries. Luna's Hash Table is a significant improvement over linear searching but represents an ongoing area for optimization.

**Full Comparison (including C):**

| Implementation | Time (s) | vs Interpreter |
|----------------|----------|----------------|
| C Native (-O3) | ~0.000s | ∞ (optimized away) |
| Python | ~0.050s | 7.8x faster |
| Luna Interpreter | ~0.39s | 1x |

> C shows ~0.000s because GCC detects the constant computation and eliminates the entire loop at compile time. The interpreter is ~8.5x slower due to AST tree-walking and hash table lookups per variable access.

**Run it yourself:**
```bash
# Luna Interpreter
./bin/luna benchmark/env_luna.lu



# C
gcc -O3 -march=native -o /tmp/env_c benchmark/env_native_c.c && /tmp/env_c

# Python
python3 benchmark/env_native.py
```

---

## Benchmark Files

| File | What it tests |
|------|--------------|
| `benchmark/luna_bench.lu` | 300×300 matrix multiply (Luna) |
| `benchmark/python_native.py` | 300×300 matrix multiply (Python) |
| `benchmark/vector_luna.lu` | 1M vector multiply (Luna SIMD) |
| `benchmark/vector_native.py` | 1M vector multiply (Python) |
| `benchmark/env_luna.lu` | 1M env lookups (Luna) |
| `benchmark/env_native.py` | 1M env lookups (Python) |


## Benchmark Results GO vs Luna

Measured by `make test-gc-three` (3-run averages; summary CSV in `stress_test/stress_results.csv`).
Luna runs on the bytecode VM.

| Benchmark | Language | User Time (s) | GC Max Pause (ms) | GC Total (ms) | Max RSS (MB) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **alloc_heavy** | Luna | 0.270 | **0.122** | 14.4 | 96.9 |
| | Go | 0.227 | 0.033 | 0.3 | 47.7 |
| **long_live** | Luna | 0.150 | **0.136** | 8.8 | 59.8 |
| | Go | 0.110 | 0.057 | 0.3 | 15.1 |
| **cycles** | Luna | 0.030 | **0.119** | 2.6 | 16.5 |
| | Go | 0.000 | 0.000 | 0.0 | 4.0 |
| **strings** | Luna | 0.200 | **0.108** | 8.5 | 83.4 |
| | Go | 0.163 | 0.037 | 0.3 | 65.0 |

### Reading the numbers

- **GC pauses**: Luna's worst-case pause is ~0.1–0.15ms on every workload — the same
  league as Go (0.03–0.06ms).
- **User time**: allocation-heavy workloads are within ~1.2–1.4x of Go (was 200–400x before
  the bytecode VM + hot-path work: fused string allocations, O(1) write barriers, cached
  interned names, O(1) box scope release).
- **GC total**: not directly comparable — Luna counts every incremental step; Go's gctrace
  totals only its two STW stop points per cycle.