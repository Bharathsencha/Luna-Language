# Rust Runtimes

This directory contains Luna's Rust-backed static runtimes.

The Luna interpreter itself is still written in C. Rust is currently used where
metadata-heavy runtime logic benefits from stronger local invariants, easier
unit testing, and a clean static-library boundary.

## Crates In This Directory

- `unsafe_rt/`
  The unsafe-memory rule runtime already used by the interpreter.
- `data_rt/`
  The new structured-data prototype for `bloc`, `box`, and `template`.

Both crates build to static `.a` archives and are copied into `lib/` by the
top-level `Makefile`.

## unsafe_rt

`unsafe_rt` is Luna's Rust-backed unsafe-memory enforcement runtime.

Actual allocation, free, value reads, value writes, and variable storage still
happen in the C runtime. Rust is used for the rule-enforcement layer because it
is a good fit for tracking pointer metadata safely and deterministically.

## Purpose

Luna's `unsafe` memory system adds manual memory tools to the language:

- `alloc(n)`
- `free(ptr)`
- `defer(ptr)`
- `load(ptr)` or legacy `deref(ptr)`
- `store(ptr, value)`
- `ptr_offset(ptr, offset)` or legacy `ptr_add(ptr, offset)`
- `address_of(name)` or legacy `addr(name)`

The Rust side does not own Luna values. It only validates whether an operation
is legal before the C interpreter performs it.

## Recommended Names

Two older names are still supported, but the clearer names below are the ones
to prefer in new Luna code:

| Preferred | Legacy | Meaning |
|----------|--------|---------|
| `address_of(name)` | `addr(name)` | Get the address of a named value |
| `ptr_offset(ptr, offset)` | `ptr_add(ptr, offset)` | Move a pointer by a slot offset |
| `load(ptr)` | `deref(ptr)` | Read the value stored at a pointer |
` 

Examples:

```javascript
unsafe {
    let value = 41
    let value_ptr = address_of(value)
    let current = load(value_ptr)

    let buf = alloc(3)
    let second = ptr_offset(buf, 1)
}
```

Important limits:

- `address_of(...)` only works on named variables, not temporary expressions
- `ptr_offset(...)` must stay inside the allocated buffer range
- `load(...)` needs a valid live pointer
- all three functions can only be used inside `unsafe { ... }`

## GC Relationship

`unsafe` does not disable Luna's normal runtime or bypass GC for everything.

- `alloc()` buffers are manual memory and are not GC-managed.
- Ordinary Luna values still behave normally outside and inside `unsafe`.
- Raw pointers are blocked from entering GC-managed values such as lists and
  maps, so the GC side never needs to trace or own unsafe pointers.

This is a smaller and safer boundary than making the whole unsafe block bypass
the rest of the runtime.

## What the Rust Runtime Tracks

The runtime keeps three pieces of state:

### 1. Allocation table

This maps base pointer addresses to metadata:

```text
base pointer -> {
    size,
    poisoned,
    base
}
```

Fields:

- `size`: allocation span in bytes
- `poisoned`: set after `free`
- `base`: original allocation base, used for cross-allocation checks

### 2. Unsafe block state

This tracks whether Luna is currently inside an `unsafe { ... }` block.
Nested unsafe blocks are rejected.

### 3. Defer stack

This stores pending pointer frees in LIFO order for `defer(ptr)`.

## Crate Layout

- `unsafe_rt/Cargo.toml`
  Defines the crate as a `staticlib`
- `unsafe_rt/src/lib.rs`
  Exposes the C ABI functions used by the interpreter
- `unsafe_rt/src/table.rs`
  Allocation metadata table and owner lookup logic
- `unsafe_rt/src/rules.rs`
  Rule-check functions for free, loads, pointer offsetting, casts, comparisons,
  and other unsafe-memory rules
- `unsafe_rt/src/block.rs`
  Unsafe block entry and exit tracking
- `unsafe_rt/src/defer.rs`
  Deferred free stack
- `unsafe_rt/src/error.rs`
  Shared numeric error codes and user-facing error messages

## Exported FFI Surface

The C side includes `include/memory.h` and calls these Rust exports:

| Function | Role |
|----------|------|
| `luna_mem_init` | Reset runtime state |
| `luna_mem_begin` | Enter an unsafe block |
| `luna_mem_end` | Exit an unsafe block and flush deferred frees |
| `luna_mem_track_alloc` | Register a successful allocation |
| `luna_mem_free_ok` | Validate and poison a pointer on free |
| `luna_mem_deref_ok` | Validate pointer reads for `load()` / `deref()` |
| `luna_mem_store_ok` | Validate writes |
| `luna_mem_ptr_add_ok` | Bounds-check pointer offsetting |
| `luna_mem_addr_ok` | Validate `address_of()` / `addr()` |
| `luna_mem_escape_ok` | Reject pointer escape |
| `luna_mem_cmp_ok` | Validate pointer comparisons |
| `luna_mem_cast_ok` | Reject non-integer pointer casts |
| `luna_mem_call_ok` | Restrict unsafe-block calls to builtin/native functions |
| `luna_mem_alloc_size_ok` | Reject invalid allocation sizes |
| `luna_mem_defer` | Push a pointer onto the defer stack |
| `luna_mem_run_defers` | Flush deferred frees |
| `luna_mem_gc_store_ok` | Reject pointer storage in GC-managed values |
| `luna_mem_error_message` | Convert runtime error codes to strings |
| `luna_mem_shutdown` | Report leaks and clear state at shutdown |

## Runtime Flow

A typical C-side memory operation sequence looks like this:

```c
luna_mem_begin();
luna_mem_alloc_size_ok(n, 1);
luna_mem_track_alloc(ptr, size_in_bytes);
luna_mem_store_ok(ptr);
luna_mem_deref_ok(ptr);
luna_mem_end();
```

The interpreter then performs the real operation after the check succeeds.

## Implemented Rules

The current runtime covers the following rule set:

- `R1` no pointer-to-pointer through `address_of()`
- `R2` no pointer escaping the current `unsafe` block
- `R3` no out-of-bounds pointer offsets
- `R4` no use-after-free and no double free
- `R5` no `address_of()` on temporaries
- `R6` no nested `unsafe` blocks
- `R7` no pointers in GC-managed values
- `R8` no invalid alloc sizes
- `R9` no ordered pointer comparisons across different allocations
- `R10` no non-integer pointer casts
- `R11` no user-defined function calls inside `unsafe`
- `R12` static rejection of conditional `free()`

## User-Facing Mental Model

The preferred Luna API is:

- `address_of(name)` gets an address
- `ptr_offset(ptr, offset)` moves a pointer
- `load(ptr)` reads from a pointer
- `store(ptr, value)` writes to a pointer
- `defer(ptr)` frees a pointer later, at unsafe-block exit

That gives you a readable flow:

```lu
unsafe {
    let value = 41
    let p = address_of(value)
    let buf = alloc(2)
    let next = ptr_offset(buf, 1)

    store(buf, 10)
    store(next, load(p))
    defer(buf)
}
```

## Design Notes

### Why Rust here?

The rule layer is metadata-heavy and logic-heavy, but not object-model-heavy.
Rust gives:

- simpler ownership for the metadata table
- safer mutation of global runtime state
- fewer edge-case bugs around poison state and deferred cleanup
- easy unit testing for the rules in isolation

### Why not move all memory work to Rust?

Because Luna values, environments, closures, and GC ownership all live in C.
Crossing FFI for every value operation would make the implementation harder,
not cleaner.

The split is intentional:

- Rust validates pointer legality
- C performs the actual memory effects

## Build Output

The crate builds to:

```text
rust/unsafe_rt/target/release/libluna_memory_rt.a
```

The top-level `Makefile` then copies that archive into:

```text
lib/libluna_memory_rt.a
```

and links Luna against it from there.

## data_rt

`data_rt` is the starting point for Luna's upcoming structured-data split:

- `bloc`: tiny immutable inline values with a hard inline-size cap
- `box`: manual non-GC heap buffers with explicit ownership
- `template`: schema-backed GC-facing structured objects

The current crate is intentionally still pure Rust and not wired into the C
interpreter yet. Phase 1 focuses on locking the runtime shape before adding the
C ABI boundary:

- first-byte runtime kind tags
- bloc field/layout validation with the 32-byte phase-1 cap
- fixed-size `LunaBox` buffers with the 4096-byte phase-1 limit
- schema-backed `TemplateSchema` descriptors
- positional construction and named field access for `TemplateObject`

Build output:

```text
rust/data_rt/target/release/libluna_data_rt.a
```

The top-level `Makefile` copies that archive into:

```text
lib/libluna_data_rt.a
```

so the eventual interpreter bridge can link it the same way as `unsafe_rt`.

## Testing

### Rust unit tests

Run:

```bash
cargo test --manifest-path rust/unsafe_rt/Cargo.toml
```

These tests cover:

- nested unsafe rejection
- allocation tracking
- poison-after-free behavior
- pointer arithmetic bounds checks
- compare and cast rules
- defer behavior
- address-of and unsafe-call rule checks
- load/read behavior through valid pointers

### Full interpreter test

From the project root:

```bash
make
./bin/luna test/memory.lu
make test
```

## Current Limits

- The runtime tracks unsafe allocation legality, not Luna GC internals
- Raw `alloc()` buffers are manual memory and are intentionally kept separate
  from GC-managed containers
- Legacy names like `addr`, `ptr_add`, and `deref` still work for backward compatibility
