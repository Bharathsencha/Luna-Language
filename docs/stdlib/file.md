# File I/O

Text files, through the C library's buffered streams. File paths are relative to the working
directory of the running program, not to the source file.

## Reference

| Function | Description |
|---|---|
| `open(path, mode)` | Opens a file in mode `"r"`, `"w"` or `"a"`. Returns a file handle, or `null` if it cannot be opened. Any other mode is an `Argument Error` |
| `close(handle)` | Flushes and closes the file. Closing twice is a runtime error |
| `write(handle, text)` | Writes a string to the file |
| `read(handle)` | Reads the rest of the file into one string |
| `read_line(handle)` | Reads the next line without its newline; `null` at end of file |
| `file_exists(path)` | `true` if the path exists |
| `remove_file(path)` | Deletes the file; `true` on success, `false` on failure |
| `flush(handle)` | Writes buffered data out without closing the file |

A file handle has `type()` `"file"`. Using a handle after `close` is a runtime error
(`write() on a closed file`, `close() on a closed file`). Handles are not closed
automatically, so close every file you open, or use `defer(close(f))`.

## Modes

| Mode | Behavior |
|---|---|
| `"r"` | Reads an existing file. `open` returns `null` if it does not exist |
| `"w"` | Creates the file or truncates an existing one |
| `"a"` | Appends to the file, creating it if needed |

## Checking a handle

Conditions must be booleans, so compare the handle with `null`; `if (file)` is a type error.

```javascript
let file = open("data.txt", "w")
if (file != null) {
    write(file, "Header\n")
    write(file, "\tIndented item\n")
    write(file, "Quote: \"Hello World\"\n")
    close(file)
} else {
    print("Failed to open file.")
}
```

Escape sequences such as `\n`, `\t` and `\"` are part of the string literal, not of `write`.

## Reading

Whole file:

```javascript
let reader = open("data.txt", "r")
if (reader != null) {
    print(read(reader))
    close(reader)
}
```

Line by line:

```javascript
let file = open("data.txt", "r")
if (file != null) {
    let line = read_line(file)
    while (line != null) {
        print("Line: {line}")
        line = read_line(file)
    }
    close(file)
}
```

`read()` loads everything into memory; for large files prefer `read_line()`.

## Existence and deletion

```javascript
if (file_exists("old.txt")) {
    remove_file("old.txt")
}
```

## Example: a small configuration file

```javascript
let path = "app.cfg"

let cfg = open(path, "w")
if (cfg == null) {
    print("[ERROR] cannot create {path}")
    exit(1)
}
write(cfg, "mode=production\n")
write(cfg, "threads=8\n")
flush(cfg)
close(cfg)

let reader = open(path, "r")
let count = 0
let line = read_line(reader)
while (line != null) {
    let parts = split(line, "=")
    print("{parts[0]} -> {parts[1]}")
    count += 1
    line = read_line(reader)
}
close(reader)
print("[OK] loaded {count} entries")

remove_file(path)
```

Output:

```
mode -> production
threads -> 8
[OK] loaded 2 entries
```

## Best practices

1. Compare the result of `open` with `null` before using it.
2. Close every file, or `defer(close(f))` right after opening it.
3. `read_line` returns `null` at the end of the file, and an empty line is `""`.
4. Check `file_exists` before `remove_file` if you need to tell the two cases apart.
