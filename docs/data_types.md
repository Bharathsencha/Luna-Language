# Luna Data Types — Complete Reference

> Luna's three native structured-data tiers: **bloc**, **box**, and **template**.
> Each tier occupies a different memory region, follows different rules, and exists for a different reason.

---

## Overview

Luna deliberately splits structured data into three tiers instead of providing one do-everything struct type. The reason is simple — one type forces one set of tradeoffs on every use case. Luna refuses that compromise.

| Type | Memory Region | GC Managed | Mutable | Max Size | Can Contain |
|------|---------------|------------|---------|----------|-------------|
| **bloc** | CPU cache line (inline) | No | No | 32 bytes | Nothing (leaf node) |
| **box** | Stack (non-GC heap backing) | No | Yes | 4096 bytes | Bloc only |
| **template** | Heap (Immix GC) | Yes | Yes | Grows in chunks | Bloc, Box |

### Containment Hierarchy

```
Template (heap)    ← can hold Box, Bloc
    ↑
Box (stack)        ← can hold Bloc only
    ↑
Bloc (cache line)  ← leaf node, holds nothing
```

**No type can contain itself. No type can appear inside a lower tier. Template cannot nest inside Template, Box, or Bloc.** These rules are enforced at runtime by the Rust backend.

---

## 1. Bloc — Cache-Aligned Inline Value

### What It Is

A `bloc` is a tiny, immutable, cache-line-aligned value type. Think of it as a struct that lives entirely inside a register-width area of memory. The GC never sees it, never touches it, never scans it. It is placed, not allocated.

### Where It Lives

- **Memory**: Inline on the **cache line** — not heap-allocated, not stack-allocated in the traditional sense. The data is packed directly into the value representation with cache-line alignment.
- **Lifetime**: Scoped to the enclosing expression or variable binding. When the scope ends, the bits are simply overwritten — no destructor, no free, no GC involvement.
- **Cache behavior**: Because blocs are ≤32 bytes and cache-line aligned, they are designed to sit in L1 cache during hot-path computation.

### Rules and Bounds

| Rule | Bound |
|------|-------|
| Maximum total size | **32 bytes** (hard cap, checked at creation) |
| Allowed field kinds | `int` (8B), `float` (8B), `bool` (1B), `char` (4B), nested `bloc` |
| Mutability | **Immutable** — always. No exceptions. |
| GC interaction | None — the GC never scans or collects blocs |
| Nesting | Can nest inside Box or Template. Cannot nest inside another Bloc unless total fits in 32B |
| Naming | Must have a non-empty name |
| Field uniqueness | All field names must be unique (enforced by Rust) |

### Examples

**Example 1 — Basic 2D vector (works)**
```luna
bloc Vec2 { x, y }
let p = Vec2{3.0, 4.0}
print(p.x)   // 3.0
```
- Total size: 2 x 8B (float) = 16 bytes. Fits in 32B cap.
- Lives inline on the cache line. No heap allocation occurs.

**Example 2 — Max-size bloc (works)**
```luna
bloc Color { r, g, b, a }
let white = Color{255, 255, 255, 255}
```
- Total size: 4 x 8B (int) = 32 bytes. Exactly at the cap.
- Four integers pack perfectly into one cache line.

**Example 3 — Oversize bloc (rejected)**
```luna
bloc TooWide { a, b, c, d, e }
// ERROR: bloc exceeds the cache-line size limit (32 bytes)
// 5 x 8B = 40 bytes, over the 32B cap
```
- The Rust backend (`BlocLayout::new`) rejects this with `BlocLayoutError::Oversize`.
- Fix: use a `template` instead, or reduce field count.

### Bloc and `unsafe {}` Blocks

**Blocs are NOT allowed inside `unsafe {}` blocks.** This is Rule 12 of Luna's unsafe rules. The reasoning: blocs have their own deterministic lifetime that doesn't need manual management, and allowing them in unsafe blocks would create confusing ownership semantics. If you need raw byte-level control, use `alloc`/`free` inside unsafe instead.

```luna
unsafe {
    bloc Vec2 { x, y }   // ERROR: bloc not allowed inside unsafe
    let p = Vec2{1, 2}   // rejected
}
```

---

## 2. Box — Stack-Scoped Manual Buffer

### What It Is

A `box` is a mutable, fixed-size byte buffer with explicit ownership. It lives on the stack with heap backing for the actual data, but the GC never manages it. You own it. You scope it. When it goes out of scope, it auto-releases (or you free it manually).

### Where It Lives

- **Memory**: The handle lives on the **stack**. The backing bytes are allocated outside GC ownership (non-GC heap region). The runtime tracks liveness via a handle registry in the Rust backend.
- **Lifetime**: Scope-tied. When the enclosing scope ends, the box is auto-released. If the box's containing Template is collected, a destructor chain fires to free it.
- **Cache behavior**: No special alignment guarantees — the data may or may not be in cache depending on access patterns and size.

### Rules and Bounds

| Rule | Bound |
|------|-------|
| Minimum size | **1 byte** |
| Maximum size | **4096 bytes** (hard cap) |
| Mutability | **Mutable** — read/write via byte offsets |
| GC interaction | None — the GC does not scan or collect boxes |
| Ownership | Explicit — tracked by Rust handle registry |
| Use-after-free | Detected and rejected at runtime |
| Double-free | Detected and rejected at runtime |
| Nesting | Can contain Bloc. Cannot contain Box or Template. |
| Bounds checking | All reads and writes are bounds-checked |

### Examples

**Example 1 — Basic buffer allocation (works)**
```luna
let buf = box[128]
print(buf.len)   // 128
print(buf.cap)   // >= 128
```
- Allocates 128 bytes of non-GC storage.
- The handle is tracked by the Rust backend via `luna_data_box_track`.

**Example 2 — Zero-size allocation (rejected)**
```luna
let buf = box[0]
// ERROR: box size must be between 1 and 4096 bytes
```
- The Rust backend (`luna_data_box_size_ok`) rejects size 0.

**Example 3 — Use-after-free protection (caught at runtime)**
```luna
let buf = box[64]
// buf goes out of scope and is freed
// later attempt to access buf:
// ERROR: box was already freed (use-after-free)
```
- The Rust handle registry marks the handle as dead when freed.
- Any subsequent access via `luna_data_box_access_ok` returns `ERR_BOX_USE_AFTER_FREE`.

### Box and `unsafe {}` Blocks

**Boxes are NOT allowed inside `unsafe {}` blocks.** This is the same Rule 12 that applies to blocs. Boxes have their own scope-based lifetime, and mixing them with `alloc`/`free` manual memory would create confusion about which system owns which memory.

```luna
unsafe {
    let buf = box[128]   // ERROR: box not allowed inside unsafe
}
```

If you need raw byte manipulation inside unsafe, use `alloc(n)` instead:
```luna
unsafe {
    let raw = alloc(16)
    store(raw, 42)
    defer(raw)
}
```

---

## 3. Template — GC-Managed Heap Object

### What It Is

A `template` is a rich, schema-backed, mutable heap object. It is fully managed by Luna's tricolor + Immix garbage collector with SATB write barriers. Templates are the go-to type for entities, game state, configs, AST nodes, and anything that benefits from named field access and automatic memory management.

### Where It Lives

- **Memory**: The **Immix heap** — managed by Luna's generational GC. Memory grows in power-of-two aligned chunks.
- **Lifetime**: Determined by GC reachability. When no live references point to a template, it becomes eligible for collection. On collection, the GC walks field metadata to free any contained Box pointers before reclaiming the template's own memory.
- **Cache behavior**: Heap-allocated objects are subject to normal heap cache behavior. The Immix collector's line-based organization provides some spatial locality.

### Rules and Bounds

| Rule | Bound |
|------|-------|
| Size | Grows in power-of-two chunks — no hard cap |
| Mutability | **Mutable by default** — fields can be reassigned |
| GC interaction | Fully managed — SATB barriers, tricolor marking |
| Schema | Every template must have a registered schema |
| Field access | By name, through schema lookup table |
| Constructor arity | Must match schema field count exactly |
| Field uniqueness | All field names must be unique |
| Naming | Must have a non-empty name |
| Duplicate schemas | Rejected (one name = one schema) |
| Nesting | Can contain Bloc, Box. Cannot contain Template directly. |

### Examples

**Example 1 — Basic entity (works)**
```luna
template Player { name, hp, pos }
let hero = Player{"Astra", 100, Vec2{0, 0}}
print(hero.name)   // "Astra"
hero.hp = 80       // mutation works
print(hero.hp)     // 80
```
- Schema registered via `luna_data_template_register`.
- Construction validated against schema arity.
- Named field access goes through the schema lookup table.

**Example 2 — Wrong constructor arity (rejected)**
```luna
template Pair { left, right }
let p = Pair{42}
// ERROR: template constructor argument count does not match field count
// Expected 2, got 1
```
- The Rust backend (`luna_data_template_arity_ok`) rejects mismatched counts.

**Example 3 — Unknown field access (rejected)**
```luna
template Point { x, y }
let p = Point{1, 2}
print(p.z)
// ERROR: template does not have a field with that name
```
- `luna_data_template_field_ok` returns `ERR_TEMPLATE_UNKNOWN_FIELD`.
- Use `shape()` to inspect what fields a template has.

---

## Cross-Type Scenarios

These examples show how the three tiers interact, including what the containment hierarchy allows and forbids.

**Scenario 1 — Bloc inside a Template (works)**
```luna
bloc Vec2 { x, y }
template Entity { name, pos, buffer }
let e = Entity{"goblin", Vec2{5, 10}, box[64]}
print(e.name)   // "goblin"
```
- Template can hold both Bloc (Vec2) and Box (64-byte buffer).
- Containment check: `luna_data_containment_ok(KIND_TEMPLATE, KIND_BLOC)` returns OK.
- Containment check: `luna_data_containment_ok(KIND_TEMPLATE, KIND_BOX)` returns OK.

**Scenario 2 — Template inside Template (rejected)**
```luna
template Inner { value }
template Outer { child }
let o = Outer{Inner{42}}
// ERROR: containment hierarchy violation
// Template cannot be stored inside another Template
```
- `luna_data_containment_ok(KIND_TEMPLATE, KIND_TEMPLATE)` returns `ERR_CONTAINMENT`.
- Design rationale: nested GC objects with arbitrary depth would complicate the write barrier.

**Scenario 3 — Nested Bloc (works if total fits)**
```luna
bloc Vec2 { x, y }
bloc Rect { min, max }
let r = Rect{Vec2{0, 0}, Vec2{10, 10}}
```
- `Vec2` = 16 bytes. `Rect` = 2 x 16 = 32 bytes. Fits exactly at the cap.
- Both blocs are inline. No heap allocation for either.

**Scenario 4 — Mutation: bloc vs template**
```luna
bloc Point { x, y }
let p = Point{1, 2}
p.x = 5   // ERROR: bloc values are immutable

template MutPoint { x, y }
let mp = MutPoint{1, 2}
mp.x = 5  // works — templates are mutable by default
```
- If you need to change field values after creation, use a `template` instead of a `bloc`.
- Blocs are designed for read-only hot-path data. Templates are designed for state that evolves.

### Template and `unsafe {}` Blocks

Templates exist in the GC tier. The `unsafe {}` block exists to bypass the GC. These two worlds do not mix.

- **Rule 7 of the Rust unsafe runtime**: No pointers in GC-managed values.
- **Rule 12**: Bloc and Box are not allowed in unsafe blocks.
- Templates are GC objects — you cannot `alloc`/`free` them or take an `address_of` a template.

```luna
unsafe {
    template Foo { x }     // ERROR: templates are GC-managed, not manual
    let f = Foo{42}        // never reaches here
}
```

Templates are the safe path — if you're using a template, you shouldn't need `unsafe` at all.

---

## Memory Layout Summary

```
┌─────────────────────────────────────────────────────────────────┐
│                         MEMORY MAP                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  L1/L2 Cache Line (≤ 32B, aligned)                             │
│  ┌──────────────────────────┐                                   │
│  │  bloc: Vec2{x, y}       │  ← inline, no allocation          │
│  │  bloc: Color{r,g,b,a}   │  ← immutable, GC-invisible        │
│  └──────────────────────────┘                                   │
│                                                                 │
│  Stack Frame (scope-tied, ≤ 4096B)                             │
│  ┌──────────────────────────┐                                   │
│  │  box[128]: handle + data │  ← mutable, explicit ownership   │
│  │  box[64]:  handle + data │  ← auto-release on scope exit    │
│  └──────────────────────────┘                                   │
│                                                                 │
│  Immix Heap (GC-managed, grows in chunks)                      │
│  ┌──────────────────────────┐                                   │
│  │  template Player { ... } │  ← mutable, schema-backed        │
│  │  template Enemy  { ... } │  ← tricolor marked, SATB barriered│
│  └──────────────────────────┘                                   │
│                                                                 │
│  Unsafe Region (manual alloc/free)                             │
│  ┌──────────────────────────┐                                   │
│  │  alloc(n) / free(ptr)    │  ← raw slots, no GC, no types    │
│  │  NOT for bloc/box/templ. │  ← enforced by Rule 12           │
│  └──────────────────────────┘                                   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Rust Runtime Kind Tag

Every value starts with a one-byte `RuntimeKind` tag (defined in `rust/data_rt/src/kinds.rs`):

| Tag | Value | Type |
|-----|-------|------|
| `1` | `RuntimeKind::Bloc` | Cache-aligned inline bloc |
| `2` | `RuntimeKind::Box` | Stack-scoped manual buffer |
| `3` | `RuntimeKind::Template` | GC-managed heap object |

The C side mirrors these as `DATA_KIND_BLOC`, `DATA_KIND_BOX`, `DATA_KIND_TEMPLATE` in `include/data_runtime.h`. This one-byte prefix lets the runtime dispatch type checks without pointer chasing.

---

## Error Codes

The Rust backend returns integer error codes that the C bridge translates into human-readable messages with fix hints:

| Code | Meaning | Fix |
|------|---------|-----|
| `0` | OK | — |
| `1` | bloc name empty | Give it a name: `bloc Vec2 { x, y }` |
| `2` | bloc duplicate field | Each field must be unique |
| `3` | bloc oversize | Reduce fields or use template |
| `4` | box invalid size | Size must be 1–4096 |
| `5` | box use-after-free | Don't access a freed box |
| `6` | box not live | Use a valid box handle |
| `7` | template name empty | Give it a name |
| `8` | template duplicate field | Each field must be unique |
| `9` | template duplicate schema | Each name can only be registered once |
| `10` | template arity mismatch | Pass the right number of values |
| `11` | template unknown field | Check field name with `shape()` |
| `12` | containment violation | Bloc is leaf. Box holds Bloc. Template holds Bloc + Box. |

---

## Rust ↔ C Bridge Architecture

```
Luna Script (.lu)
       │
       ▼
  C Interpreter (value.c)
       │
       ▼
  C Bridge (data_runtime.c)           ← wraps + error reporting
       │
       ▼
  Rust FFI (libluna_data_rt.a)        ← validates + enforces rules
       │
       ├── luna_data_bloc_validate()
       ├── luna_data_box_size_ok()
       ├── luna_data_box_track()
       ├── luna_data_box_access_ok()
       ├── luna_data_box_free_ok()
       ├── luna_data_template_register()
       ├── luna_data_template_arity_ok()
       ├── luna_data_template_field_ok()
       ├── luna_data_containment_ok()
       └── luna_data_error_message()
```

The C side performs the memory effects. The Rust side validates every operation before it happens.

---

## Build and Linking

```
rust/data_rt/                         ← Rust crate source
rust/data_rt/target/release/
    libluna_data_rt.a                 ← static archive (cargo build --release)
lib/
    libluna_data_rt.a                 ← copied by Makefile for gcc linking
```

Build: `make` (top-level) handles everything.
Test: `cargo test --manifest-path rust/data_rt/Cargo.toml` — runs all 11 tests.

---

## Testing

| Layer | Command | Tests |
|-------|---------|-------|
| Rust unit | `cargo test --manifest-path rust/data_rt/Cargo.toml` | 8 unit + 3 integration |
| Luna scripts | `./bin/luna test/test_bloc.lu` | Bloc creation, field access |
| Luna scripts | `./bin/luna test/test_box.lu` | Box lifecycle, bounds |
| Luna scripts | `./bin/luna test/test_template.lu` | Template CRUD, schema |
