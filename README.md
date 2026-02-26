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
- **Creative Coding**: Direct bindings for Raylib for 2D/3D graphics, audio, and UI development.
- **Self-Hosting**: Includes a bootstrap interpreter and transpiler written in Luna itself.(Removed).

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
```

Run it:
```bash
./bin/luna hello.lu
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
| Comments | `#` or `//` | Standard inline comments |

### Performance Extensions

| Function | Description |
|----------|-------------|
| `dense_list(size, fill)` | Pre-flattened, SIMD-ready float array |
| `vec_mul(A, B)` | SIMD-accelerated vector multiplication |
| `clock()` | High-precision monotonic timer |

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
- **[Installation Guide](docs/installation.md)**: Comprehensive setup for Luna and Raylib.
- **[Performance Benchmarks](docs/benchmarks.md)**: Detailed speed comparisons vs Python and C.
- **[Standard Library](docs/readme.md)**
- **[Demo Technical Walkthrough](fun/README.md)**: A deep, exhaustive explanation of the `fun/` directory.

---

## Legal Disclaimer

I do not own any of the songs or assets used in the demonstration projects. All materials belong to their respective artists. For a full list of music, fonts, and icon credits, please refer to **[musicplayer/CREDITS.md](musicplayer/CREDITS.md)**.

---

## License

This project is licensed under the GNU General Public License v3.0 (GPLv3). See the [LICENSE](LICENSE) file for details.

---