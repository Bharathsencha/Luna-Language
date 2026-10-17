# Unsafe Memory

`unsafe { ... }` gives Luna manual memory: raw buffers you allocate, read, write and free
yourself. Most programs never need it. Lists, maps, strings and `dense_list` cover normal work.

The C side (`src/unsafe_runtime.c`) performs allocation, loads, stores and frees. Every
operation is first checked by the Rust rule engine in `rust/unsafe_rt`
(`lib/libluna_memory_rt.a`).

## Functions

Available only inside `unsafe { }`:

| Function | Meaning |
|---|---|
| `alloc(n)` | Allocate `n` value slots; returns a pointer |
| `free(ptr)` | Free an allocation |
| `defer(ptr)` | Free `ptr` when the block exits |
| `load(ptr)` | Read the value at `ptr` |
| `store(ptr, value)` | Write `value` at `ptr` |
| `ptr_offset(ptr, n)` | Pointer `n` slots past `ptr`, bounds-checked |
| `address_of(name)` | Pointer to a named variable |
| `int(ptr)` | The only allowed pointer cast |

The older names `deref`, `ptr_add` and `addr` still work as aliases.

Outside `unsafe`, `defer(call(...))` schedules a call for scope exit, for example
`defer(close(file))`. Deferred calls run in LIFO order.

## Boundary with the GC

- Buffers from `alloc()` are not GC-managed and are never traced.
- Normal Luna values inside an `unsafe` block are still GC-managed as usual.
- A raw pointer may not be stored in a list, map or other GC value (R7), so the collector
  never sees one.

## Lifecycle Example

```javascript
unsafe {
    let buf = alloc(2)
    defer(buf)

    store(buf, 12)
    let next = ptr_offset(buf, 1)
    store(next, 34)

    print(load(buf))
    print(load(next))
}
```

This is the usual flow:

- allocate with `alloc(...)`
- register cleanup with `defer(...)`
- write with `store(...)`
- read with `load(...)`

| Function | Description | Example |
|----------|-------------|---------|
| `alloc(n)` | Allocates `n` Luna value slots and returns a pointer | `let buf = alloc(4)` |
| `free(ptr)` | Frees an allocation pointer manually | `free(buf)` |
| `defer(ptr)` | Schedules a pointer to be freed when the current `unsafe` block ends | `defer(buf)` |
| `load(ptr)` | Reads the value stored at a pointer | `let x = load(buf)` |
| `store(ptr, value)` | Writes a value to a pointer slot | `store(buf, 10)` |
| `ptr_offset(ptr, offset)` | Moves `offset` slots forward inside the same allocation | `let p2 = ptr_offset(buf, 2)` |
| `address_of(name)` | Takes the address of a named variable | `let p = address_of(score)` |
| `int(ptr)` | Returns the raw integer address of a pointer | `print(int(buf))` |

## Scope Defer

Deferred calls are available in normal Luna code and are separate from pointer
cleanup in `unsafe`.

```javascript
func write_log(path, text) {
    let file = open(path, "w")
    defer(close(file))
    write(file, text)
}
```

The call expression inside `defer(...)` is captured immediately, and the
cleanup runs when the current scope exits.

## Valid and Invalid Use

### Valid

```javascript
unsafe {
    let buf = alloc(2)
    defer(buf)

    store(buf, 12)
    store(ptr_offset(buf, 1), 34)

    print(load(buf))
    print(load(ptr_offset(buf, 1)))
}
```

### Invalid: `alloc()` outside `unsafe`

```javascript
let buf = alloc(2)   # invalid
```

### Invalid: `address_of()` on a temporary

```javascript
unsafe {
    let p = address_of(1 + 2)   # invalid
}
```

### Invalid: pointer escaping the block

```javascript
let escaped = null

unsafe {
    let buf = alloc(1)
    escaped = buf   # invalid
}
```

### Invalid: storing pointers inside GC values

```javascript
unsafe {
    let buf = alloc(1)
    let items = [buf]   # invalid
}
```

## Safety Rules

| Rule | Meaning | Interpreter | VM |
|---|---|---|---|
| `R1` | No pointer-to-pointer through `address_of()` | enforced | enforced |
| `R2` | Pointers cannot escape the current `unsafe` block | enforced | enforced |
| `R3` | `ptr_offset()` cannot go out of bounds | enforced | enforced |
| `R4` | No use-after-free and no double free | enforced | enforced |
| `R5` | `address_of()` only works on named values | enforced | enforced |
| `R6` | Nested `unsafe` blocks are rejected | enforced | enforced |
| `R7` | Pointers cannot be stored in GC-managed values | enforced | enforced |
| `R8` | `alloc()` size must be a positive integer | enforced | enforced |
| `R9` | Ordered pointer comparisons need the same base allocation | enforced | enforced |
| `R10` | Pointers may only be cast with `int(ptr)` | enforced | enforced |
| `R11` | Only builtin calls are allowed inside `unsafe` | enforced | enforced |
| `R12` | `free()` inside conditional control flow is rejected (static lint) | enforced | enforced |

R11 is more policy than memory safety. It keeps pointer lifetime analysis simple by forbidding
user-defined calls inside `unsafe`.

## Engine status

All 12 rules are enforced by both the bytecode VM and the legacy interpreter
(`LUNA_USE_INTERPRETER=1`), with the same messages. `box` and `bloc` values are also rejected
inside `unsafe` blocks. A rule violation stops the program.

`make test` checks this: `test/test_unsafe_errors.lu` has one case per rule, compared against
the VM's exact output, and `test/check_engines.sh` runs every case on the interpreter too.

## Error Cases

These examples intentionally fail, with the same message on both engines.

### R1: `address_of()` of a pointer

```javascript
unsafe {
    let buf = alloc(1)
    let bad = address_of(buf)
}
```

Expected message: `cannot take address_of() of a pointer value`

### R2: pointer escape

```javascript
let escaped = null

unsafe {
    let buf = alloc(1)
    escaped = buf
}
```

Expected message: `pointer cannot escape the current unsafe block`

### R3: out-of-bounds `ptr_offset()`

```javascript
unsafe {
    let buf = alloc(1)
    let bad = ptr_offset(buf, 1)
}
```

Expected message: `ptr_add() moved outside the allocation bounds`

### R4: use after free

```javascript
unsafe {
    let buf = alloc(1)
    free(buf)
    load(buf)
}
```

Expected message: `pointer was freed or freed twice`

### R5: `address_of()` on a temporary

```javascript
unsafe {
    let bad = address_of(1 + 2)
}
```

Expected message: `address_of() only works on named values`

### R6: nested `unsafe`

```javascript
unsafe {
    unsafe {
    }
}
```

Expected message: `nested unsafe blocks are not allowed`

### R7: pointer inside a GC value

```javascript
unsafe {
    let buf = alloc(1)
    let items = [buf]
}
```

Expected message: `pointers cannot be stored inside GC-managed values`

### R8: invalid allocation size

```javascript
unsafe {
    let buf = alloc(0)
}
```

Expected message: `alloc() size must be a positive integer`

### R9: ordered compare across different allocations

```javascript
unsafe {
    let left = alloc(1)
    let right = alloc(1)
    let bad = left < right
}
```

Expected message: `ordered pointer comparisons need the same base allocation`

### R10: invalid pointer cast

```javascript
unsafe {
    let buf = alloc(1)
    let bad = float(buf)
}
```

Expected message: `pointers may only be cast to int`

### R11: non-builtin call inside `unsafe`

```javascript
func touch(ptr) {
    free(ptr)
}

unsafe {
    let buf = alloc(1)
    touch(buf)
}
```

Expected message: `only builtin calls are allowed inside unsafe blocks`

### R12: conditional `free()`

```javascript
unsafe {
    let buf = alloc(1)
    if (true) {
        free(buf)
    }
}
```

Expected message: `free() may not appear inside conditional control flow`

## Example 1: Basic Allocate, Store, Read

```javascript
unsafe {
    let buf = alloc(3)
    defer(buf)

    store(buf, 10)
    store(ptr_offset(buf, 1), 20)
    store(ptr_offset(buf, 2), 30)

    print(load(buf))
    print(load(ptr_offset(buf, 1)))
    print(load(ptr_offset(buf, 2)))
}
```

## Example 2: Editing a Variable Through `address_of()`

```javascript
let score = 5

unsafe {
    let p = address_of(score)
    let current = load(p)
    store(p, current + 10)
}

print(score)   # 15
```

## Example 3: Fill a Buffer with a Loop

```javascript
unsafe {
    let buf = alloc(5)
    defer(buf)

    for (let i = 0; i < 5; i++) {
        let slot = ptr_offset(buf, i)
        store(slot, i * i)
    }

    for (let i = 0; i < 5; i++) {
        print("slot", i, "=", load(ptr_offset(buf, i)))
    }
}
```

## Example 4: Compare Pointers Inside One Allocation

```javascript
unsafe {
    let buf = alloc(4)
    defer(buf)

    let a = buf
    let b = ptr_offset(buf, 1)
    let c = ptr_offset(buf, 2)

    print(a < b)
    print(b < c)
    print(c >= b)
}
```

## Example 5: Build and Scan a Small Table

```javascript
unsafe {
    let width = 3
    let height = 3
    let total = width * height
    let grid = alloc(total)
    defer(grid)

    for (let y = 0; y < height; y++) {
        for (let x = 0; x < width; x++) {
            let index = y * width + x
            let cell = ptr_offset(grid, index)
            store(cell, x + y)
        }
    }

    for (let y = 0; y < height; y++) {
        let row = []
        for (let x = 0; x < width; x++) {
            let index = y * width + x
            append(row, load(ptr_offset(grid, index)))
        }
        print(row)
    }
}
```
