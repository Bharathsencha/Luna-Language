# Luna Language: AST Memory Arenas (The "New GC")

## Overview

In recent updates, Luna introduced a  optimization to its memory management model by implementing **AST Memory Arenas** (`arena.c` and `arena.h`). This replaces the traditional use of `malloc`, `free`, and `strdup` for Abstract Syntax Tree (AST) node allocations.

While not a traditional tracing Garbage Collector (like Java's or Go's), the Memory Arena acts as an ultra-fast, bulk-reclamation memory manager specifically tailored for the lifespan of a script's execution.

---

## The Problem: The `malloc` Tax

### 1. Allocation Overhead
In the original architecture, parsing a Luna script required calling the OS-level `malloc()` function for every single AST node (numbers, strings, binary operators, functions, etc.). For a complex script, this meant thousands or millions of tiny memory allocations. `malloc` is slow because it has to search for free memory blocks, manage fragmentation, and acquire thread locks.

### 2. Poor Cache Locality
Because each node was allocated individually, the AST was scattered randomly across the computer's RAM. When the interpreter walked the tree during execution, it suffered constant **CPU cache misses**. The CPU had to wait for deeply scattered memory addresses to be fetched from RAM, which is hundreds of times slower than reading from the L1/L2 cache.

### 3. Teardown Cost
When a script finished running, the original `ast_free()` had to recursively traverse the entire tree and call `free()` on every single node and string copy.

---

## The Solution: Memory Arenas

The Memory Arena is a bulk memory allocator designed specifically for short-lived, interconnected structures like an AST.

### How it Works (`arena.c`)
1. **Bulk Allocation**: Instead of asking the OS for 32 bytes thousands of times, the Arena asks the OS for **1 Megabyte** upfront (`arena_create(1024 * 1024)`).
2. **Pointer Bumping**: When the parser needs a new AST node, the `arena_alloc` function simply advances a pointer within that 1MB block (called "pointer bumping"). This operation takes just a few CPU cycles—nearly instantaneous compared to `malloc`.
3. **Automatic Scaling**: If the 1MB block runs out, the Arena automatically allocates a new block, doubling the size to prevent fragmentation.
4. **Zero-Overhead Teardown**: `ast_cleanup()` no longer traverses the tree. It simply frees the 1MB blocks back to the OS in a fraction of a millisecond. Individual `ast_free()` calls are now no-ops.

---

## Performance Benefits

### 1. Instantaneous Node Creation
Parsing speed has dramatically increased because generating the AST nodes no longer incurs OS-level context switching or heap management overhead.

### 2. CPU Cache Locality 
Because nodes are allocated sequentially via pointer bumping, parent modules and children nodes end up right next to each other in physical RAM. 
When the interpreter executes a `for` loop or evaluates an expression, the CPU pre-fetches the entire cache line, allowing execution to flow without RAM-latency bottlenecks.

### 3. Complete Elimination of Memory Leaks in AST
Strings copied during parsing (`NODE_STRING`, identifiers, variable names) use `arena_strdup`. Everything is tied to the arena's lifecycle, meaning if the script crashes or completes, a single `arena_free(ast_arena)` guarantees 100% of the parsed memory is released cleanly safely.

## TLDR:
By replacing universal `malloc` with a domain-specific **Memory Arena**, Luna completely eliminates the traditional "Interpreter memory bottleneck," paving the way for lightning-fast parsing, better CPU utilization, and zero-traverse garbage collection.
