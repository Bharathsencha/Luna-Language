# Language Documentation

## "Balls" Syntax Extension

For users who enjoy a more eccentric coding style, the following aliases are supported.  
They behave exactly like their standard keyword counterparts.

| "Balls" Syntax     | Standard Keyword | Description                               |
|--------------------|------------------|-------------------------------------------|
| `balls`            | `let`            | Variable declaration                      |
| `big_balls`        | `let`            | Variable declaration (intended for large or float values) |
| `shared_balls`     | `let`            | Variable declaration (intended for globals) |
| `loop_your_balls`  | `for`            | For-loop iterator                         |
| `spin_balls`       | `while`          | While loop                                |
| `if_balls`         | `if`             | Conditional block                         |
| `else_balls`       | `else`           | Else block                                |
| `switch_balls`     | `switch`         | Switch statement                          |
| `drop_balls`       | `break`          | Exit the current loop                     |
| `jiggle_balls`     | `continue`       | Continue to next iteration                |
| `grab_balls`       | `func`           | Function definition                       |

---

## Examples

### **Variable Declarations**
```
balls x = 10
big_balls PI = 3.14159
shared_balls user_name = "Admin"

```

### For Loop
```
loop_your_balls i in [1, 2, 3] {
    print(i)
}

```

### While Loop
```
spin_balls (x < 20) {
    print("Spinning...")
    x = x + 1
}
```

### Function Definition
```
grab_balls calculate_area(w, h) {
    return w * h
}
```
