# Core Builtins

Builtins that are always available, registered in `env_register_stdlib` (`src/library.c`).
Strings, math, files and graphics have their own pages:
[string.md](string.md), [math.md](math.md), [file.md](file.md), [gui.md](gui.md).

## Values and types

| Function | Returns |
|---|---|
| `print(a, ...)` | Prints its arguments (a statement keyword, not a value) |
| `input(prompt)` | One line read from stdin, as a string |
| `type(v)` | Type name: `"int"`, `"long"`, `"float"`, `"char"`, `"string"`, `"bool"`, `"null"`, `"list"`, `"map"`, `"function"`, `"native_function"`, ... |
| `shape(v)` | Constructor name of a `data` / `bloc` / `template` value, for example `"Vec2"`; `null` for other values |
| `int(v)` | Integer from a float, char, bool or string; also the only allowed pointer cast |
| `float(v)` | Float conversion |
| `to_string(v)`, `to_int(s)`, `to_float(s)` | String conversions (see [string.md](string.md)) |
| `debug(v)` | Prints `value = <v>` and returns `v` unchanged, so it can wrap an expression |
| `assert(cond)` | Raises an assertion error when `cond` is false |
| `len(v)` | Length of a string, list, dense list or map |
| `clock()` | Monotonic time in seconds, as a float |

## Lists

| Function | Effect |
|---|---|
| `append(list, v)` / `list_append(list, v)` | Add to the end |
| `remove(list, i)` | Remove and return the element at `i` (negative counts from the end) |
| `find(list, v)` | Index of the first equal element, or `-1` |
| `slice(list, a, b)` | New list of elements `a` to `b - 1` (also works on strings) |
| `range(end)`, `range(start, end)`, `range(start, end, step)` | List of integers; `step` may be negative and must not be 0 |
| `map(list, fn)`, `filter(list, fn)`, `reduce(list, fn, init)` | Higher-order helpers; return new values |
| `sort(list)` | In-place ascending sort: merge sort, with insertion sort for small runs |
| `shuffle(list)` | In-place Fisher-Yates shuffle |
| `ssort(list)` | Stalin sort, in place: deletes every element smaller than the last one kept, leaving a non-decreasing list. Works on dense lists too |
| `dense_list(n, fill)` | Contiguous float array of `n` copies of the float `fill`, for SIMD math |

## Maps

Maps use string keys. `m["k"]` and `m.k` read a key; `m["k"] = v` writes one.

| Function | Effect |
|---|---|
| `map_set(m, key, v)` | Insert or replace |
| `map_get(m, key)` | Value for `key`, or `null` |
| `map_has(m, key)` | `true` if present |
| `map_delete(m, key)` | Remove; returns `true` if the key existed |
| `map_keys(m)`, `map_values(m)` | Lists of keys or values |
| `map_items(m)` | List of `[key, value]` pairs |

## Memory

`alloc`, `free`, `load`, `store`, `ptr_offset`, `address_of` and `defer(ptr)` work only
inside `unsafe { }`; see [internals/unsafe.md](../internals/unsafe.md). `defer(call(...))` for
scope cleanup works everywhere ([language/syntax.md](../language/syntax.md#scope-cleanup-with-defer)).
