# Luna File I/O Library

The File I/O library provides native filesystem operations through direct Linux system bindings, enabling Luna scripts to read, write, and manage files efficiently.

---

## File Function Reference

| Function              | Description                                                                                | Syntax                    |
| --------------------- | ------------------------------------------------------------------------------------------ | ------------------------- |
| `open(path, mode)`    | Opens a file stream in specified mode (`"r"`, `"w"`, `"a"`). Returns file handle or `null` | `open("file.txt", "w")`   |
| `close(handle)`       | Flushes and closes an open file handle                                                     | `close(file)`             |
| `write(handle, text)` | Writes a string to the file. Supports escape sequences (`\n`, `\t`, `\"`)                  | `write(file, "Hello\n")`  |
| `read(handle)`        | Reads the entire file content into a single string                                         | `read(file)`              |
| `read_line(handle)`   | Reads the next line from file (strips trailing `\n`). Returns `null` at EOF                | `read_line(file)`         |
| `file_exists(path)`   | Returns `true` if file exists on disk, otherwise `false`                                   | `file_exists("data.txt")` |
| `remove_file(path)`   | Deletes the specified file from disk. Returns `true` on success                            | `remove_file("old.txt")`  |
| `flush(handle)`       | Force-writes buffered data to disk without closing the file                                | `flush(file)`             |

---

## File Modes

| Mode  | Description | Behavior                                                                               |
| ----- | ----------- | -------------------------------------------------------------------------------------- |
| `"r"` | Read mode   | Opens existing file for reading. Returns `null` if file doesn't exist                  |
| `"w"` | Write mode  | Creates new file or truncates existing file. Overwrites content                        |
| `"a"` | Append mode | Opens file for appending. Creates file if it doesn't exist. Preserves existing content |

---

## Quick Examples

### Opening and Closing Files

```javascript
let file = open("data.txt", "w")
if (file) {
    print("File opened successfully.")
    close(file)
} else {
    print("Failed to open file.")
}
```

### Writing Data with Escape Sequences

```javascript
let writer = open("notes.txt", "w")
if (writer) {
    write(writer, "Header\n")
    write(writer, "\tIndented item\n")
    write(writer, "Quote: \"Hello World\"\n")
    close(writer)
}
```

### Reading Entire File Content

```javascript
let reader = open("notes.txt", "r")
if (reader) {
    let content = read(reader)
    print("File content:", content)
    close(reader)
}
```

### Reading Line by Line

```javascript
let file = open("data.txt", "r")
if (file) {
    let line = read_line(file)
    while (line != null) {
        print("Line:", line)
        line = read_line(file)
    }
    close(file)
}
```

### File Existence and Deletion

```javascript
if (file_exists("old.txt")) {
    remove_file("old.txt")
    print("File deleted successfully.")
} else {
    print("File does not exist.")
}
```

---

## Example 1: Basic File Operations

```javascript
let filename = "test.txt"

# 1. Writing with newline and tab support
print("Writing to file...")
let f_out = open(filename, "w")
if (f_out) {
    write(f_out, "First Line\n")
    write(f_out, "Second Line with \t tabs\n")
    write(f_out, "Third Line: \"Quotes inside!\"")
    close(f_out)
    print("Done writing.")
}

# 2. Check if file exists
if (file_exists(filename)) {
    print("Verification: File exists on disk.")
}

# 3. Reading line by line
print("\nReading line by line:")
let f_in = open(filename, "r")
if (f_in) {
    let l1 = read_line(f_in)
    let l2 = read_line(f_in)
    let l3 = read_line(f_in)
    
    print("L1:", l1)
    print("L2:", l2)
    print("L3:", l3)
    print("Length of L1:", len(l1))
    
    close(f_in)
}

# 4. Cleanup
print("\nCleaning up...")
remove_file(filename)
print("File removed. Test complete.")
```

---

## Example 2: Configuration Manager

```javascript
print("=== Luna Config Manager ===")
let path = "app.cfg"

# 1. Start fresh - remove old config
if (file_exists(path)) {
    remove_file(path)
}

# 2. Write configuration
let cfg = open(path, "w")
if (!cfg) {
    print("[error] Cannot create config")
} else {
    write(cfg, "mode=production\n")
    write(cfg, "threads=8\n")
    write(cfg, "logging=true\n")
    flush(cfg)  # Ensure data is written to disk
    close(cfg)
    print("[ok] Config written")
}

# 3. Reload and parse configuration
let reader = open(path, "r")
if (!reader) {
    print("[error] Cannot read config")
} else {
    let line = read_line(reader)
    let count = 0
    
    while (line != null) {
        # Basic validation
        if (len(line) == 0) {
            print("[warn] Empty config entry")
        } else {
            print("[cfg]", line)
        }
        count = count + 1
        line = read_line(reader)
    }
    
    close(reader)
    print("[ok] Loaded", count, "entries")
}

# 4. Safe removal
if (remove_file(path)) {
    print("[done] Config cleaned up")
} else {
    print("[error] Cleanup failed")
}

print("=== End ===")
```

---

## Example 3: Mission Control Data Sync

```javascript
print("--- [ Luna Mission Control: Data Sync ] ---")
let log_path = "system_log.txt"

# 1. Prepare environment
if (file_exists(log_path)) {
    remove_file(log_path)
    print(">> Previous log cleared.")
}

# 2. Write complex strings with escape sequences
print(">> Initializing Log Write...")
let stream = open(log_path, "w")
if (stream) {
    write(stream, "STATUS: Operational\n")
    write(stream, "METRICS:\n")
    write(stream, "\t- Internal Temp: 32C\n")
    write(stream, "\t- Pressure: Stable\n")
    write(stream, "MESSAGE: \"All systems go!\"\n")
    close(stream)
    print(">> Write Successful.")
}

# 3. Verification
if (file_exists(log_path)) {
    print(">> Verification: Log file found on disk.")
}

# 4. Read and display log entries
print("--- [ Reading Log Entries ] ---")
let reader = open(log_path, "r")
if (reader) {
    let entry_num = 1
    let entry = read_line(reader)
    
    while (entry != null) {
        print("Entry", entry_num, ":", entry)
        entry_num = entry_num + 1
        entry = read_line(reader)
    }
    
    close(reader)
}

# 5. Final cleanup
print("\n>> Cleaning up mission data...")
let success = remove_file(log_path)
if (success) {
    print("--- [ MISSION COMPLETE ] ---")
} else {
    print(">> Error: Could not remove log.")
}
```

---

## Best Practices

1. **Always check file handles** - Use `if (file)` to verify successful opening
2. **Close files after use** - Prevents resource leaks and ensures data is written
3. **Use `flush()` for critical writes** - Guarantees data is written to disk immediately
4. **Check existence before deletion** - Use `file_exists()` to avoid errors
5. **Handle read_line() EOF** - Check for `null` to detect end of file
6. **Use escape sequences** - `\n` for newlines, `\t` for tabs, `\"` for quotes

---

## Error Handling

* `open()` returns `null` if the file cannot be opened
* `remove_file()` returns `true` on success, `false` on failure
* `read_line()` returns `null` when reaching end of file

---

**Note:**

* File handles are opaque, runtime-managed values and must not be reused after being closed.
* `read()` loads the entire file into memory; for large files, prefer `read_line()` to avoid high memory usage.
* The File I/O library currently targets Linux-based systems using native system bi
