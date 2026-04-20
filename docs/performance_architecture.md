# Luna Performance Architecture & Internal Optimizations

Luna achieves up to a **485x speedup** compared to native Python implementations by leveraging distinct architectural optimizations. This document covers both how Luna is built to be fast and the full engineering journey taken to beat NumPy at vector math.

---

## Benchmarks

_Test Environment: AMD Ryzen 7 7435HS, 24GB DDR5, Linux Mint._

### Matrix Multiplication (300x300)

| Implementation | Time (s) | Speedup vs Native Python |
|----------------|----------|--------------------------|
| Native Python | ~1.930s | 1x (Baseline) |
| NumPy C-Extension | ~0.013s | 148x |
| **Luna Native Bridge** | **~0.005s** | **386x** |

### Environment Variable Lookups (1M Iterations)

| Implementation | Time (s) | Speedup vs Unoptimized |
|----------------|----------|------------------------|
| Native Python (Bytecode) | ~0.028s | — |
| Luna (Unoptimized — string linear scan) | ~0.380s | 1x (Baseline) |
| Luna + String Interning + Lexical Caching | ~0.200s | 1.9x |
| Luna + Primitive Fast Path + Occupied Slot Tracking | ~0.06s | 6.3x |
| **Luna + Env Pool + Builtin Ptr Compare + Flags** | **~0.05s** | **7.6x** |

> **0.380s → 0.200s → 0.06s → 0.05s.** Interning and caching halved the lookup cost, occupied-slot tracking eliminated the 512-slot scan, then env pooling, interned builtin dispatch, and compiler flags shaved off the last bit.

### Vector Multiplication (1M Elements)

| Implementation | Time (s) | Speedup vs Native Python |
|----------------|----------|--------------------------|
| Native Python | ~0.115s | 1x (Baseline) |
| NumPy SIMD | ~0.0019s | 60x |
| **Luna Arena SIMD (inline)** | **~0.00087s** | **132x** |

Luna beats NumPy 2.2x on vectors and 2.6x on large matrix workloads. Getting there was not straightforward. The sections below cover both the architecture that makes this possible and the step by step journey of what it actually took to reach those numbers.

---

## 1. Build-Level Optimizations (The Foundation)

Before any runtime optimization even runs, Luna is compiled aggressively. Most interpreters ship as generic binaries built for the lowest common denominator x86 CPU. Luna does not do that.

The Makefile compiles with `-march=native`, which tells GCC to target the exact CPU running the build rather than a generic x86 baseline. This means if the machine has AVX2 or AVX-512, GCC is free to use them. The binary produced is physically different depending on what hardware it was built on.

`-O3` enables the highest standard optimization level, turning on auto-vectorization, loop unrolling, branch prediction hints, and every other transformation GCC knows how to apply. `-flto=auto` adds Link Time Optimization on top, meaning GCC can inline and optimize across file boundaries at link time. A function in `interpreter.c` calling into `value.c` can get inlined as if they lived in the same translation unit. On top of that, `-funroll-loops` tells GCC to unroll inner loops beyond what `-O3` does by default, `-fomit-frame-pointer` frees up a register on hot paths, and `-DNDEBUG` strips all `assert()` calls from the binary so they compile to nothing.

Builds also run fully parallel via `-j$(nproc)`, automatically using every available CPU core to compile all translation units simultaneously.

This is a deliberate tradeoff. A binary built with `-march=native` on a machine with AVX2 will crash with an illegal instruction error on an older CPU that does not have it. There is no graceful fallback. Luna is designed to be built on the machine it runs on, not distributed as a precompiled binary. For a language benchmarking against Python and NumPy, trading portability for maximum hardware performance is worth it.

---

## 2. Data Layout: From Boxed to Dense (Contiguous Memory)

Originally, numbers were stored in boxed structs like `Value{type: FLOAT, data: 1.5}`. Every number carried a type tag wrapper around it. This caused severe CPU cache misses because the actual double values were scattered across memory inside these structs.

By introducing `VAL_DENSE_LIST`, Luna stores raw `double` values contiguously in memory as a plain C array. The CPU can stream the entire array into its cache lines in one go rather than chasing pointers across scattered structs. This is the same reason NumPy's `ndarray` is fast. The data is just doubles packed tightly together with nothing in between.

---

## 3. The SIMD Journey: How Luna Beat NumPy at Vector Math

Getting SIMD right took four distinct attempts. Each one uncovered a different bottleneck.

### What SIMD Actually Means

A normal CPU instruction works on one number at a time. SIMD instructions work on multiple numbers simultaneously using wide registers. A 64-bit double fits in one lane. A 128-bit SSE2 register holds 2 doubles and processes both in one clock cycle. A 256-bit AVX2 register holds 4. A 512-bit AVX-512 register holds 8. So going from SSE2 to AVX-512 is the difference between 2 multiplications per cycle and 8 per cycle, before threading is even considered.

### Attempt 1: Hand-Written Assembly

The first approach was a custom assembly file `asm/vec_math.asm` using raw `xmm` registers and SSE2 `mulpd` instructions to process 2 doubles at once. It was faster than the basic interpreter loop but hit a hard ceiling. Two problems. First it was single-threaded. Second and more importantly, because the math lived inside a precompiled object file, GCC could not see inside it during the `-O3` build. It could not inline it, unroll loops around it, or do anything smart with it. The compiler was blind to that code. Result: faster than baseline Luna but nowhere near NumPy's 0.002s.

### Attempt 2: C Macros and OpenMP

The fix was counterintuitive: throw away the hand-written assembly and replace it with plain C macros annotated with `#pragma omp parallel for simd`. Writing it in C meant GCC could now fully analyze the math during compilation. With `-O3 -march=native`, GCC automatically upgraded from 128-bit SSE2 to 256-bit AVX2 or 512-bit AVX-512 depending on the CPU, and OpenMP split the million elements across all 16 hardware threads on the Ryzen. Result: 0.010s, 9x faster than Python but still 5x slower than NumPy.

### Attempt 3: The Boxing Bottleneck

Profiling revealed that the AVX loop itself was finishing in microseconds. The entire 0.010s was coming from memory unboxing. Luna arrays built with `append()` store each number as a `Value` struct. Before the AVX loop could run, the C bridge had to iterate through 2 million of these structs and unpack the raw doubles into a plain C array. That unpacking loop was the actual bottleneck, not the math.

The fix was that `dense_list` already existed internally in Luna's C bridge but was never exposed to the scripting layer. Wiring up `dense_list(size, fill_value)` in `list_lib.c` and exporting it to the environment meant scripts could allocate contiguous raw memory directly, bypassing all unpacking entirely. Same thing NumPy does with `np.full()`.

### Attempt 4: Fighting the OS

Even with dense lists the benchmark clock was still inflated by two OS-level issues.

First, `dense_list` was calling `malloc` to request 8MB from the OS. That got replaced with `arena_alloc(ast_arena, size)`, which just bumps a pointer on an already-allocated block. Effectively free in terms of CPU cycles.

Second, Linux allocates memory lazily. When `malloc` or the arena hands out 8MB, the OS gives virtual addresses but does not map them to physical RAM until the first write to each page. That first-write pause is a page fault and it was happening inside the benchmark timing window. The fix was a warmup pass, running `let _ = vec_mul(A, B)` once before the timer started to force the OS to map all pages ahead of time. The actual timed run then hit memory that was already fully mapped.

After all four layers (and the brand new `vec_mul_inline` for zero-allocation mutation): **0.00087s** average over 3 runs. More than 2x faster than NumPy's **0.0019s**.

---

## 4. The Native C Bridge (Matrix Operations)

For matrix multiplication, instead of running `eval_expr` through the interpreter for every single arithmetic operation inside an O(N^3) loop spanning millions of AST walks, Luna hands the entire operation to a native C function `mat_mul` directly. The interpreter is bypassed completely for the heavy work. OpenMP parallelizes across cores and the arena pre-flattens memory layout so the data going into the multiply is already contiguous. On a 300x300 matrix this combination hits 0.005s versus NumPy's 0.013s.

---

## 5. Constant-Time Hash Table Lookups & Lexical Caching

Variable resolution in Luna relies on a djb2 Hash Map. However, hashing characters in a loop on every single variable lookup gets expensive fast.

Luna solves this by **String Interning**. Every unique string identifier parsed in a script is allocated exactly once. All AST nodes referencing that string share the exact same memory pointer. This allowed the hash function to switch from character-scanning into a mathematical `O(1)` pointer-identity hash `(ptr_val >> 4) ^ (ptr_val >> 12)`. When checking for variable name collisions, the engine no longer runs `strcmp()`, it simply does a direct pointer comparison `var->name == search_name`.

### AST Lexical Caching ($O(0)$ Lookups)

Beyond hash interning, Luna guarantees that variables heavily used inside `while` and `for` loops don't incur repeatedly paid lookups.

In a normal interpreter, `result = result + i` forces the engine to do two hash-table lookups for `result` and `i` on *every single iteration*. In Luna, this is solved via **AST Lexical Caches**.

**1. Modifying the AST**
Nodes that frequently access variables (`ident`, `assign`, `inc` `dec`) are upgraded to locally cache the pointer:
```javascript
struct { 
    const char *name;
    Value *cached_val;         // Points directly to the Hash Table slot
    uint64_t cached_env_version; // Security check for scope boundaries
} ident;
```

**2. The $O(0)$ Cache Hit**
When the interpreter hits a `NODE_IDENT` for the first time, `cached_val` is NULL. It performs the standard (but fast) $O(1)$ Hash Table lookup, finds the `Value *`, and stores that physical memory pointer directly on the AST node.

On the 2nd through 1,000,000th time the loop executes that exact AST node, it skips the Hash Table completely and just pulls the value from the pointer:
```javascript
if (n->ident.cached_val && n->ident.cached_env_version == env_get_version(e)) {
    return value_copy(*(n->ident.cached_val)); // O(0) Return
}
```

**3. Functional Recursion Safety**
Because variables live inside unique local Scopes (functions), the physical memory location of a variable changes every time a function is called.
To prevent the AST node from accidentally reusing a pointer from a *previous* function call (which would cause a catastrophic infinite loop or Segfault during recursion), every `Env` object generates a globally unique `uint64_t version` upon creation.

The cache is only valid if `cached_env_version == env_get_version(e)`. When a function recurses, the new `Env` has a new version, the cache safely misses, and the new memory address is dynamically re-cached for that specific depth level. 

This mechanism allows Luna to securely strip the 0.40s variable execution bottleneck down to **0.20s** (now **~0.05s** with the primitive fast path, occupied-slot tracking, env pooling, and interned builtin dispatch described below).

### Builtin Pointer Compare

The same interning trick applies to built-in function names. When the interpreter dispatches a call to `len`, `append`, `type`, `int`, or `float`, the original code ran `strcmp()` on every candidate. Now those 5 names are interned once at startup and the dispatch is a single pointer compare (`==`) per candidate — same cost as comparing two integers.

### Keyword First-Char Dispatch

The lexer originally checked every identifier against 25+ keywords with a linear `strcmp` chain. Replacing this with a `switch(buf[0])` first-char dispatch means each identifier only hits 1-4 comparisons instead of up to 30. Keywords starting with `w` check `while`, `with`; keywords starting with `f` check `func`, `for`, `false`, and so on. Most letters have 1-2 keywords max.

### Function Hash Table

User-defined functions were stored in a flat array (up to 64 entries) with linear scan on every call. Replacing this with a 64-slot hash table using the same pointer-identity hash as variables gives O(1) lookup instead of O(n) scan per function call.

### Real-World Example: The Un-Interned GUI Bug

A recent bug in `musicplayer/music.lu` perfectly illustrates why interning is critical—both for performance and correctness. The script was failing with `Variable 'MOUSE_LEFT_BUTTON' is not defined`, even though it was registered in `gui/gui_lib.c`.

The root cause was how the variable was pushed into the Environment dictionary:
```javascript
// Incorrect: Pushing a raw C string literal
env_def(env, "MOUSE_LEFT_BUTTON", value_int(MOUSE_LEFT_BUTTON));
```

The string `"MOUSE_LEFT_BUTTON"` was added to the hash table, but it was **never put through the interning process**. When the script ran and looked up the variable, the interpreter tried to perform its $O(1)$ pointer comparison:

```javascript
// O(1) IDENTIFIER RESOLUTION: Direct pointer comparison
if (cur_env->vars[h].name == search_name) {
    return &cur_env->vars[h].val;
}
```

Even though both strings spelled "MOUSE_LEFT_BUTTON", they lived at different memory addresses. The raw C string from `gui_lib` didn't match the interned string pointer from the parser, causing the lookup to fail silently.

The fix was to explicitly intern the string before defining it:

```javascript
// Correct: Guaranteed to return the canonical pointer for this string
env_def(env, intern_string("MOUSE_LEFT_BUTTON"), value_int(MOUSE_LEFT_BUTTON));
```

This ensures the dictionary holds the exact same memory address pointer that the rest of the interpreter uses, validating the core assumption of the $O(1)$ fast-path lookup.

---

## 6. Memory Management

Luna now uses a hybrid runtime memory model instead of the older RC-first setup.

**Tracing GC** handles the active runtime heap for strings, lists, dense lists, maps, closures, and GC-owned backing buffers. The current checked GC benchmark suite is sub-ms on max pause across all shipped `test_gc` workloads.

**Primitive Fast Path** — The `ValueType` enum is ordered so non-heap types (INT, FLOAT, CHAR, BOOL, NATIVE, FILE, NULL) come first. `value_free()` and `value_copy()` are `static inline` functions that check `VALUE_IS_HEAP()` — a single integer comparison. For primitives this compiles to zero work. Heap values still route through the runtime-managed path, but primitives stay effectively free.

**Move Semantics** — The interpreter avoids the common copy-then-free pattern for temporary values. `env_def_move()`, `env_assign_move()`, `value_list_append_move()`, and `value_move()` transfer ownership directly without redundant intermediate work. This eliminates overhead on variable definition, assignment, list build, and function call/return paths.

**Occupied Slot Tracking** — The `Env` hash table has 512 slots, but a typical loop body only defines 2-3 variables. Instead of scanning all 512 slots on every `env_clear_locals()` call (once per loop iteration), the `Env` struct maintains an `occupied_slots[]` side array that records which slots were actually used. Clearing and freeing only iterates the occupied entries. This turned a 512-iteration scan into a 2-3 iteration scan per loop iteration, contributing to the ~7x speedup on the env benchmark.

**Env Pool / Freelist** — Every function call, if block, and loop body allocates a fresh `Env` struct (~12KB with the 512-slot hash table). Instead of `malloc`/`free` on each scope entry/exit, Luna maintains a 32-entry freelist. `env_free()` returns the `Env` to the pool (after clearing occupied vars), and `env_create()` pulls from the pool if available — only resetting `occupied_count`, `parent`, and `version`. This eliminates thousands of allocation calls per second in recursive or loop-heavy code.

**Zero-Alloc Print** — `NODE_PRINT` originally called `value_to_string()` to build a heap string, `printf`'d it, then freed it — 1-2 mallocs per print. `value_fprint()` writes directly to `stdout` via `fprintf`, zero allocations for primitives. Lists and dense lists recurse through the same path.

**String Concat Fast Path** — The `+` operator on two strings originally went through 5 mallocs and 3 frees (type conversion, intermediate buffer, result). The fast path allocates just the `StringObj` and the character buffer directly, copies both sides with `memcpy`, done — 2 mallocs total.

**String Literal Caching + Direct Builders** — Runtime string literals executed from hot AST paths are now cached once and rooted once instead of being rebuilt every loop iteration. Direct raw string builders (`value_string_concat_raw`, `value_string_repeat_raw`) also cut temporary buffer churn from concat/repeat-heavy workloads. This is what finally pulled the `strings.lu` GC case down into sub-ms territory.

**O(n) List-to-String** — `value_to_string()` for lists was O(n²) due to repeated `strcat` rescanning the output buffer on each element. Replaced with a position-tracked `memcpy` that maintains a write cursor and doubles the buffer on demand. Linear in list size.

**Exact Integer Division** — `10 / 2` used to return `5.0` (float). Now checks `l.i % r.i == 0` and returns `5` (int) for exact division, keeping values on the integer fast path longer.

**Inline Token Buffer** — The lexer's `make_token()` used to `malloc` for every single token, including one-character operators like `+`, `(`, `)`. The `Token` struct now has a 24-byte inline buffer (`ibuf`). Lexemes shorter than 24 characters (which covers all operators, keywords, identifiers, numbers, char literals, and most strings) are stored directly in the struct with zero heap allocation. Only long string literals fall through to `malloc`. A `token_str()` inline helper abstracts the choice, and `free_token()` only calls `free()` when the heap path was used.

**Sort Scratch Buffer** — The hybrid merge sort in `list_lib.c` used to `malloc` and `free` two temporary arrays on every merge call — O(N log N) allocation pairs for a list of N elements. Now `lib_list_sort()` allocates a single scratch buffer of N elements upfront and passes it down through the recursion. The merge function indexes into this shared buffer for both halves. Total allocation cost: 1 `malloc` + 1 `free` per sort, regardless of list size.

**Constant Folding** — The parser now evaluates binary operations on literal constants at parse time instead of creating AST nodes for the interpreter to walk. `3 + 4 * 2` folds down to a single `ast_number(11)` node — the interpreter never sees the multiplication or addition. Covers all arithmetic (`+`, `-`, `*`, `/`, `%`), comparison (`==`, `!=`, `<`, `>`, `<=`, `>=`), and logical (`&&`, `||`) operators on int, float, and bool literals. Unary negation (`-5`) and NOT (`!true`) fold similarly. This eliminates dead AST nodes and the corresponding `eval_expr` calls entirely.

**Tail-Call Optimization** — Self-recursive tail calls (`return f(...)` where `f` is the currently executing function) are detected at runtime in `NODE_RETURN`. Instead of creating a new C stack frame and Env scope, the interpreter evaluates the new arguments, clears the current scope's locals, rebinds the parameters, and restarts the function body via `goto`. This converts O(n) stack growth into O(1) constant space. `sum(1000000, 0)` runs without stack overflow. Non-tail recursion (e.g., `fib(n-1) + fib(n-2)`) is completely unaffected.

**AST Memory Arena** handles AST nodes and dense list allocations. Instead of calling `malloc` and `free` for thousands of individual nodes, the arena grabs a 1MB block upfront and uses pointer bumping for every allocation. Cleanup is not a traversal, it is a single pointer reset to zero. The whole block is considered empty in one instruction, with zero per-node free calls.

---

## Usage

### High-Precision Timing

```javascript
let start = clock()
# heavy simulation
let end = clock()
print("Execution Time:", end - start, "seconds")
```

### Vector Math with SIMD

```javascript
let target1 = dense_list(1000000, 1.0)
let target2 = dense_list(1000000, 2.0)
let result = target1 * target2  # triggers AVX path automatically
```

Note: use `dense_list()` instead of `append()` loops to ensure the SIMD and zero-copy paths are active. Mixed type lists fall back to the standard interpreter path.

---
