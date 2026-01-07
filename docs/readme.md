<p align="center">
  <img src="../assets/luna.png" alt="Luna Logo" width="140">
</p>

# Luna 

Luna is my own self-initiated programming language project—built from scratch in C—with the simple goal of creating something that's easy to read, straightforward to work with, and fast as hell.

## Why Luna?

Luna was born from a simple frustration: modern programming languages often get in your way. Whether it's wrestling with memory management, fighting strict type systems, or memorizing complex syntax rules, coding can feel harder than it needs to be.

Luna takes a different approach—**just let you code**. No compiler battles, no boilerplate, no unnecessary complexity. It combines the intuitive parts of languages you already know while leaving out the friction:

- **Familiar syntax** inspired by JavaScript and Python
- **Simple I/O** with easy-to-use `input()` and `print()` functions
- **Flexible typing** that adapts to what you're building
- **No setup headaches**—compile and run

**Perfect for:**
- Beginners learning programming fundamentals without the usual frustrations
- Quick scripting and prototyping when you just want to build something
- Anyone who values readable, maintainable code over clever tricks

**What makes Luna different:**
- **Clean Syntax**: Write code that reads like what it does
- **Fast Execution**: Built in C for performance where it matters
- **Dynamic Typing**: Mix types freely without ceremony
- **Rich Standard Library**: Math, strings, files, and more—batteries included

---

## Installation

### Prerequisites
- GCC or Clang compiler
- Make

### Setup
```bash
# Clone the repository
git clone https://github.com/yourusername/luna.git
cd luna

# Build Luna
make

# Run your first program
./luna your_file_name.lu
```

## Quick Start

Create a file called `hello.lu`:

```javascript
print("Hello, Luna!")

let name = input("What's your name? ")
print("Welcome to Luna,", name, "!")
```

Run it:
```bash
./luna hello.lu
```

**Note:** Luna uses JavaScript-like syntax, so use JavaScript syntax highlighting in your editor for the best experience.

---

## Language Reference

### Core Features

| Feature | Syntax | Description |
|---------|--------|-------------|
| Declaration | `let x = 10` | Declares a variable in the current scope |
| Assignment | `x = 20` | Updates an existing variable (searches parent scopes) |
| Lists | `let arr = [1, 2, 3]` | Creates a dynamic list of values |
| Output | `print(x)` | Prints values to standard output |
| Input | `input("Prompt")` | Reads a string from the user |
| Comments | `#` or `//` | Ignored by the interpreter |
|Escape Sequences| `\n`, `\t`, `\` | "Newline, tab, and quote characters in strings|

### Control Flow

| Keyword | Description |
|---------|-------------|
| `if` / `else` | Executes blocks based on boolean conditions |
| `while` | Repeats a block while a condition is true |
| `for` | Iterates over elements in a list (C-like loop) |
| `break` | Exits the current loop or switch statement immediately |
| `continue` | Skips to the next iteration of a loop |
| `switch` | Compares a value against multiple case options |

### Functions

Functions in Luna are defined using the `func` keyword.

| Feature | Syntax |
|---------|--------|
| Definition | `func name(arg1, arg2) { ... }` |
| Return | `return value` |
| Call | `name(1, 2)` |

### Built-in Functions

| Function | Description |
|----------|-------------|
| `len(x)` | Returns length of a string or list |
| `int(x)` | Converts string/float/bool to integer |
| `float(x)` | Converts string/int/bool to float |
| `type(x)` | Returns the variable type (int, float, etc.) |
| `append(list, value)` | Adds a value to the end of a list |

### Operators & Comparisons

**Arithmetic Operators:** `+`, `-`, `*`, `/`, `%`

**Comparison Operators:** `==`, `!=`, `<`, `>`, `<=`, `>=`

**Logical Operators:** `&&` (and), `||` (or), `!` (not)

### List Operations

Lists in Luna are dynamic arrays that can hold mixed types.

```javascript
let arr = [1, 2, 3, "hello", 5.5]
let first = arr[0]        # Access by index
arr[1] = 10               # Modify by index
append(arr, "new item")   # Add to end
let size = len(arr)       # Get length
```

## Modules

Luna's standard library provides additional functionality through modules:

1. **Architecture & Implementation**  
   Detailed overview of Luna's internal architecture and file structure  
   → [View Architecture Documentation](arch.md)

2. **Math Functions**  
   Mathematical utilities and numeric operations  
   → [View Math Documentation](math.md)

3. **String Functions**  
   String manipulation, formatting, and utilities  
   → [View String Documentation](string.md)

4. **File Functions**  
   File creation, reading, and writing operations  
   → [View File Documentation](file.md)

5. **Performance Extensions**  
   High-precision timing and hardware-accelerated SIMD vector mathematics  
   → [View Performance Documentation](performance.md)

---

## How to Build and Run

### 1. Build
```bash
make
```

### 2. Run (Manually)
```bash
./luna myscript.lu
```

### Or Let Makefile Handle It
By default, it will run `main.lu`. You can modify it in the Makefile:
```bash
make run
```

### 3. Clean
```bash
make clean
```

## To Enter REPL Mode

The REPL (Read-Eval-Print Loop) allows you to test Luna code interactively:

```bash
make repl
```

In REPL mode, you can:
- Execute Luna statements line by line
- Test expressions and see immediate results
- Define functions and variables for quick experimentation
- Exit by pressing `Ctrl+C` or typing your exit command

---

## User Input in Luna

The language supports runtime input using the built-in `input(prompt)` function. It displays the given prompt string and waits for the user to type something, which is returned as a string.

### Example

```javascript
let name = input("Enter your name: ")
print("Hello,", name)
```

If you need the input as a number instead of a string, convert it using:
- `int(value)`
- `float(value)`

### Example (Integer Input)

```javascript
let age = int(input("Enter your age: "))
print(age)
```

---

## Error Handling

Luna provides runtime error messages for common issues:
- **Type errors**: Invalid operations on incompatible types
- **Undefined variables**: Accessing variables that don't exist
- **Division by zero**: Arithmetic errors
- **Index out of bounds**: Accessing invalid list indices
- **Function errors**: Wrong number of arguments or undefined functions

When an error occurs, Luna will display a descriptive message and halt execution.

---

## Test Examples

Save the following code blocks into `.lu` files to test specific functionalities.

### 1. Getting Started - Basic Operations
```javascript
# Variables and basic math
let x = 10
let y = 5
print("Sum:", x + y)
print("Product:", x * y)

# Multiple assignments in one line
let a, b, c = 10, 1.1, "Hello"
print("a:", a)
print("b:", b)
print("c:", c)

# Strings
let greeting = "Hello"
let name = "Luna"
print(greeting, name)

# Escape sequences
print("Line 1\nLine 2")        # Newline
print("Col1\tCol2\tCol3")      # Tabs
print("She said \"Hello!\"")   # Quotes

# Lists
let numbers = [1, 2, 3, 4, 5]
print("Numbers:", numbers)
print("First number:", numbers[0])
```

### 2. List Operations 

```javascript
# print("=== List Test ===")

let grades = [85, 92, 78, 90, 88]
let sum = 0
let count = len(grades)

print("Grades:", grades)

for (let i = 0; i < len(grades); i++) {
    let g = grades[i]
    sum = sum + g
}

# Calculate average (convert to float for precision)
let average = float(sum) / float(count)

print("Class Average:", average)

if (average >= 90.0) {
    print("Excellent Performance!")
} else {
    print("Good Job.")
}
```

### 3. Switch Statement 

```javascript
print("=== Menu System ===")
print("1. Start Game")
print("2. Load Game")
print("3. Exit")

let choice = int(input("Select an option (1-3): "))

switch (choice) {
    case 1:
        print("Starting new game...")
        break
    case 2:
        print("Loading save file...")
        break
    case 3:
        print("Exiting. Goodbye!")
        break
    default:
        print("Invalid selection!")
}
```

### 4. Recursive Algorithms 

```javascript
print("=== Recursion Test ===")

func factorial(n) {
    if (n <= 1) {
        return 1
    }
    return n * factorial(n - 1)
}

let num = 6
let result = factorial(num)

print("Factorial of", num, "is", result)
```

### 5. Calculator Using User Input

```javascript
print("      SIMPLE CALCULATOR          ")

while (true) {
    print("")
    print("Options: +, -, *, / or 'exit'")
    let op = input("Enter operation: ")

    if (op == "exit") {
        print("Exiting calculator. Goodbye!")
        break
    }

    # Read inputs as strings first
    let n1 = int(input("Enter first number: "))
    let n2 = int(input("Enter second number: "))

    let result = 0

    switch (op) {
        case "+":
            result = n1 + n2
            print("Result:", result)
            break
        case "-":
            result = n1 - n2
            print("Result:", result)
            break
        case "*":
            result = n1 * n2
            print("Result:", result)
            break
        case "/":
            if (n2 == 0) {
                print("Error: Cannot divide by zero!")
            } else {
                result = n1 / n2
                print("Result:", result)
            }
            break
        default:
            print("Invalid operation selected.")
    }
}
```

### 6. Nested Function

```javascript
func get_user_items() {
    print("How many items do you want to add?")
    
    let count = int(input("Enter count: "))
    let list = []
    
    print("Okay, please enter", count, "items below:")

    for (let i = 0; i < count; i++) {
        let e = input("> ")
        append(list, e)
    }
    
    return list
}

let my_items = get_user_items()

print("You collected:", my_items)
print("Total items:", len(my_items))
```

---

## Contributing

Feel free to open issues or submit pull requests to improve Luna!

## License

---
This project is licensed under the GNU General Public License v3.0 (GPLv3). See the [LICENSE](LICENSE) file for details.

Luna is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
