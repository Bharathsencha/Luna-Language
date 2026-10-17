# Syntax

Luna's syntax looks like JavaScript, without semicolons. One statement per line; blocks use
`{ }`. Comments start with `#` or `//`.

```javascript
# comment
// also a comment
let name = "Luna"
print("Hello, {name}!")
```

## Values and types

| Literal | `type()` |
|---|---|
| `42`, `-7`, `9223372036854775807` | `int` (`long` when it needs 64 bits) |
| `1.5`, `3.0` | `float` |
| `'c'` | `char` |
| `"text"` | `string` |
| `true`, `false` | `bool` |
| `null` | `null` |
| `[1, 2, 3]` | `list` |
| `{"key": value}` | `map` |
| `func(x) { return x }` | `function` |
| builtins such as `len` | `native_function` |

`dense_list(n, init)` creates a contiguous float array for SIMD math (see
[stdlib/math.md](../stdlib/math.md)). `data`, `bloc`, `box` and `template` define structured
values (see [data-types.md](data-types.md)).

Integer division is exact when it can be: `8 / 2` is `4`, and `7 / 2` is `3.5`.

## Variables

```javascript
let x = 10
const version = "0.1"     # cannot be reassigned
x = 20
x += 5                    # also -=, *=, /=
x++
x--
```

## Strings

Double-quoted strings interpolate any expression inside `{ }`:

```javascript
let hp = 125
print("next hp = {hp + 1}")           # next hp = 126
print("slot = {[3, 7, 9][1]}")        # slot = 7
```

Indexing a string returns a `char`, and negative indices count from the end: `"Luna"[0]` is
`'L'` and `"Luna"[-1]` is `'a'`. The library is in [stdlib/string.md](../stdlib/string.md).

## Operators

| Kind | Operators |
|---|---|
| Arithmetic | `+ - * / %`, unary `-` |
| Comparison | `== != < <= > >=` |
| Logical | `and` / `&&`, `or` / `\|\|`, `not` / `!` (short-circuit) |
| Assignment | `= += -= *= /= ++ --` |

`+` concatenates strings. Between two lists of numbers, `+ - * /` work element by element and
return a dense list: `[1, 2] + [3, 4]` gives `d[4, 6]`.

## Lists and maps

```javascript
let l = [1, 2, 3]
l[-1] = 99                 # negative index from the end
append(l, 4)
print(len(l))

let player = {"name": "Astra", "hp": 100}
print(player["name"], player.hp)   # key or field syntax
map_set(player, "hp", 125)
```

Map and list functions are listed in [stdlib/core.md](../stdlib/core.md).

## Control flow

```javascript
if (hp <= 0) {
    print("down")
} else if (hp < 50) {
    print("low")
} else {
    print("ok")
}

while (i < 10) {
    i += 1
}

for (let k = 0; k < 3; k++) {
    print(k)
}

for (let item in [1, 2, 3]) {
    print(item)
}

switch (choice) {
    case 1:
        print("one")
        break
    case 2:
        print("two")
        break
    default:
        print("other")
}
```

`break` and `continue` work in all loops. `range(start, end, step)` builds a list for `for-in`.

## Functions

```javascript
func greet(name, greeting = "Hello") {    # default parameter
    return "{greeting}, {name}"
}

let inc = func(x) { return x + 1 }        # anonymous function

func make_counter() {                     # closure
    let count = 0
    return func() {
        count += 1
        return count
    }
}
```

Functions are values: pass them, return them, and store them in lists.
`map`, `filter` and `reduce` take functions:

```javascript
let doubled = map([1, 2, 3], func(x) { return x * 2 })
let total = reduce(doubled, func(acc, x) { return acc + x }, 0)
```

If the file defines a zero-argument `main()`, it runs automatically after the top level.

On the bytecode VM, recursion is limited to 64 nested calls (see
[internals/vm.md](../internals/vm.md#limits)).

## Scope cleanup with defer

`defer(call(...))` runs the call when the current scope exits, in LIFO order:

```javascript
func save(path, text) {
    let f = open(path, "w")
    defer(close(f))
    write(f, text)
}
```

## Output and input

```javascript
print("a")               # one line
let name = input("Name: ")
```

When `print` receives several arguments, the VM prints each one on its own line. The legacy
interpreter prints them on one line, separated by spaces. Use interpolation for one-line
output: `print("{a} {b}")`.

## Keyword aliases

Every keyword below is an exact synonym, mapped in the lexer:

| Alias | Keyword |
|---|---|
| `balls`, `big_balls`, `shared_balls` | `let` |
| `grab_balls` | `func` |
| `if_balls` / `else_balls` | `if` / `else` |
| `spin_balls` | `while` |
| `loop_your_balls` | `for` |
| `switch_balls` | `switch` |
| `drop_balls` | `break` |
| `jiggle_balls` | `continue` |

```javascript
balls x = 10
spin_balls (x < 20) {
    x = x + 1
}
grab_balls area(w, h) {
    return w * h
}
```

`test/balls.lu` covers all of them, and `docs/examples/fun/boobs.lu` is written with them.

## Next

- [modules.md](modules.md): `use`, `export`, `import`
- [data-types.md](data-types.md): `data`, `bloc`, `box`, `template`
- [internals/unsafe.md](../internals/unsafe.md): manual memory
