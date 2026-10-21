# Syntax

Luna's syntax looks like JavaScript, without semicolons. One statement per line; blocks use
`{ }`. Comments start with `#` or `//`.

```javascript
# comment
// also a comment
let name = "Luna"
print("Hello, {name}!")
```

A program is compiled ahead of time (see [getting-started.md](../getting-started.md)), then
its top-level statements run in order. Names are looked up when the statement runs: a `func`
is defined when execution reaches it, so calling a function above its definition at the top
level is a `Name Error`. A function may call another one that is defined later in the file, as
long as the call happens after that definition has run.

## Values and types

| Literal | `type()` |
|---|---|
| `42`, `-7` | `int` |
| an integer outside the 32-bit range, such as `9223372036854775807` | `long` |
| `1.5`, `3.0` | `float` |
| `'c'` | `char` |
| `"text"` | `string` |
| `true`, `false` | `boolean` |
| `null` | `null` |
| `[1, 2, 3]` | `list` |
| `{"key": value}` | `map` |
| `func(x) { return x }` | `function` |
| builtins such as `len` | `native_function` |

Other values report `file` (from `open`), `bloc`, `box`, `template`, `pointer` (inside
`unsafe`), and `map` for a `data` value. `long` is only a label: integers are 64-bit
everywhere, and `int` and `long` values mix freely.

`dense_list(n, init)` creates a contiguous float list for vector math (see
[stdlib/math.md](../stdlib/math.md)); its `type()` is `list`. `data`, `bloc`, `box` and
`template` define structured values (see [data-types.md](data-types.md)).

Number literals are decimal digits with an optional fractional part. There are no hex or
binary literals and no exponent form (`1e3`).

`/` follows the operand types: two ints give an int that truncates toward zero (`7 / 2` is `3`,
`-7 / 2` is `-3`), and if either side is a float the result is a float (`7 / 2.0` is `3.5`).
`%` takes the sign of the left operand (`-10 % 3` is `-1`) and also works on floats
(`10.5 % 3` is `1.5`).

Floats print with up to six significant digits, as C's `%g` does: `print(1.0)` prints `1`,
`print(100.0 / 3)` prints `33.3333` and `print(1234567.0)` prints `1.23457e+06`.

## Variables

```javascript
let x = 10
const version = "0.1"
x = 20
x += 5                    # also -=, *=, /=
x++
x--
```

`const` declares a binding that is meant to stay fixed; reassigning it is currently not
rejected by the compiler (see [../known-issues.md](../known-issues.md)).

## Strings

Double-quoted strings interpolate any expression inside `{ }`:

```javascript
let hp = 125
print("next hp = {hp + 1}")           # next hp = 126
print("slot = {[3, 7, 9][1]}")        # slot = 7
```

Escapes: `\n`, `\t`, `\r`, `\"`, `\\`, `\{` and `\}`. Any other character after a backslash is
taken literally. A string may span lines. Character literals are `'a'`, `'\n'`, `'\t'`
and `'\0'`.

Indexing a string returns a `char`, and negative indices count from the end: `"Luna"[0]` is
`'L'` and `"Luna"[-1]` is `'a'`. The library is in [stdlib/string.md](../stdlib/string.md).

## Operators

| Kind | Operators |
|---|---|
| Arithmetic | `+ - * / %`, unary `-` |
| Comparison | `== != < <= > >=` |
| Logical | `and` / `&&`, `or` / `\|\|`, `not` / `!` (short-circuit) |
| Assignment | `= += -= *= /= ++ --` |

Operators are strict: there are no hidden conversions.

| Rule | Example | Result |
|---|---|---|
| Numbers mix: int with float gives a float | `1 + 1.5` | `2.5` |
| `+` joins two strings only | `"a" + 1` | error: build text with `"a{1}"` or `"a" + to_string(1)` |
| Other types don't do arithmetic | `null + 1`, `1 + true` | error |
| Integer overflow is an error, never a wrap-around | `9223372036854775807 + 1` | `integer overflow in '+'` |
| Division or `%` by zero is an error | `1 / 0`, `5 % 0` | `division by zero` |
| `==` / `!=` need the same kind of value; comparing with `null` always works | `1 == "1"` | error |
| `<`, `<=`, `>`, `>=` order two numbers, two strings or two chars | `1 < "a"` | error |
| Conditions (`if`, `while`, `for`, `and`, `or`, `not`) must be booleans | `if (5)` | `condition must be a boolean, got int`: write `if (x != 0)` or `if (len(s) > 0)` |

`1 == 1.0` is `true`. Two lists or maps are equal only when they are the same object.

Between two lists of numbers, `+ - * /` work element by element and return a dense list:
`[1, 2] + [3, 4]` gives `d[4, 6]`.

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

Map keys are strings. Reading or writing a list index outside `0 .. len - 1` (or the negative
equivalent) is an `Index Error`; a list does not grow by assignment. Use `append`.

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

`break` and `continue` work in all loops. `for-in` iterates a list, a dense list or a string
(one `char` at a time); to walk a map use `map_keys`, `map_values` or `map_items`.
`range(start, end, step)` builds a list of integers. A `switch` case does not fall through to
the next case, and `break` inside a `switch` leaves it.

A `for-in` or `while` body gets a fresh scope on each iteration, so closures created in the
body capture that iteration's variables. The variable of a C-style `for (let i = ...; ...)`
loop is a single variable shared by every iteration.

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

Functions are values: pass them, return them, and store them in lists. A function without
`return` gives `null`. Calling with the wrong number of arguments is an `Argument Error`
(`f() expects 1 argument, but got 2`). `map`, `filter` and `reduce` take functions:

```javascript
let doubled = map([1, 2, 3], func(x) { return x * 2 })
let total = reduce(doubled, func(acc, x) { return acc + x }, 0)
```

If the file defines a zero-argument `main()`, it runs automatically after the top level.

Recursion is limited to 100000 nested calls; beyond that the program stops with
`stack overflow: recursion is too deep`.

## Scope cleanup with defer

`defer(call(...))` runs the call when the current scope exits, in LIFO order:

```javascript
func save(path, text) {
    let f = open(path, "w")
    defer(close(f))
    write(f, text)
}
```

Inside `unsafe { }`, `defer(ptr)` frees a pointer instead (see
[../internals/unsafe.md](../internals/unsafe.md)).

## Output and input

```javascript
print("a")               # one line
let name = input("Name: ")
```

`print` and `input` are keywords of the grammar, not ordinary functions. When `print` receives
several arguments, it prints each one on its own line. Use interpolation for one-line output:
`print("{a} {b}")`. `input(prompt)` writes the prompt and returns one line from standard
input without the newline, or `""` at end of input.

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

`test/balls.lu` covers them.

## Names to avoid

The words `data`, `template`, `bloc`, `box`, `unsafe`, `use`, `from`, `export`, `import`,
`print`, `input`, `in`, `case` and `default` are keywords and cannot be used as variable or
function names.

## Next

- [modules.md](modules.md): `use`, `export`, `import`
- [data-types.md](data-types.md): `data`, `bloc`, `box`, `template`
- [internals/unsafe.md](../internals/unsafe.md): manual memory
