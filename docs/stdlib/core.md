# Core Builtins

Builtins are always available and need no import. The compiler knows their names (the
`BUILTINS` list in `compiler/shared.lu`), and each one is implemented as a function named
`lb_<name>` in `compiler/runtime/`. Strings, math and files have their own pages:
[string.md](string.md), [math.md](math.md), [file.md](file.md).

A call with the wrong number of arguments is an `Argument Error` such as
`min() expects 2 arguments, but got 1`. An argument of the wrong type is a `Type Error` such
as `sqrt() argument 1 must be a number, got string`.

## Values and types

| Function | Returns |
|---|---|
| `print(a, ...)` | Prints each argument on its own line (a statement keyword, not a value) |
| `input(prompt)` | One line read from standard input, without the newline, as a string; `""` at end of input. The prompt is optional (`input()`), and `input` is a keyword like `print` |
| `type(v)` | Type name: `"int"`, `"long"` (an integer outside the 32-bit range), `"float"`, `"char"`, `"string"`, `"boolean"`, `"null"`, `"list"`, `"map"`, `"function"`, `"native_function"`, `"file"`, `"bloc"`, `"box"`, `"template"`, `"pointer"`. A dense list reports `"list"` and a `data` value reports `"map"` |
| `shape(v)` | Constructor name of a `data` / `bloc` / `template` value, for example `"Vec2"`; `null` for other values |
| `int(v)` | Integer from a float (truncated toward zero), char (its code), boolean (0 or 1) or string (leading digits; `0` if there are none). Also the only allowed pointer cast |
| `float(v)` | Float from an int, char, boolean or string |
| `to_string(v)`, `to_int(s)`, `to_float(s)` | Conversions (see [string.md](string.md)) |
| `debug(v)` | Prints `value = <v>` and returns `v` unchanged, so it can wrap an expression |
| `assert(cond)` | Stops the program with `Assertion failed` when `cond` is false |
| `len(v)` | Length of a string, list, dense list, map or box; anything else is a `Type Error` |
| `clock()` | Monotonic time in seconds, as a float. The zero point is arbitrary: subtract two readings |
| `args()` | The command-line arguments after the program name, as a list of strings |
| `exit(code)` | Flushes standard output and ends the program with that status (default 0) |
| `system(cmd)` | Runs `cmd` with the shell and returns its exit status as an int |

Floats are printed with up to six significant digits (`print(1.0)` prints `1`).

## Lists

| Function | Effect |
|---|---|
| `append(list, v)` / `list_append(list, v)` | Add to the end |
| `remove(list, i)` | Remove and return the element at `i` (negative counts from the end); an index out of range is an `Index Error` |
| `find(list, v)` | Index of the first equal element, or `-1`. Lists only |
| `slice(list, a, b)` | New list of elements `a` to `b - 1`; the bounds are clamped, and `a >= b` gives `[]`. Also works on strings |
| `range(end)`, `range(start, end)`, `range(start, end, step)` | List of integers; `step` may be negative and must not be 0 |
| `map(list, fn)`, `filter(list, fn)`, `reduce(list, fn, init)` | Higher-order helpers; return new values. `reduce` needs all three arguments |
| `sort(list)` | In-place ascending sort of numbers or of strings; returns `null`. Mixing numbers and strings is an error |
| `shuffle(list)` | In-place Fisher-Yates shuffle |
| `ssort(list)` | Stalin sort, in place: deletes every element smaller than the last one kept, leaving a non-decreasing list. Returns `null`. Works on dense lists too |
| `dense_list(n, fill)` | Contiguous float list of `n` copies of the float `fill`, for vector math (see [math.md](math.md)) |

`contains(list, v)` and `join(list, sep)` also work on lists; see [string.md](string.md).

## Maps

Maps use string keys. `m["k"]` and `m.k` read a key; `m["k"] = v` and `m.k = v` write one.

| Function | Effect |
|---|---|
| `map_set(m, key, v)` | Insert or replace |
| `map_get(m, key)` | Value for `key`, or `null` |
| `map_has(m, key)` | `true` if present |
| `map_delete(m, key)` | Remove; returns `true` if the key existed |
| `map_keys(m)`, `map_values(m)` | Lists of keys or values |
| `map_items(m)` | List of `[key, value]` pairs |

A `for-in` loop does not iterate a map directly: loop over `map_keys(m)`.

## Memory

`alloc`, `free`, `load`, `deref`, `store`, `ptr_offset`, `ptr_add`, `address_of` and
`defer(ptr)` work only inside `unsafe { }`; see [internals/unsafe.md](../internals/unsafe.md).
`defer(call(...))` for scope cleanup works everywhere
([language/syntax.md](../language/syntax.md#scope-cleanup-with-defer)). `free` also frees a
`box` (see [language/data-types.md](../language/data-types.md)).
