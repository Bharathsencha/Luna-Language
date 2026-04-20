# Luna Memory Library

The Memory library gives Luna a low-level `unsafe` mode for manual memory work.
Most Luna programs should stay with normal values, lists, maps, and strings.
Use this only when you need raw pointer-style control.

Inside `unsafe { ... }`, Luna now supports:

- `alloc(n)`
- `free(ptr)`
- `defer(ptr)`
- `load(ptr)` or legacy `deref(ptr)`
- `store(ptr, value)`
- `ptr_offset(ptr, offset)` or legacy `ptr_add(ptr, offset)`
- `address_of(name)` or legacy `addr(name)`
- `int(ptr)`

The Rust runtime (`libluna_memory_rt.a`) enforces the memory rules, while the C
interpreter performs the actual allocation, reads, writes, and frees.

Outside `unsafe`, Luna also supports scope cleanup with deferred calls:

- `defer(close(file))`
- `defer(save_state(cache))`

These deferred calls run in LIFO order when the current scope exits.

Inside `unsafe`, Luna does not bypass the whole runtime or disable the GC.
What changes is this:

- Raw buffers created by `alloc()` are manual memory and are not GC-managed.
- Normal Luna values still behave normally and still use the existing runtime
  ownership / GC rules.
- The unsafe runtime blocks pointers from being stored inside GC-managed values
  like lists and maps, so GC-tracked objects never end up holding raw pointers.

Today, Luna runs on the new SATB tracing collector plus the existing arena.
Unsafe memory still follows the same boundary we want to keep:

- manual `alloc()` buffers stay outside GC ownership
- normal Luna values stay under the language runtime
- raw pointers are forbidden from entering GC-managed containers

## Memory Functions

Preferred names for new code:

- `address_of(name)` instead of `addr(name)`
- `ptr_offset(ptr, offset)` instead of `ptr_add(ptr, offset)`
- `load(ptr)` instead of `deref(ptr)`

Legacy names still work, so older Luna code stays valid.

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

## Implemented Safety Rules

The following rules are currently enforced by the runtime and interpreter.
Most of them are worth keeping because they remove real undefined-behavior
classes. The one that is more policy than raw memory safety is `R11`, which
forbids user-defined calls inside `unsafe`. It is still kept for now because it
keeps pointer lifetime analysis simple and predictable.

| Rule | Status | Meaning |
|------|--------|---------|
| `R1` | Implemented | No pointer-to-pointer through `address_of()` |
| `R2` | Implemented | Pointers cannot escape the current `unsafe` block |
| `R3` | Implemented | `ptr_offset()` cannot go out of bounds |
| `R4` | Implemented | No use-after-free and no double free |
| `R5` | Implemented | `address_of()` only works on named values |
| `R6` | Implemented | Nested `unsafe` blocks are rejected |
| `R7` | Implemented | Pointers cannot be stored in GC-managed values |
| `R8` | Implemented | `alloc()` size must be a positive integer |
| `R9` | Implemented | Ordered pointer comparisons need the same base allocation |
| `R10` | Implemented | Pointers may only be cast with `int(ptr)` |
| `R11` | Implemented | Only builtin/native calls are allowed inside `unsafe` |
| `R12` | Implemented | `free()` inside conditional control flow is rejected by a static lint pass |

## Error Cases

These examples intentionally fail. The messages below match the combined golden
test in `test/test_unsafe_errors.expect`.

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
    store(ptr_add(buf, 1), 20)
    store(ptr_add(buf, 2), 30)

    print(deref(buf))
    print(deref(ptr_add(buf, 1)))
    print(deref(ptr_add(buf, 2)))
}
```

## Example 2: Editing a Variable Through `addr()`

```javascript
let score = 5

unsafe {
    let p = addr(score)
    let current = deref(p)
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
        let slot = ptr_add(buf, i)
        store(slot, i * i)
    }

    for (let i = 0; i < 5; i++) {
        print("slot", i, "=", deref(ptr_add(buf, i)))
    }
}
```

## Example 4: Compare Pointers Inside One Allocation

```javascript
unsafe {
    let buf = alloc(4)
    defer(buf)

    let a = buf
    let b = ptr_add(buf, 1)
    let c = ptr_add(buf, 2)

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
            let cell = ptr_add(grid, index)
            store(cell, x + y)
        }
    }

    for (let y = 0; y < height; y++) {
        let row = []
        for (let x = 0; x < width; x++) {
            let index = y * width + x
            append(row, deref(ptr_add(grid, index)))
        }
        print(row)
    }
}
```
