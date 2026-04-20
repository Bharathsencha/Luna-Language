<p align="center">
  <img src="assets/luna.png" alt="Luna Logo" width="140">
</p>

# Luna 

Luna is a high-performance programming language built from scratch in C, designed for creative coding and high-speed scripting.

The project explores the boundary between high-level scripting ease (JavaScript-like syntax) and bare-metal performance. By leveraging professional-grade optimizations like SIMD vectorization and memory arenas, Luna achieves execution speeds comparable to C and NumPy on heavy workloads.

---

## Key Features

- **High-Performance Runtime**: SIMD-accelerated math, constant-time environment lookups, and O(1) string interning.
- **Modern Syntax**: JavaScript-inspired syntax for ease of use.
- **Creative Coding**:New GUI based on OpenGL, [Mini Audio](https://github.com/mackron/miniaudio) and [STB](https://github.com/nothings/stb).
=======
- **Sub-MS GC**: The current tracing GC benchmark suite is below 1ms max pause.
- **Modern Syntax**: JavaScript-inspired syntax with explicit `use` modules and interpolation-ready strings.
- **Manual Memory Tools**: `unsafe` blocks with `alloc`, `store`, `deref`, `ptr_add`, `addr`, and `defer`.
- **Own Data Types**: Luna ships native `bloc` / `box` / `template` data tiers with a Rust-backed validation runtime; see [Data Types Reference](docs/data_types.md).
- **Creative Coding**: Direct bindings for Luna's native OpenGL/GLFW graphics and audio layer for interactive apps and experiments.

---

## Setup
```bash
# Clone the repository
git clone https://github.com/Bharathsencha/Luna-Language
cd Luna-Language
```

```bash
# Build the Luna toolchain
make
```

```bash
# Run your first program
./bin/luna your_script.lu
```

---

## Quick Start
Create a file called `hello.lu`:

```javascript
print("Hello, Luna!")

let name = input("What's your name? ")
print("Welcome to Luna,", name, "!")
let greeting = "Hello {name}"
print(greeting)
```

Run it:
```bash
./bin/luna hello.lu
```

### Type Inference And Input

Luna infers a variable's runtime type from the value assigned to it.

```javascript
let x = 10        # int
let y = 1.5       # float
let name = "Luna" # string
let ok = true     # boolean
```

`input(prompt)` always returns a `string`, even if the user types digits.

```javascript
let raw_age = input("Age: ")  # "10"
let age = int(raw_age)        # 10
```

---

## Language Reference

### Core Syntax

| Feature | Syntax | Description |
|---------|--------|-------------|
| Declaration | `let x = 10` | Declares a variable in the current scope |
| Functions | `func add(a, b) { return a + b }` | First-class function support |
| Control Flow | `if`, `else`, `while`, `for`, `switch` | Standard structured control flow |
| Lists | `let arr = [1, 2, 3]` | Dynamic, mixed-type lists |
| Maps | `let cfg = {"width": 1280}` | String-keyed hash maps for config/state |
| Modules | `use {greet} from "utils.lu"` | Loads explicit exports from another Luna file |
| Comments | `#` or `//` | Standard inline comments |
| String Interpolation | `let msg = "sum = {1 + 2}"` | Evaluates Luna expressions inside string literals at runtime |
| Data Types | `data Vec2 { x, y }` | Declares a tagged constructor for your own structured values |

### Performance Extensions

| Function | Description |
|----------|-------------|
| `dense_list(size, fill)` | Pre-flattened, SIMD-ready float array |
| `vec_mul(A, B)` | SIMD-accelerated vector multiplication |
| `range(start, end, step)` | Builds integer ranges for loops and iteration |
| `clock()` | High-precision monotonic timer |

### Collection Helpers

| Function | Description |
|----------|-------------|
| `map_keys(m)` | Returns the map's keys as a list |
| `map_values(m)` | Returns the map's values as a list |
| `map_items(m)` | Returns `[key, value]` pairs as a list |
| `shape(value)` | Returns the data tag for a data value |

### Memory Functions

| Function | Description |
|----------|-------------|
| `alloc(n)` | Allocates `n` Luna value slots inside `unsafe` |
| `free(ptr)` | Frees an allocation pointer manually |
| `defer(ptr)` | Frees a pointer automatically when the unsafe block ends |
| `defer(close(file))` | Schedules a cleanup call when the current scope ends |
| `deref(ptr)` | Reads a value through a pointer |
| `store(ptr, value)` | Writes a value through a pointer |
| `ptr_add(ptr, offset)` | Moves to another slot in the same allocation |
| `addr(name)` | Takes the address of a named variable |
| `int(ptr)` | Returns a raw integer address from a pointer |

### Runtime Docs

- `docs/memory.md` for the unsafe memory API guide
- `docs/gc.md` for the tracing GC design, benchmarks, and current sub-ms pause profile
- `docs/data structure.md` for the planned `bloc`, `box`, and `template` model
- `rust/readme.md` for the Rust static-runtime crates and build layout

### Structured Data Types

Luna ships three native structured-data tiers, each optimized for a different
use case:

```javascript
bloc Vec2 { x, y }
template Player { name, hp, pos }

let p = Vec2{3, 4}
let hero = Player{"Astra", 100, p}
let buf = box[256]
```

- `bloc` — tiny immutable inline values (GC-ignored, cache-line capped)
- `box` — manual non-GC heap buffers with scope-based cleanup
- `template` — rich schema-backed GC-managed objects with named field access

Short example:

```javascript
unsafe {
    let buf = alloc(4)
    defer(buf)

    for (let i = 0; i < 4; i++) {
        store(ptr_add(buf, i), i * 10)
    }

    for (let i = 0; i < 4; i++) {
        print("slot", i, "=", deref(ptr_add(buf, i)))
    }
}
```

---

## Examples & Test Programs

Here are some complete programs you can run to explore Luna's capabilities.

### 1. Basic Operations
```javascript
# Variables and basic math
let x = 10
let y = 5
print("Sum:", x + y)
print("Product:", x * y)

# Multiple assignments
let a, b, c = 10, 1.1, "Hello"
print("a:", a, "b:", b, "c:", c)

# String escaping
print("Line 1\nLine 2")
print("She said \"Hello!\"")
```

### 2. List Operations
```javascript
let grades = [85, 92, 78, 90, 88]
let sum = 0
for (let i = 0; i < len(grades); i++) {
    sum = sum + grades[i]
}
let average = float(sum) / float(len(grades))
print("Grades:", grades)
print("Average:", average)
```

### 2.1. For-In Loops
```javascript
let names = ["Luna", "Sol", "Nova"]
for (let name in names) {
    print(format("Hello {}", name))
}
```

### 2.2. Modules
```javascript
# utils.lu
export func greet(name) {
    return "Hello {name}"
}
```

```javascript
# main.lu
use {greet} from "utils.lu"
print(greet("Luna"))
```

### 3. Switch Statement & Input
```javascript
print("1. Start Game", "2. Load Game", "3. Exit")
let choice = int(input("Select (1-3): "))

switch (choice) {
    case 1: print("Starting..."); break
    case 2: print("Loading..."); break
    case 3: print("Goodbye!"); break
    default: print("Invalid!")
}
```

### 4. Recursive Algorithms
```javascript
func factorial(n) {
    if (n <= 1) { return 1 }
    return n * factorial(n - 1)
}
print("Factorial of 6 is", factorial(6))
```

### 5. Interactive Calculator
```javascript
while (true) {
    let op = input("Op (+, -, *, /) or 'exit': ")
    if (op == "exit") { break }
    let n1 = int(input("N1: "))
    let n2 = int(input("N2: "))
    switch (op) {
        case "+": print("Result:", n1 + n2); break
        case "-": print("Result:", n1 - n2); break
        case "*": print("Result:", n1 * n2); break
        case "/": 
            if (n2 == 0) { print("Error!"); }
            else { print("Result:", n1 / n2); }
            break
    }
}
```

### 6. Dynamic Lists
```javascript
func get_items() {
    let count = int(input("How many items? "))
    let list = []
    for (let i = 0; i < count; i++) {
        append(list, input("> "))
    }
    return list
}
print("You collected:", get_items())
```

### 7. Hashmaps 
Hashmaps in Luna are string-keyed objects for app state, configuration, save data, and lookup tables.
You write them with braces and string keys, then use the map helpers to read or update fields.

```javascript
let config = {
    "title": "Luna",
    "width": 1280,
    "height": 720
}

print(map_get(config, "title"))
map_set(config, "fullscreen", true)
print(map_has(config, "fullscreen"))
```

```javascript
let player = {
    "name": "Astra",
    "hp": 100,
    "zone": "forest",
    "inventory_count": 3
}

if (map_has(player, "hp")) {
    map_set(player, "hp", map_get(player, "hp") + 25)
}

map_set(player, "last_checkpoint", "ruins_gate")

print("Loaded player:", player)
print("Known fields:", map_keys(player))
print("Known values:", map_values(player))
print("Known entries:", map_items(player))
```

### 7.1. String Interpolation

String literals can interpolate full Luna expressions directly and still support normal
concatenation.

```javascript
let name = "Luna"
let score = 42
let nums = [10, 20, 30]

let hello = "Hello {name}"
let line = "Player {name} scored {score}"
let expr = "next score = {score + 1}, slot = {nums[1]}"

print(hello)
print(line)
print(expr)
print("Hello " + name)
```

### 7.2. Range

```javascript
for (let i in range(0, 5)) {
    print("step", i)
}
```

### 8. Higher-Order List Workflows
Now that functions are first-class values, you can pass closures into list helpers directly.

```javascript
let numbers = [1, 2, 3, 4, 5]

let doubled = map(numbers, func(x) {
    return x * 2
})

let evens = filter(doubled, func(x) {
    return x % 4 == 0
})

let total = reduce(evens, func(acc, x) {
    return acc + x
}, 0)

print(doubled) # [2, 4, 6, 8, 10]
print(evens)   # [4, 8]
print(total)   # 12
```

### 9. Scope Cleanup

```javascript
func write_note(path, text) {
    let file = open(path, "w")
    defer(close(file))
    write(file, text)
}
```

### 10. Data Types

```javascript
data Vec2 { x, y }
data Result { ok, value }

let p = Vec2(3, 4)
let r = Result(true, 99)

print(shape(p))
print(p["x"], p["y"])
print("point = ({p[\"x\"]}, {p[\"y\"]})")
print(shape(r), r["ok"], r["value"])
```

---

## User Input in Luna

The language supports runtime input using the built-in `input(prompt)` function. It displays the given prompt string and waits for the user to type something, which is returned as a string.

```javascript
let name = input("Enter your name: ")
print("Hello,", name)
```

If you need the input as a number instead of a string, convert it using `int(value)` or `float(value)`.

---

## Error Handling

Luna provides clear diagnostics for:
- **Type errors**: Invalid operations on incompatible types.
- **Undefined variables**: Accessing variables that don't exist.
- **Index out of bounds**: Accessing invalid list indices.
- **Division by zero**: Arithmetic errors.

The interpreter will highlight the exact line and provide a hint to help you fix the issue.

---

## Further Documentation

- **[Architecture Guide](docs/arch.md)**: Deep dive into the compiler and interpreter internals.
- **[Data Types Reference](docs/data_types.md)**: Complete guide to `bloc`, `box`, and `template` — rules, bounds, examples, and memory layout.
- **[Rust Data-Type Runtime](rust/readme.md)**: The `data_rt` crate — kind tags, layout validation, schema access, and C bridge.
- **[Installation Guide](docs/installation.md)**: Comprehensive setup for Luna.
- **[Performance Benchmarks](docs/benchmarks.md)**: Detailed speed comparisons vs Python and C.
- **[Rust Runtime Notes](rust/readme.md)**: How Luna's Rust static libraries are organized and built.
- **[Demo Technical Walkthrough](fun/README.md)**: A deep, exhaustive explanation of the `fun/` directory.

---

## Legal Disclaimer

I do not own any of the songs or assets used in the demonstration projects. All materials belong to their respective artists. For a full list of music, fonts, and icon credits, please refer to **[musicplayer/CREDITS.md](musicplayer/CREDITS.md)**.

---

## License

This project is licensed under the GNU General Public License v3.0 or later (GPLv3+). See the [LICENSE](LICENSE) file for details.

---
