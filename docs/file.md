
# File functions in Luna
This section describes the native File I/O system implemented in **Luna**. These functions bridge Luna scripts with the Linux Mint filesystem through native bindings.

---

## File Function Reference

| Function Name | Description                                                                                    | Syntax                |
| ------------- | ---------------------------------------------------------------------------------------------- | --------------------- |
| `open`        | Opens a file stream in a specific mode (`"r"`, `"w"`, `"a"`). Returns a file handle or `null`. | `open(path, mode)`    |
| `close`       | Flushes and closes an open file handle.                                                        | `close(handle)`       |
| `write`       | Writes a string to the file. Supports escape sequences (`\n`, `\t`).                           | `write(handle, text)` |
| `read`        | Reads the entire content of a file into a single string.                                       | `read(handle)`        |
| `read_line`   | Reads the next line from a file (strips the trailing `\n`).                                    | `read_line(handle)`   |
| `file_exists` | Returns `true` if the file exists on disk, otherwise `false`.                                  | `file_exists(path)`   |
| `remove_file` | Deletes the specified file from disk.                                                          | `remove_file(path)`   |
| `flush`       | Force-writes buffered data to disk without closing the file.                                   | `flush(handle)`       |

---

## Function Samples

### 1. Opening and Closing

```luna
let file = open("data.txt", "w")
if (file) {
    print("Handle secured.")
    close(file)
}
```

---

### 2. Writing Data (with Escapes)

```luna
let writer = open("notes.txt", "w")
write(writer, "Header\n\tSub-item\nDone.")
close(writer)
```

---

### 3. Reading Entire Content

```luna
let reader = open("notes.txt", "r")
let all_text = read(reader)
print(all_text)
close(reader)
```

---

### 4. Existence and Deletion

```luna
if (file_exists("old.txt")) {
    remove_file("old.txt")
    print("Cleared space.")
}
```

---

## Sample codes
```swift
let filename = "test.txt"

# 1. Writing with our new newline support
print("Writing to file...")
let f_out = open(filename, "w")
if (f_out) {
    write(f_out, "First Line\n")
    write(f_out, "Second Line with \t tabs\n")
    write(f_out, "Third Line: \"Quotes inside!\"")
    close(f_out)
    print("Done writing.")
}

# 2. Checking if it exists
if (file_exists(filename)) {
    print("Verification: File exists on disk.")
}

# 3. Reading line by line (Testing the \n fix)
print("\nReading line by line:")
let f_in = open(filename, "r")
if (f_in) {
    let l1 = read_line(f_in)
    let l2 = read_line(f_in)
    let l3 = read_line(f_in)
    
    print("L1: " + l1)
    print("L2: " + l2)
    print("L3: " + l3)
    
    # Testing our new polymorphic len()
    print("Length of L1 is: " + len(l1))
    
    close(f_in)
}

# 4. Cleanup
print("\nCleaning up...")
remove_file(filename)
print("File removed. Test Complete.")
```
## Example 2:
  
```javascript
print("=== Luna Config Manager ===")

let path = "app.cfg"

# 1. Start Fresh
if (file_exists(path)) {
    remove_file(path)
}

# 2. Write Configuration
let cfg = open(path, "w")
if (!cfg) {
    print("[error] Cannot create config")
} else {
    write(cfg, "mode=production")
    write(cfg, "threads=8")
    write(cfg, "logging=true")
    flush(cfg)
    close(cfg)
}

# 3. Reload and Parse
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
            print("[cfg] " + line)
        }

        count = count + 1
        line = read_line(reader)
    }

    close(reader)
    print("[ok] Loaded " + count + " entries")
}

# 4. Safe Removal
if (remove_file(path)) {
    print("[done] Config cleaned up")
} else {
    print("[error] Cleanup failed")
}

print("=== End ===")

```


##  *Mission Control*


```javascript
# mission_control.lu 
print("--- [ Luna Mission Control: Data Sync ] ---")

let log_path = "system_log.lu"

# 1. Prepare Environment
if (file_exists(log_path)) {
    remove_file(log_path)
    print(">> Previous log cleared.")
}

# 2. Writing Complex Strings (Testing 
, 	, and \"")
print(">> Initializing Log Write...")
let stream = open(log_path, "w")

if (stream) {
    # Testing the 
 fix: These will be stored as real line breaks
    write(stream, "STATUS: Operational")
    write(stream, "METRICS: - Internal Temp: 32C - Pressure: Stable")
    write(stream, "MESSAGE: \"All systems go!\"")
    
    close(stream)
    print(">> Write Successful.")
}

# 3. Verification & Metrics
if (file_exists(log_path)) {
    print(">> Verification: 'system_log.lu' found on disk.")
}

# 4. Reading Line-by-Line (Testing Line Separation)
print("--- [ Reading Log Entries ] ---")
let reader = open(log_path, "r")

if (reader) {
    let entry1 = read_line(reader)
    let entry2 = read_line(reader)
    let entry3 = read_line(reader)
    let entry4 = read_line(reader)
    
    print("Entry 1: " + entry1)
    print("Entry 2: " + entry2)
    print("Entry 3: " + entry3)
    print("Entry 4: " + entry4)
    
    # 5. Testing Polymorphic len() on Strings
    # This will return 19 (length of "STATUS: Operational") because 
 is stripped
    print(">> Metadata: First entry length is " + len(entry1) + " characters.")
    
    close(reader)
}

# 6. Final Cleanup
print("
>> Cleaning up mission data...")
let success = remove_file(log_path)

if (success) {
    print("--- [ MISSION COMPLETE ] ---")
} else {
    print(">> Error: Could not remove log.")
}
```
