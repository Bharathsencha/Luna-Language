<p align="center">
  <img src="assets/luna.png" alt="Luna Logo" width="140">
</p>

<h1 align="center">Luna</h1>

<p align="center">
  A small scripting language with JavaScript-like syntax, compiled ahead of time to native
  binaries through LLVM IR.
</p>

---

## About Luna

Luna is a compiler. `luna build prog.lu -o prog` reads a Luna program, writes LLVM IR, and
has clang turn it into a native executable. There is no interpreter, no virtual machine and
no REPL.

- The compiler is written in Luna and compiles itself. The runtime that every compiled program
  links (values, strings, lists, maps, operators, builtins, garbage collector) is written in
  Luna too.
- The language is dynamically typed and strict: no implicit conversions, integer overflow is an
  error, conditions must be booleans.
- It has closures, modules, `data` / `template` / `bloc` / `box` types, and `unsafe` blocks
  for manual memory with rule checks.
- Graphics and audio are not available at the moment. They are planned to return as Luna
  programs that call GLFW and OpenGL through foreign calls.

## Getting Started

### Requirements

Linux (developed and tested on x86-64), `clang` 18 and `make`. On Ubuntu or Debian:

```bash
sudo apt install build-essential clang       # clang 18 on Ubuntu 24.04
```

The `clang` command must be LLVM 18 (the generated IR uses opaque pointers).

### Build

```bash
git clone https://github.com/Bharathsencha/Luna-Language
cd Luna-Language
make
make test        # compiles and runs every test; the runner ends with "passed N, failed 0, skipped 0"
```

`make` builds `bin/luna` from the seed files in `bootstrap/` (LLVM IR of the compiler and of
the runtime), then checks that the compiler reproduces itself. More details:
[docs/getting-started.md](docs/getting-started.md).

### Hello World

Save this as `hello.lu`:

```javascript
print("Hello, World!")
```

Build and run it:

```bash
./bin/luna build hello.lu -o hello
./hello

./bin/luna run hello.lu        # build into C/run/ and run in one step
```

`luna` looks for the runtime sources in `compiler/runtime` under `$LUNA_HOME`, or under the
current directory when `LUNA_HOME` is not set. From any other directory, run
`export LUNA_HOME=/path/to/Luna-Language` first.

### Commands

| Command | Effect |
|---|---|
| `luna build prog.lu -o prog` | Compile to a native binary (the default output name is `prog`) |
| `luna build prog.lu -S` | Write `C/prog.ll` and stop |
| `luna run prog.lu [args...]` | Build into `C/run/prog` and run it; the exit status is the program's |
| `luna emit prog.lu [-o out.ll]` | Write the LLVM IR only (default `C/prog.ll`) |

## Language Tour

Luna has no semicolons and no `main` requirement: statements at the top level run in order.
If the file defines a zero-argument `main()`, it is called after the top level finishes.

### Variables

Use `let` for things that change and `const` for things that don't.

```javascript
let score = 10
const VERSION = "0.1"
score += 5
```

### Strings

Put any expression inside `{ }` and Luna fills it in. Write `\{` for a literal brace.

```javascript
let hp = 42
print("You have {hp} HP, next level at {hp + 8}")
```

### Functions

Parameters can have default values.

```javascript
func greet(name, greeting = "Hello") {
    return "{greeting}, {name}!"
}

print(greet("Astra"))         # Hello, Astra!
print(greet("Astra", "Hi"))   # Hi, Astra!
```

Functions can also remember things. This is called a closure:

```javascript
func make_counter() {
    let count = 0
    return func() {
        count += 1
        return count
    }
}

let next = make_counter()
print(next())   # 1
print(next())   # 2
```

### Lists

```javascript
let nums = [1, 2, 3, 4, 5]
append(nums, 6)
print(nums[-1])   # 6, negative indexes count from the end
```

`map`, `filter` and `reduce` take functions:

```javascript
let doubled = map(nums, func(n) { return n * 2 })
let evens = filter(nums, func(n) { return n % 2 == 0 })
let total = reduce(nums, func(acc, n) { return acc + n }, 0)
```

### Maps

```javascript
let player = {"name": "Astra", "hp": 100}
print(player.name)
map_set(player, "hp", 125)
```

### Loops and choices

Conditions must be booleans: `if (n)` is an error, write `if (n != 0)`.

```javascript
for (let i in range(3)) {
    print(i)
}

if (player.hp > 100) {
    print("Feeling strong!")
} else {
    print("Keep going")
}
```

### Your own types

`data` bundles values together.

```javascript
data Vec2 { x, y }

let p = Vec2(3, 4)
print("({p.x}, {p.y})")   # (3, 4)
```

### Modules

Split your code across files. Mark what you want to share with `export`:

```javascript
# utils.lu
export func greet(name) {
    return "Hello, {name}!"
}
```

Then bring the file in with `use`. Only the exported names are visible:

```javascript
# main.lu
use "utils.lu"
print(greet("Luna"))
```

To pick some names: `use { greet } from "utils.lu"`. Imported files are resolved relative to
the importing file when the program is compiled, and are built into the binary.

### Cleaning up automatically

`defer` runs a call when the current scope exits. It is handy for closing files.

```javascript
func save_log(path, text) {
    let file = open(path, "w")
    defer(close(file))
    write(file, text)
}
```

There is more in [docs/language/syntax.md](docs/language/syntax.md).

## Documentation

Everything starts at [docs/README.md](docs/README.md). Some good places to begin:

- **New to Luna?** [Getting started](docs/getting-started.md), then the
  [syntax guide](docs/language/syntax.md).
- **Looking up a function?** [Core](docs/stdlib/core.md), [strings](docs/stdlib/string.md),
  [math](docs/stdlib/math.md), [files](docs/stdlib/file.md).
- **Curious how it works inside?** [Architecture](docs/internals/architecture.md),
  [the compiler](docs/internals/lunac.md), [the garbage collector](docs/internals/gc.md),
  [unsafe memory](docs/internals/unsafe.md).
- **Want to help?** Start with [Testing](docs/testing.md) and
  [Known issues](docs/known-issues.md). The direction of the project is in
  [docs/plan-aot.md](docs/plan-aot.md).

## Project Layout

```
compiler/       the compiler (lexer, parser, LLVM IR backend, driver), written in Luna
compiler/runtime/   the runtime of compiled programs, written in Luna
bootstrap/      LLVM IR seeds of the compiler and the runtime, used to build the first luna
test/           Luna test programs and their expected output (test/expected/)
assets/         the logo
docs/           documentation
```

## License

Luna is free software under the **GNU General Public License v3.0 or later**. See
[LICENSE](LICENSE).
