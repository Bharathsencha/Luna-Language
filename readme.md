<p align="center">
  <img src="assets/luna.png" alt="Luna Logo" width="140">
</p>

# Luna 

This project implements a custom interpreted programming language written in C. It supports integers, floating-point numbers, strings, booleans, lists, variables, control flow (if/while/for/switch), and user-defined functions.

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

# Math Library

## These functions provide standard mathematical operations.

## Basic Utility

| Function             | Description                                       | Example Code            |
| -------------------- | ------------------------------------------------- | ----------------------- |
| `abs(x)`             | Returns the absolute value of x.                  | `abs(-10) → 10`         |
| `min(a, b)`          | Returns the smaller of two values.                | `min(10, 5) → 5`        |
| `max(a, b)`          | Returns the larger of two values.                 | `max(10, 5) → 10`       |
| `clamp(x, min, max)` | Constrains x between min and max.                 | `clamp(15, 0, 10) → 10` |
| `sign(x)`            | Returns -1 if negative, 1 if positive, 0 if zero. | `sign(-50) → -1`        |

## Powers & Roots

| Function         | Description                              | Example Code        |
| ---------------- | ---------------------------------------- | ------------------- |
| `pow(base, exp)` | Returns base raised to the power of exp. | `pow(2, 3) → 8.0`   |
| `sqrt(x)`        | Returns the square root of x.            | `sqrt(16) → 4.0`    |
| `cbrt(x)`        | Returns the cube root of x.              | `cbrt(27) → 3.0`    |
| `exp(x)`         | Returns e raised to the power of x.      | `exp(1) → 2.718...` |
| `ln(x)`          | Returns the natural logarithm of x.      | `ln(10) → 2.302...` |
| `log10(x)`       | Returns the base-10 logarithm of x.      | `log10(100) → 2.0`  |

## Rounding

| Function    | Description                                    | Example Code        |
| ----------- | ---------------------------------------------- | ------------------- |
| `floor(x)`  | Rounds down to the nearest integer.            | `floor(5.9) → 5`    |
| `ceil(x)`   | Rounds up to the nearest integer.              | `ceil(5.1) → 6`     |
| `round(x)`  | Rounds to the nearest integer.                 | `round(5.5) → 6`    |
| `trunc(x)`  | Removes the fractional part.                   | `trunc(5.9) → 5`    |
| `fract(x)`  | Returns only the fractional part.              | `fract(5.9) → 0.9`  |
| `mod(x, y)` | Returns the floating-point remainder of x / y. | `mod(5.5, 2) → 1.5` |

## Trigonometry

| Function      | Description                                    | Example Code               |
| ------------- | ---------------------------------------------- | -------------------------- |
| `sin(rad)`    | Returns sine of an angle (in radians).         | `sin(0) → 0.0`             |
| `cos(rad)`    | Returns cosine of an angle (in radians).       | `cos(0) → 1.0`             |
| `tan(rad)`    | Returns tangent of an angle (in radians).      | `tan(0) → 0.0`             |
| `asin(x)`     | Returns arc sine.                              | `asin(0) → 0.0`            |
| `acos(x)`     | Returns arc cosine.                            | `acos(1) → 0.0`            |
| `atan(x)`     | Returns arc tangent.                           | `atan(0) → 0.0`            |
| `atan2(y, x)` | Returns arc tangent of y/x (correct quadrant). | `atan2(10, 10) → 0.785...` |

## Hyperbolic

| Function  | Description                 | Example Code    |
| --------- | --------------------------- | --------------- |
| `sinh(x)` | Returns hyperbolic sine.    | `sinh(0) → 0.0` |
| `cosh(x)` | Returns hyperbolic cosine.  | `cosh(0) → 1.0` |
| `tanh(x)` | Returns hyperbolic tangent. | `tanh(0) → 0.0` |

## Random

| Function            | Description                                   | Example Code         |
| ------------------- | --------------------------------------------- | -------------------- |
| `rand()`            | Returns a random float between 0.0 and 1.0.   | `rand() → 0.42...`   |
| `randint(min, max)` | Returns a random integer between min and max. | `randint(1, 10) → 7` |
| `srand(seed)`       | Seeds the random number generator.            | `srand(123)`         |

## Conversions

| Function          | Description                                | Example Code                  |
| ----------------- | ------------------------------------------ | ----------------------------- |
| `deg_to_rad(deg)` | Converts degrees to radians.               | `deg_to_rad(180) → 3.14...`   |
| `rad_to_deg(rad)` | Converts radians to degrees.               | `rad_to_deg(3.14) → 179.9...` |
| `lerp(a, b, t)`   | Linear interpolation between a and b by t. | `lerp(0, 100, 0.5) → 50.0`    |

----

# String Library

### These functions facilitate text manipulation and inspection.
---
## Inspection

| Function      | Description                     | Example Code          |
| ------------- | ------------------------------- | --------------------- |
| `str_len(s)`  | Returns the length of string s. | `str_len("abc") → 3`  |
| `is_empty(s)` | Returns true if s length is 0.  | `is_empty("") → true` |

## Slicing & Access

| Function                   | Description                                | Example Code                      |
| -------------------------- | ------------------------------------------ | --------------------------------- |
| `char_at(s, i)`            | Returns character at index i as a string.  | `char_at("ABC", 1) → "B"`         |
| `substring(s, start, len)` | Extracts len chars starting at start.      | `substring("Hello", 0, 2) → "He"` |
| `slice(s, start, end)`     | Extracts from start index up to end index. | `slice("Hello", 1, 4) → "ell"`    |

## Searching

| Function                | Description                               | Example Code                             |
| ----------------------- | ----------------------------------------- | ---------------------------------------- |
| `contains(s, sub)`      | Returns true if sub is inside s.          | `contains("Team", "ea") → true`          |
| `index_of(s, sub)`      | Returns index of first occurrence of sub. | `index_of("banana", "nan") → 2`          |
| `last_index_of(s, sub)` | Returns index of last occurrence of sub.  | `last_index_of("banana", "a") → 5`       |
| `starts_with(s, pre)`   | Returns true if s starts with pre.        | `starts_with("file.txt", "file") → true` |
| `ends_with(s, suf)`     | Returns true if s ends with suf.          | `ends_with("file.txt", ".txt") → true`   |

## Manipulation

| Function                | Description                           | Example Code                             |
| ----------------------- | ------------------------------------- | ---------------------------------------- |
| `concat(s1, s2)`        | Concatenates two strings.             | `concat("He", "llo") → "Hello"`          |
| `to_upper(s)`           | Converts string to uppercase.         | `to_upper("abc") → "ABC"`                |
| `to_lower(s)`           | Converts string to lowercase.         | `to_lower("ABC") → "abc"`                |
| `trim(s)`               | Removes whitespace from both ends.    | `trim("  hi  ") → "hi"`                  |
| `trim_left(s)`          | Removes whitespace from the start.    | `trim_left("  hi") → "hi"`               |
| `trim_right(s)`         | Removes whitespace from the end.      | `trim_right("hi  ") → "hi"`              |
| `replace(s, old, new)`  | Replaces occurrences of old with new. | `replace("foobar", "o", "a") → "faabar"` |
| `reverse(s)`            | Reverses the string.                  | `reverse("Luna") → "anuL"`               |
| `repeat(s, n)`          | Repeats string s, n times.            | `repeat("Na", 3) → "NaNaNa"`             |
| `pad_left(s, w, char)`  | Pads start of s to width w with char. | `pad_left("7", 3, "0") → "007"`          |
| `pad_right(s, w, char)` | Pads end of s to width w with char.   | `pad_right("Ok", 5, ".") → "Ok..."`      |

## Lists & Formatting

| Function            | Description                         | Example Code                            |
| ------------------- | ----------------------------------- | --------------------------------------- |
| `split(s, delim)`   | Splits string into a list by delim. | `split("a,b,c", ",") → ["a", "b", "c"]` |
| `join(list, delim)` | Joins a list of strings into        |                                         |


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
