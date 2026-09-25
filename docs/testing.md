# Luna Testing Infrastructure & Test Suite Documentation

Comprehensive guide to the testing infrastructure, verification harnesses, GC stress suites, and performance benchmarking workflows in the Luna Language repository.

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Quick Start: Test Commands](#2-quick-start-test-commands)
3. [Test Runner Infrastructure (`test_runner.sh`)](#3-test-runner-infrastructure-test_runnersh)
4. [GC Stress & Allocation Workloads (`test_gc/`)](#4-gc-stress--allocation-workloads-test_gc)
   - [4.1 `alloc_heavy.lu` — Allocation Throughput & Heap Expansion](#41-alloc_heavylu--allocation-throughput--heap-expansion)
   - [4.2 `cycles.lu` & `cycles_5k.lu` — Cyclic Graph Collection](#42-cycleslu--cycles_5klu--cyclic-graph-collection)
   - [4.3 `strings.lu` — String Churn & Interning Overhead](#43-stringslu--string-churn--interning-overhead)
   - [4.4 `binary_trees.lu` — Recursive Tree Churn & Stack Roots](#44-binary_treeslu--recursive-tree-churn--stack-roots)
   - [4.5 `long_live.lu` — Generational Survival & Ephemeral Churn](#45-long_livelu--generational-survival--ephemeral-churn)
   - [4.6 `map_churn.lu` — Hash Map Churn & Tombstone Reclamation](#46-map_churnlu--hash-map-churn--tombstone-reclamation)
   - [4.7 `object_graph.lu` — Dynamic Edge Mutation & SATB Barrier](#47-object_graphlu--dynamic-edge-mutation--satb-barrier)
   - [4.8 `concurrent_map.lu` & `test_inc.lu` — Incremental Stepping](#48-concurrent_maplu--test_inclu--incremental-stepping)
5. [Language Functional Test Suite (`test/`)](#5-language-functional-test-suite-test)
   - [5.1 Core Types & Primitives (`test_core.lu`)](#51-core-types--primitives-test_corelu)
   - [5.2 String Library & Interpolation (`test_strings.lu`)](#52-string-library--interpolation-test_stringslu)
   - [5.3 Math & Vector SIMD (`test_math.lu`, `test_vectors.lu`)](#53-math--vector-simd-test_mathlu-test_vectorslu)
   - [5.4 Functions, Closures & Recursion (`test_functions.lu`)](#54-functions-closures--recursion-test_functionslu)
   - [5.5 Structured Memory Primitives (`test_data_types.lu`, `test_bloc.lu`, `test_box.lu`, `test_template.lu`)](#55-structured-memory-primitives-test_data_typeslu-test_bloclu-test_boxlu-test_templatelu)
   - [5.6 Unsafe Memory & Pointer Safety (`test_unsafe.lu`)](#56-unsafe-memory--pointer-safety-test_unsafelu)
   - [5.7 File I/O Subsystem (`test_file_io.lu`)](#57-file-io-subsystem-test_file_iolu)
   - [5.8 Module System & Import Validation (`test_modules.lu`, `test_import_private_error.lu`)](#58-module-system--import-validation-test_moduleslu-test_import_private_errorlu)
   - [5.9 Quality of Life & Simulation (`test_qol.lu`, `balls.lu`)](#59-quality-of-life--simulation-test_qollu-ballslu)
6. [GC Safety & Verification Harness](#6-gc-safety--verification-harness)
7. [Comparative Benchmark Infrastructure (`stress_test/` & `test_gc/`)](#7-comparative-benchmark-infrastructure-stress_test--test_gc)
8. [Host-Level Zig & Native Rust Testing](#8-host-level-zig--native-rust-testing)
9. [Developer Guide: Writing & Adding Tests](#9-developer-guide-writing--adding-tests)

---

## 1. Architecture Overview

The Luna Language test infrastructure is split into distinct, complementary layers designed to validate functional correctness, memory safety, GC pauses, interpreter execution speed, and low-level compiler invariants:

```
                      +-------------------------------------------------------+
                      |                 Luna Test Ecosystem                   |
                      +-------------------------------------------------------+
                                                  |
         +--------------------+-------------------+--------------------+--------------------+
         |                    |                   |                    |                    |
         v                    v                   v                    v                    v
+------------------+ +------------------+ +------------------+ +------------------+ +------------------+
| Language Tests   | | GC Stress Tests  | | Cross-Bench Suite| | Host-Side Zig    | | Rust Runtimes    |
| (`test/*.lu`)    | | (`test_gc/*.lu`) | | (`stress_test/`) | | (`zig-test/`)    | | (`rust/*/`)      |
+------------------+ +------------------+ +------------------+ +------------------+ +------------------+
| - `test_runner`  | | - alloc_heavy    | | - Luna vs Go     | | - Lexer tokens   | | - `unsafe_rt`    |
| - Golden diffs   | | - cycles (5k)    | | - Luna vs Java G1| | - AST shape      | | - `data_rt`      |
| - Split Golden   | | - strings churn  | | - Luna vs Python | | - Bytecode chunk | | - Cargo unit     |
| - Exit assertions| | - binary_trees   | | - RSS / Pauses   | | - Native bridge  | |   tests          |
+------------------+ +------------------+ +------------------+ +------------------+ +------------------+
```

### Design Philosophy

- **End-to-End Behavioral Correctness (`test/`)**: Executes real `.lu` scripts against the compiled `bin/luna` binary. Tests use language-level `assert()` or compare compiler error output against `.expect` golden files.
- **Collector Stress & Pause Minimization (`test_gc/`)**: Simulates extreme runtime heap patterns (heavy allocation loops, cyclic graphs, string transformations, deep trees) to enforce sub-millisecond GC pauses and prevent memory leaks.
- **Cross-Language Rigor (`stress_test/`)**: Evaluates Luna's memory footprint (Max RSS), CPU user time, and GC pause distribution against Go, Java (G1), and CPython on identical workloads.
- **Host Subsystem Verification (`zig-test/`)**: Directly inspects C-engine memory structures (tokens, AST node variants, VM chunks, environment scopes) to pinpoint exact compiler regressions.
- **Native Memory Runtime Isolation (`rust/`)**: Validates unmanaged pointer operations and structured layout data crates via standard Rust test harnesses.

---

## 2. Quick Start: Test Commands

All test workflows are automated through top-level `Makefile` targets:

```bash
# Run the complete test suite (Core scripts, test_runner.sh, Rust data/unsafe runtimes)
make test

# Run the standard GC benchmark suite (alloc_heavy, long_live, cycles, strings)
make test-gc

# Run GC safety tests under forced stress and heap validation (LUNA_GC_STRESS=1 LUNA_GC_VERIFY=1)
make test-gc-safety

# Run the 3-iteration Luna vs Go stress comparison suite
make test-gc-three

# Run the Luna vs Java (G1 GC) stress comparison suite
make test-java-gc

# Run the Luna vs Python (CPython) comparison suite
make test-py

# Run the unified multi-language GC benchmark suite (Luna vs Go vs Java vs Python)
make test-gc-all

# Run host-level Zig integration tests
make zig-test

# Run Rust structured data runtime tests
make test-rust-data

# Run Luna vs Zig native performance benchmarks
make run-comp
```

---

## 3. Test Runner Infrastructure (`test_runner.sh`)

The central test orchestrator for `.lu` files is `test_runner.sh`. It automatically iterates over all test scripts in `test/` and categorizes execution based on test type:

```
                +------------------------------------+
                |  Discovers test/*.lu test scripts  |
                +------------------------------------+
                                  |
                +-----------------+-----------------+
                | Is base_name test_gc_*?           |
                | -> Set LUNA_GC_STRESS=1 & VERIFY=1|
                +-----------------+-----------------+
                                  |
       +--------------------------+--------------------------+
       |                                                     |
       v                                                     v
+-----------------------------+               +-----------------------------+
| .expect file exists?        |               | No .expect file             |
+-----------------------------+               +-----------------------------+
       |                                                     |
       +------------+------------+                           v
       |                         |             +-----------------------------+
       v                         v             | Assertion Test:             |
+----------------+      +----------------+     | Execute: bin/luna <src>     |
| Single Golden  |      | Split Golden   |     | Verify exit code == 0       |
| Diff stdout/   |      | (`# CASE <id>`)|     +-----------------------------+
| stderr vs      |      | Parse & diff   |
| <src>.expect   |      | each block     |
+----------------+      +----------------+
```

### 1. Assertion Tests (Exit Code Verification)
For standard `.lu` test scripts without an accompanying `.expect` file, `test_runner.sh` executes the script directly:
- The script uses built-in `assert(condition)` statements.
- If an assertion fails, the Luna runtime terminates with a non-zero exit code and writes diagnostic information to `stderr`.
- `test_runner.sh` records a `[PASS]` if the exit code is `0`, or `[FAIL]` and dumps `stderr` if non-zero.

### 2. Golden File Tests (Output Matching)
When a test script `test/<name>.lu` has a matching `test/<name>.expect`:
- The script is executed, redirecting combined `stdout` and `stderr` to a temporary output buffer.
- `diff -q --strip-trailing-cr` performs a strict diff against `test/<name>.expect`.
- If the output matches, `[PASS] <name> (Output Match)` is emitted. If diffs are detected, the runner outputs the expected vs. actual output and increments the failure counter.

### 3. Split Golden Tests (`# CASE <name>`)
For error testing scripts containing multiple isolated error scenarios in a single file (such as `test_unsafe_errors.lu`):
- The runner scans for `# CASE <name>` delimiters using `awk`.
- It isolates each sub-case, runs it individually through `bin/luna`, and compares against the matching `# CASE <name>` segment in the `.expect` file.
- Guarantees that compilation errors in one section do not abort testing of subsequent error cases.

### 4. Automated GC Stress Injection
If a test file name begins with `test_gc_*`, `test_runner.sh` automatically wraps the invocation:
```bash
env LUNA_GC_STRESS=1 LUNA_GC_VERIFY=1 ./bin/luna "$src"
```
This forces the garbage collector to execute at every memory allocation and validates all object pointers in the heap.

---

## 4. GC Stress & Allocation Workloads (`test_gc/`)

The `test_gc/` directory houses specialized workloads engineered to stress distinct aspects of Luna's tri-color incremental tracing collector, string interning cache, hash map implementation, and generational promotion mechanics.

```
+-------------------+-------------------------------------------------------------------------+
| Benchmark File    | Primary GC Mechanics & Stress Characteristics                           |
+-------------------+-------------------------------------------------------------------------+
| alloc_heavy.lu    | Rapid allocation throughput (750k strings), nursery expansion rate      |
| cycles.lu         | 50k node cyclic ring graph; tests non-refcounted mark-sweep reclamation |
| cycles_5k.lu      | 5k node variant for rapid regression cycles                             |
| strings.lu        | 500k heavy string concatenations; heap pressure & string interning      |
| binary_trees.lu   | Deep recursion (depth 17), transient tree churn, stack root scanning   |
| long_live.lu      | 200k long-lived objects vs 400k transient scratch objects               |
| map_churn.lu      | 2M map ops (set/delete/get); bucket resizing & tombstone reclamation    |
| object_graph.lu   | 100k interconnected nodes with dynamic edge mutation; SATB write barrier|
| concurrent_map.lu | Map stress operations under incremental collector stepping              |
| test_inc.lu       | Low-level incremental GC step verification                              |
+-------------------+-------------------------------------------------------------------------+
```

---

### 4.1 `alloc_heavy.lu` — Allocation Throughput & Heap Expansion

[alloc_heavy.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test_gc/alloc_heavy.lu) evaluates raw allocation speed, heap growth pacing, and nursery collector throughput.

```luna
let start = clock()
let items = []
for (let i = 0; i < 750000; i++) {
    append(items, repeat("item-", 2) + to_string(i))
}
print("alloc_heavy", len(items))
print("Time:", clock() - start, "seconds")
```

#### What It Tests:
- **Monotonic Heap Expansion**: Generates 750,000 distinct strings appended into a growing dynamic array.
- **Nursery Allocation & Minor GCs**: Fast-path object creation in the nursery without fragmentation.
- **Amortized Array Resizing**: Tests backing buffer reallocations (`capacity *= 2`) during continuous GC activity.
- **Throughput vs Latency**: Measures total user CPU time to verify Luna's memory allocators avoid lock contention and overhead.

---

### 4.2 `cycles.lu` & `cycles_5k.lu` — Cyclic Graph Collection

[cycles.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test_gc/cycles.lu) and `cycles_5k.lu` verify that circular object graphs are collected correctly without memory leaks.

```luna
template Node { name, next }

let start = clock()
let nodes = []
for (let i = 0; i < 50000; i++) {
    append(nodes, Node("n" + to_string(i), null))
}

for (let i = 0; i < len(nodes); i++) {
    let nextI = (i + 1) % len(nodes)
    nodes[i].next = nodes[nextI]
}

print("cycles", len(nodes))
print("Time:", clock() - start, "seconds")
```

#### What It Tests:
- **Cycle Handling (Reference Counting vs Tracing GC)**: Reference counting implementations (such as CPython) fail to collect circular structures without a dedicated cyclic detector. Luna's tri-color tracing collector traverses pointer graphs from root sets, detecting live nodes and reclaiming abandoned cycles.
- **No Infinite Mark Loops**: Tests that the marking phase properly tags visited nodes as `BLACK` to avoid infinite loops when traversing circular topologies (`nodes[i].next = nodes[i+1]`).
- **Tri-color Invariants**: Ensures pointers within cyclic structures remain valid throughout multi-step incremental mark cycles.

---

### 4.3 `strings.lu` — String Churn & Interning Overhead

[strings.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test_gc/strings.lu) stresses the string allocator, heap defragmentation, and string table recycling.

```luna
let start = clock()
let out = []
for (let i = 0; i < 500000; i++) {
    append(out, repeat("luna", 16) + "-" + to_string(i))
}
print("strings", len(out))
print("Time:", clock() - start, "seconds")
```

#### What It Tests:
- **String Header + Payload Churn**: Allocates 500,000 strings with 64-byte repeated prefixes plus formatted integer tails (`lunalunaluna...-12345`).
- **Interning Cache & Hash Collisions**: Ensures string intern pools and memory allocations handle large volumes of unique string objects without degrading lookup times.
- **Fused String Allocations**: Validates that Luna's single-allocation string representation (header + string data in contiguous memory) avoids pointer indirection overhead.

---

### 4.4 `binary_trees.lu` — Recursive Tree Churn & Stack Roots

[binary_trees.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test_gc/binary_trees.lu) tests deep recursive allocation patterns and stack-root scanning.

```luna
template Node { left, right, value }

func build(depth) {
    if (depth == 0) {
        return null
    }
    return Node(build(depth - 1), build(depth - 1), 1)
}

func checksum(node) {
    if (node == null) {
        return 0
    }
    return node.value + checksum(node.left) + checksum(node.right)
}

let start = clock()
let total = 0
for (let i = 0; i < 12; i++) {
    let root = build(17)
    total = total + checksum(root)
    root = null
}
print("binary_trees", total)
print("Time:", clock() - start, "seconds")
```

#### What It Tests:
- **Deep Recursive Stack Traversal**: Builds full binary trees of depth 17 ($2^{18}-1 = 262,143$ nodes per tree) across 12 consecutive iterations ($>3.14 \times 10^6$ nodes total).
- **Stack-Root Scanning**: Verifies that the GC accurately scans interpreter stack frames and call frames during recursive function calls.
- **Bulk Deallocation & Tree Reclamation**: Setting `root = null` renders the entire tree unreachable; tests the collector's ability to sweep massive transient subtrees cleanly.

---

### 4.5 `long_live.lu` — Generational Survival & Ephemeral Churn

[long_live.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test_gc/long_live.lu) simulates real-world application state: long-lived retained data structures surrounded by high-frequency temporary churn.

```luna
let start = clock()
let table = []
for (let i = 0; i < 200000; i++) {
    append(table, repeat("value-", 2) + to_string(i))
}
for (let j = 0; j < 400000; j++) {
    let _ = repeat("scratch-", 2) + to_string(j)
}
print("long_live", len(table))
print("Time:", clock() - start, "seconds")
```

#### What It Tests:
- **Generational GC Mechanics**: The first loop allocates 200,000 persistent items into `table` (which survive all subsequent collections). The second loop allocates 400,000 temporary `scratch` strings that die immediately.
- **Remembered Sets & Old Generation**: Verifies that long-lived objects promoted to older generations are not needlessly rescanned on every minor GC cycle.
- **Heap Partitioning**: Confirms that resident memory remains bounded and old-generation objects do not cause fragmentation.

---

### 4.6 `map_churn.lu` — Hash Map Churn & Tombstone Reclamation

[map_churn.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test_gc/map_churn.lu) subjects Luna's hash map implementation to 4,000,000 key insertions, lookups, and deletions.

```luna
let start = clock()
let m = {}
for (let i = 0; i < 2000000; i++) {
    let key = to_string((i * 2654435761) % 100000)
    map_set(m, key, i)
}
for (let i = 0; i < 2000000; i++) {
    let key = to_string((i * 2654435761) % 100000)
    let op = i % 3
    if (op == 0) {
        map_set(m, key, i)
    } else if (op == 1) {
        map_delete(m, key)
    } else {
        let v = map_get(m, key)
    }
}
print("map_churn", len(m))
print("Time:", clock() - start, "seconds")
```

#### What It Tests:
- **Hash Table Tombstone Reclamation**: High-churn insertions and deletions create tombstones in open-addressing hash maps. This test ensures deleted keys are reclaimed without causing bucket degradation.
- **Rehashing Under GC**: Verifies that table growth and rehashing operate safely while GC triggers.
- **Fibonacci Multiplicative Hashing**: Uses Knuth's multiplicative hash constant (`2654435761`) to distribute keys uniformly across buckets.

---

### 4.7 `object_graph.lu` — Dynamic Edge Mutation & SATB Barrier

[object_graph.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test_gc/object_graph.lu) constructs complex directed graphs and continuously mutates edges to test write-barrier invariants.

```luna
template Node { edges, value }

let start = clock()
let nodes = []
for (let i = 0; i < 100000; i++) {
    append(nodes, Node([], i))
}
for (let i = 0; i < 100000; i++) {
    let n = nodes[i]
    for (let e = 0; e < 4; e++) {
        let other = nodes[(i * 31 + e * 17) % 100000]
        append(n.edges, other)
    }
}
for (let i = 0; i < 10000; i++) {
    let a = nodes[(i * 97) % 100000]
    let b = nodes[(i * 53) % 100000]
    if (len(a.edges) > 0) {
        a.edges[(i * 13) % len(a.edges)] = b
    }
}
let sum = 0
for (let i = 0; i < 100000; i++) {
    sum = sum + len(nodes[i].edges)
}
print("object_graph", sum)
print("Time:", clock() - start, "seconds")
```

#### What It Tests:
- **Snapshot-At-The-Beginning (SATB) Write Barrier**: When mutator threads overwrite pointer fields (`a.edges[...] = b`) during concurrent or incremental marking, overwritten references must be marked to prevent live objects from being collected.
- **High-Density Graph Tracing**: 100,000 nodes with 400,000 directed edges verify pointer traversal throughput and mark stack capacity.

---

### 4.8 `concurrent_map.lu` & `test_inc.lu` — Incremental Stepping

- [concurrent_map.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test_gc/concurrent_map.lu): Validates hash map lookups, mutations, and insertions under concurrent workload patterns and incremental collector stepping.
- [test_inc.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test_gc/test_inc.lu): Low-overhead micro-test designed for rapid verification of incremental GC step boundaries.

---

## 5. Language Functional Test Suite (`test/`)

The `test/` directory contains functional regression test suites covering language syntax, semantics, standard libraries, structured data types, unsafe blocks, and the module system.

```
+-----------------------------+-----------------------------------------------------------------------+
| Test File                   | Covered Language Features & Subsystems                                |
+-----------------------------+-----------------------------------------------------------------------+
| test_core.lu                | Primitives, types, multi-assignment, ASCII, escapes, operators        |
| test_strings.lu             | String stdlib (trim, slice, pad, search, split/join, interpolation)   |
| test_math.lu                | Arithmetic, trig, abs/floor/ceil, precedence, logical truthiness      |
| test_vectors.lu             | Vector math (vec_add, vec_sub, vec_mul, vec_dot), SIMD intrinsics    |
| test_functions.lu           | First-class functions, closures, recursion, variable shadowing        |
| test_data_types.lu          | Structured data integration: data, bloc, box, template                |
| test_bloc.lu                | Stack-allocated value-types (bloc), field access, value equality      |
| test_box.lu                 | Fixed-capacity memory buffers (box[N]), free(), scope-exit cleanup    |
| test_template.lu            | Object templates, field mutation, index access, shape introspection   |
| test_unsafe.lu              | Unsafe blocks, raw pointers (alloc, store, load, ptr_offset, defer)   |
| test_file_io.lu             | File handles, read/write/append, line reading, defer(close)           |
| test_modules.lu             | use { ... } from "...", exports, module caching, cross-module state   |
| test_import_private_error.lu| Golden error matching for private symbol import enforcement           |
| test_qol.lu                 | Augmented assignment (+=, -=), negative indexing, for-in loops        |
| test_gc_safety.lu           | GC stress safety with closures, map roots, and object preservation    |
| test_gc_large_safety.lu     | GC safety with multi-kilobyte payloads and deep reference graphs      |
| balls.lu                    | Physics simulation integration test                                   |
+-----------------------------+-----------------------------------------------------------------------+
```

---

### 5.1 Core Types & Primitives (`test_core.lu`)

[test_core.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test/test_core.lu) tests fundamental language types and syntax:

```luna
# Type introspection & literal definitions
let x = 10
let y = 1.5
let flag = true
let name = "Luna"
let empty = null

assert(type(10) == "int")
assert(type(1.5) == "float")
assert(type(true) == "boolean")
assert(type(null) == "null")

# 64-bit integer promotion
let big = 9999999999
assert(type(big) == "long")

# Character type & ASCII conversions
let ch = 'A'
assert(type(ch) == "char")
assert(int('A') == 65)

# Multiple assignment
let a, b, c = 10, 1.1, "Hello"
assert(a == 10 && b == 1.1 && c == "Hello")
```

---

### 5.2 String Library & Interpolation (`test_strings.lu`)

[test_strings.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test/test_strings.lu) tests string inspection, transformation, searching, and interpolation:

```luna
let s = "  Hello World  "

# Inspection & Trimming
assert(len(s) == 15)
assert(trim(s) == "Hello World")
assert(trim_left("  a") == "a")

# Slicing & Transformations
assert(substring("Hello", 0, 2) == "He")
assert(slice("Hello", -3, -1) == "ll")
assert(to_upper("abc") == "ABC")
assert(reverse("Luna") == "anuL")
assert(repeat("Na", 4) == "NaNaNaNa")

# Searching & Split/Join
assert(contains("Team", "ea") == true)
assert(index_of("banana", "nan") == 2)
let list = split("apple,banana,cherry", ",")
assert(join(list, " | ") == "apple | banana | cherry")

# String Interpolation with expressions and function calls
let pilot = "Luna"
let items = [7, 11]
func plus(a, b) { return a + b }

assert("Hello {pilot}" == "Hello Luna")
assert("sum={1 + 2}" == "sum=3")
assert("slot={items[0]}" == "slot=7")
assert("call={plus(3, 4)}" == "call=7")
```

---

### 5.3 Math & Vector SIMD (`test_math.lu`, `test_vectors.lu`)

- [test_math.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test/test_math.lu): Validates arithmetic operator precedence, modulo, comparison operators, and boolean short-circuit evaluation (`&&`, `||`, `and`, `or`, `not`).
- [test_vectors.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test/test_vectors.lu): Validates vector addition, subtraction, multiplication, and matrix transformations:

```luna
let A = [10, 20, 30, 40]
let B = [2, 4, 5, 8]

let sum = vec_add(A, B)  # [12, 24, 35, 48]
let sub = vec_sub(A, B)  # [8, 16, 25, 32]
let mul = vec_mul(A, B)  # [20, 80, 150, 320]

assert(sum[0] == 12 && sum[3] == 48)
assert(mul[0] == 20 && mul[3] == 320)
```

---

### 5.4 Functions, Closures & Recursion (`test_functions.lu`)

[test_functions.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test/test_functions.lu) tests execution semantics for first-class functions:

```luna
# Closures capturing local environment
func outer() {
    func inner(val) {
        return val * 2
    }
    return inner(50)
}
assert(outer() == 100)

# Recursion (Fibonacci & Factorial)
func factorial(n) {
    if (n <= 1) { return 1 }
    return n * factorial(n - 1)
}
assert(factorial(5) == 120)

# Lexical shadowing
let global_var = 100
func shadow_test() {
    let global_var = 999
    return global_var
}
assert(shadow_test() == 999)
assert(global_var == 100)
```

---

### 5.5 Structured Memory Primitives (`test_data_types.lu`, `test_bloc.lu`, `test_box.lu`, `test_template.lu`)

Luna provides four structured data primitives:

```
+-----------+-------------------------+----------------------+----------------------------------------+
| Primitive | Memory Backing          | Allocation Domain    | Lifetime & Semantics                   |
+-----------+-------------------------+----------------------+----------------------------------------+
| `data`    | Hash-map backed         | GC Heap              | Dynamic key-value structured record    |
| `bloc`    | Contiguous value struct | Stack / Inlined      | Zero-overhead value type, deep equality|
| `box`     | Contiguous array block  | Unmanaged / Arena    | Fixed capacity, RAII/free(), fast index|
| `template`| Class-like object slot  | GC Heap              | Named fields, dynamic mutation, shape  |
+-----------+-------------------------+----------------------+----------------------------------------+
```

#### `bloc` Value Structs ([test_bloc.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test/test_bloc.lu)):
```luna
bloc Vec2 { x, y }
bloc Rect { min, max }

let p = Vec2{3, 4}
let q = Vec2{3, 4}
let r = Vec2{5, 6}

assert(p.x == 3 && p.y == 4)
assert(p == q)        # Structural value equality
assert(p != r)

let rect = Rect{Vec2{0, 0}, Vec2{10, 20}}
assert(shape(rect.min) == "Vec2")
```

#### `box` Fixed Memory Blocks ([test_box.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test/test_box.lu)):
```luna
let buf = box[16]
assert(buf.len == 16)

let alias = buf
free(buf)              # Explicit deallocation

assert(alias.len == 0) # Aliases are immediately invalidated

# Lexical scope escape invalidation
let escaped = null
if (true) {
    let scoped = box[8]
    escaped = scoped
}
assert(escaped.len == 0) # Invalidated upon exiting defining scope
```

#### `template` Prototypes ([test_template.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test/test_template.lu)):
```luna
template Person { name, hp, pos }
let p = Person("Hero", 100, [0, 0])

assert(p.name == "Hero")
p.hp -= 20
assert(p.hp == 80)
assert(shape(p) == "Person")
```

---

### 5.6 Unsafe Memory & Pointer Safety (`test_unsafe.lu`)

[test_unsafe.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test/test_unsafe.lu) tests raw pointer operations within isolated `unsafe` blocks:

```luna
unsafe {
    let value = 7
    let value_ptr = address_of(value)
    let buf = alloc(2)
    let next = ptr_offset(buf, 1)

    store(value_ptr, 11)
    store(buf, "left")
    store(next, "right")

    assert(value == 11)
    assert(load(buf) == "left")
    assert(load(next) == "right")
    assert(type(buf) == "pointer")
    assert(next > buf)

    defer(buf)  # Unsafe buffer deallocation
}
```

---

### 5.7 File I/O Subsystem (`test_file_io.lu`)

[test_file_io.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test/test_file_io.lu) verifies file system handles, stream reads/writes, line parsing, and `defer` cleanup:

```luna
let filename = "test_output.txt"
if (file_exists(filename)) {
    remove_file(filename)
}

func write_with_defer(path, text) {
    let handle = open(path, "w")
    assert(handle)
    defer(close(handle))
    assert(write(handle, text))
}

write_with_defer(filename, "Deferred Hello\n")
assert(file_exists(filename))

let f = open(filename, "r")
let content = read(f)
assert(content == "Deferred Hello\n")
close(f)
remove_file(filename)
```

---

### 5.8 Module System & Import Validation (`test_modules.lu`, `test_import_private_error.lu`)

- [test_modules.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test/test_modules.lu): Verifies module path resolution, single-execution caching, constant re-exports, and cross-module persistent state:

```luna
use { BASE, scale, bump } from "modules_fixtures/math_mod.lu"
use { scaled, bump_twice } from "modules_fixtures/sub/wrapper.lu"

assert(BASE == 10)
assert(scale(4) == 40)
assert(scaled(4) == 41)

# Module state persistence across call boundaries
assert(bump() == 1)
assert(bump() == 2)
assert(bump_twice() == 4)
```

- [test_import_private_error.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test/test_import_private_error.lu): Ensures compiler diagnostics cleanly reject attempts to import non-exported symbols. Tested against [test_import_private_error.expect](file:///c:/Users/adwai/LUNA/Luna-Language/test/test_import_private_error.expect):

```
Name Error in test/import_helper.lu at line 2:
  Variable 'private_func' is not defined
Hint: Declare variables with 'let' before using them
```

---

### 5.9 Quality of Life & Simulation (`test_qol.lu`, `balls.lu`)

- [test_qol.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test/test_qol.lu): Tests negative indexing (`l[-1]`), compound assignment operators (`+=`, `-=`, `*=`, `/=`), default function arguments (`func greet(name, greeting = "Hello")`), and `for (let item in list)` loops.
- [balls.lu](file:///c:/Users/adwai/LUNA/Luna-Language/test/balls.lu): Integration test exercising vector updates, math operations, and array manipulation in a physics loop.

---

## 6. GC Safety & Verification Harness

Luna features dedicated GC safety suites executed under aggressive stress flags:

```bash
# Execute GC Safety harness
make test-gc-safety
```

Under the hood, this sets two environment variables:
1. `LUNA_GC_STRESS=1`: Forces an incremental GC step at every allocation safepoint.
2. `LUNA_GC_VERIFY=1`: Runs a full heap validation sweep checking object headers, color states, and pointers for dangling references.

### Test Implementations:
- **`test/test_gc_safety.lu`**: Creates 32 closure keepers holding dynamic map metadata. Allocates 80 rounds of short-lived noise arrays while validating that captured closure state and root table matrices remain undamaged across GC passes.
- **`test/test_gc_large_safety.lu`**: Allocates multi-kilobyte string payloads (2,048-iteration repeated strings) inside keepers while churning noise arrays, ensuring large-object heap partitions survive stress collections.

---

## 7. Comparative Benchmark Infrastructure (`stress_test/` & `test_gc/`)

The repository includes an automated multi-language benchmarking suite to track performance and GC pause metrics:

```
+-----------------------------------+---------------------------------------------------------+
| Runner Script                     | Description                                             |
+-----------------------------------+---------------------------------------------------------+
| `test_gc/gc_bench.py`             | Luna vs Go benchmark; records metrics to CSV            |
| `test_gc/gc_bench_all.py`         | Unified benchmark (Luna, Go, Java G1, Python)           |
| `stress_test/stress_bench.py`     | 3-iteration stress comparison with Go                   |
| `stress_test/java_bench.py`       | Luna vs OpenJDK Java (G1 collector)                     |
| `stress_test/python_bench.py`     | Luna vs CPython (refcounting + cyclic GC)               |
+-----------------------------------+---------------------------------------------------------+
```

### Metrics Recorded:
1. **User Time (seconds)**: CPU time consumed in user mode (via `/usr/bin/time -v`).
2. **Max RSS (MB)**: Maximum resident set size / physical memory usage.
3. **GC Total Time (ms)**: Cumulative time spent executing GC cycles.
4. **GC Max Pause (ms)**: Longest individual stop-the-world or incremental pause.
5. **GC Events**: Number of collection cycles or incremental step events.

### How Metrics are Captured:
- **Luna**: Runs with `LUNA_GC_STATS=1`, emitting structured stats parsed via regex: `LUNA_GC_STATS,<total_ms>,<max_pause_ms>,<event_count>`.
- **Go**: Runs with `GODEBUG=gctrace=1`, extracting STW pause times `A+B+C ms clock`.
- **Java**: Executes with `-XX:+UseG1GC` and `-Xlog:gc*`, parsing G1 pause events.
- **Python**: Profiles cycle collections via the `gc` module.

---

## 8. Host-Level Zig & Native Rust Testing

### 1. Zig Testing Layer (`zig-test/`)
The `zig-test/` suite directly tests Luna's C API via `@cImport`:
- Lexer token stream accuracy.
- AST node generation and parser recovery.
- Bytecode chunk serialization.
- Run with:
  ```bash
  make zig-test
  ```

### 2. Rust Runtime Crates (`rust/`)
The low-level unsafe and structured data runtimes are tested via Cargo:
- `rust/unsafe_rt`: Tests unmanaged pointer allocations, boundary checks, and alignment.
- `rust/data_rt`: Tests memory layouts and field indexing for `bloc`, `box`, and `data` structures.
- Run with:
  ```bash
  make test-rust-data
  ```

---

## 9. Developer Guide: Writing & Adding Tests

### Adding a Functional Test (`test/test_<feature>.lu`)
1. Create a new `.lu` file in `test/`:
   ```luna
   print("=== Running My Feature Tests ===")
   
   let x = 40 + 2
   assert(x == 42)
   
   print("My Feature tests passed!")
   ```
2. Add a manual check in `Makefile` under the `test:` target (optional, as `test_runner.sh` discovers all `test/*.lu` files automatically).
3. Run `make test`.

### Adding a Golden Output Test
1. Create `test/test_feature.lu`.
2. Create `test/test_feature.expect` containing the exact expected stdout/stderr output.
3. Run `test_runner.sh` to verify output matching.

### Adding a Split Golden Error Test
1. Create `test/test_errors.lu` using `# CASE <id>` section headers:
   ```luna
   # CASE undefined_var
   print(unknown_variable)

   # CASE type_mismatch
   let a = 10 + "string"
   ```
2. Create `test/test_errors.expect` with corresponding `# CASE <id>` blocks:
   ```
   # CASE undefined_var
   Name Error: Variable 'unknown_variable' is not defined

   # CASE type_mismatch
   Type Error: Cannot add int and string
   ```

### Adding a GC Stress Benchmark
1. Add `<workload_name>.lu` to `test_gc/`.
2. Append the benchmark name to `BENCHMARKS` list in `test_gc/gc_bench.py` and `test_gc/gc_bench_all.py`.
3. Provide an equivalent Go implementation in `go/<workload_name>.go` (if comparing against Go).
4. Run `make test-gc`.
