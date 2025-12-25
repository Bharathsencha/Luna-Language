<p align="center">
  <img src="assets/luna.png" alt="Luna Logo" width="140">
</p>

# Luna 

Luna is my own self-initiated programming-language project—built from scratch in C—with the simple goal of creating something that’s easy to read, straightforward to work with, and fast af.

## Architecture Overview

The interpreter processes code in the following pipeline order:

### Source Code (.lu)
The raw text file containing the source code is read into memory.

### Lexer (lexer.c / token.c)
The source code is converted into a stream of Tokens.

- It handles whitespace skipping and comment removal (# and //).
- It recognizes keywords (let, if, func, for, switch), literals (numbers, strings), and operators.

Output: A sequence of tokens (e.g., T_LET, T_IDENT, T_EQ, T_NUMBER).

### Parser (parser.c)
The tokens are analyzed to ensure they follow the grammar rules.

- It constructs an Abstract Syntax Tree (AST) defined in ast.c.
- It handles operator precedence for mathematical expressions.
- It groups statements into blocks (functions, loops, conditional bodies).

Output: A hierarchical tree of nodes (e.g., NODE_FUNC_DEF, NODE_WHILE, NODE_BINOP).

### Interpreter (interpreter.c / value.c)
The AST is traversed recursively to execute the program.

- Environment: Manages memory scopes. Functions create new local scopes; global variables exist in the root scope.
- Execution: Performs arithmetic, executes control flow logic, and handles I/O.

Output: The actual program results printed to the console.

## File Structure & Explanation

- src/main.c: The entry point. Reads the file, initializes the parser, and calls the interpreter.
- src/lexer.c: Scans the source string and produces tokens. Handles string literals, numbers (int/float), and comments.
- src/parser.c: Consumes tokens to build the AST. Contains the logic for grammar rules (expressions, statements, blocks).
- src/ast.c: Defines the AST node structures and functions to create/free them.
- src/interpreter.c: The core runtime. executing the AST. Handles variable lookups, function calls, and control flow (return/break/continue).
- src/value.c: Manages the dynamic Value type (Int, Float, String, List, Bool) and memory management for values.
- src/token.c: Helper to convert token enums to string names for debugging.
- src/util.c: File reading utilities.
- include/*.h: Header files defining the structs and function prototypes for the corresponding .c files.

## Language Reference

### Core Features

| Feature | Syntax | Description |
|--------|--------|-------------|
| Declaration | let x = 10 | Declares a variable in the current scope. |
| Assignment | x = 20 | Updates an existing variable (searches parent scopes). |
| Lists | let arr = [1, 2, 3] | Creates a dynamic list of values. |
| Output | print(x) | Prints values to the standard output. |
| Input | input("Prompt") | Reads a string from the user. |
| Comments | # or // | Ignored by the interpreter. |

### Control Flow

| Keyword | Description |
|--------|-------------|
| if / else | Executes blocks based on boolean conditions. |
| while | Repeats a block while a condition is true. |
| for | Iterates over elements in a list (C like loop). |
| break | Exits the current loop or switch statement immediately. |
| continue | Skips to the next iteration of a loop. |
| switch | Compares a value against multiple case options. |

### Functions

| Feature | Syntax |
|---------|--------|
| Definition | func name(arg1, arg2) { ... } |
| Return | return value |
| Call | name(1, 2) |

### Built-in Functions

| Function | Description |
|----------|-------------|
| len(x) | Returns length of a string or list. |
| int(x) | Converts string/float/bool to integer. |
| float(x) | Converts string/int/bool to float. |
|type(x)|Says the variable type int,float etc.

# Standard Library Documentation

## This section outlines the standard library functions available in Luna (.lu)

---
# Luna Language – Documentation

This section contains detailed documentation for Luna’s standard library and core features.

## Modules

1. **Math Functions**  
   Mathematical utilities and numeric operations.  
   → [View Math Documentation](math.md)

2. **String Functions**  
   String manipulation, formatting, and utilities.  
   → [View String Documentation](string.md)

---

← [Back to Main README](../README.md)


## How to Build and Run

### 1. Build
```
make
```

### 2. Run
```
./luna myscript.lu
```
or
```
make run
```

### 3. Clean
```
make clean
```

## To Enter REPL mode
```
make repl
```

## User Input in .lu

The language supports runtime input using the built-in `input(prompt)` function.
It displays the given prompt string and waits for the user to type something, which is returned as a string.

### Example

```javascript
let name = input("Enter your name: ")
print("Hello,", name)
```

If you need the input as a number instead of a string, convert it using:
- `int(value)`
- `float(value)`

### Example (Number Input)

```javascript
let age_str = input("Enter your age: ")
let age = int(age_str)

print("You are", age, "years old.")
```

### or
````javascript
let age = int(input(""Enter your age: " ))
print(age)
````



## Test Examples

Save the following code blocks into .lu files to test specific functionalities.
```javascript
#print("=== List Test (C-Style For Loop) ===")

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

### 2. Switch Statement (Menu System)
```javascript
print("=== Menu System ===")
print("1. Start Game")
print("2. Load Game")
print("3. Exit")

let choice = input("Select an option (1-3): ")

# Convert input string to integer for comparison
let opt = int(choice)

switch (opt) {
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

## 2.1

### Here you can conver input to int by surrounding the input by int()
### Eg: let a = int(input(Statement))

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

### 3. Recursive Algorithms (Factorial)
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
### 4. Calculator using user input
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
    let n1 = float(input("Enter first number: "))
    let n2 = float(input("Enter second number: "))

   
    let result = 0.0

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
            if (n2 == 0.0) {
                print("Error: Cannot divide by zero!")
            } else {
                result = n1 / n2
                let sum = 0
                print("Result:", result)
            }
            break
        default:
            print("Invalid operation selected.")
    }
}
```
### 5. Temperature Converter
```javascript
print("1. Fahrenheit to Celsius")
print("2. Celsius to Fahrenheit")

let choice = input("Select mode (1 or 2): ")
let mode = int(choice)

if (mode == 1) {
    # F to C
    let f_str = input("Enter Temperature (F): ")
    let f = float(f_str)
    
    # Formula: (F - 32) * 5 / 9
    let c = (f - 32.0) * 5.0 / 9.0
    
    print("---------------------------------")
    print(f, "F is equal to", c, "C")
    
} else {
    if (mode == 2) {
        # C to F
        let c_str = input("Enter Temperature (C): ")
        let c = float(c_str)
        
        # Formula: (C * 9 / 5) + 32
        let f = (c * 9.0 / 5.0) + 32.0
        
        print("---------------------------------")
        print(c, "C is equal to", f, "F")
    } else {
        print("Invalid selection. Please run again.")
    }
}
```
### 6.Nested Function
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
