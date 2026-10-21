# Modules

A module is any `.lu` file. It marks its public names with `export`. Everything else stays
private to the file.

```javascript
# math_mod.lu
export let BASE = 10
export func scale(x) {
    return x * BASE
}
```

Bring in the whole file:

```javascript
# main.lu
use "math_mod.lu"
print(scale(4))     # 40
print(BASE)         # 10
```

Or pick only the names you need:

```javascript
use { scale } from "math_mod.lu"
print(scale(4))     # 40
```

Either way, only exported names are visible. A name the module does not export is undefined
in the importing file, even with `use "file.lu"`.

## Rules

- **Paths are relative to the importing file**, not the working directory. A module in
  `sub/wrapper.lu` that writes `use { x } from "../helper.lu"` finds `helper.lu` next to `sub/`.
- **Modules are part of the binary.** The compiler reads every imported file while it compiles
  the program, and compiles it into the same executable. The `.lu` files are not read again at
  run time, and a program built from `main.lu` runs without the sources next to it. A file
  that cannot be read at compile time is reported when the `use` statement runs, not when the
  program is compiled: `Could not import file 'nope.lu'`.
- **Each file runs once.** The first `use` executes the module's top-level statements. Later
  imports, from any file, reuse the same bindings. Module-level state therefore persists: an
  exported counter function keeps counting across every importer.
- **Only exported names are visible.** Asking for a name the module does not export with
  `use { name } from` is an error: `Module 'helper.lu' does not export 'hidden'`. Using a name
  that was never imported is `Variable 'name' is not defined`.
- **Circular imports are reported** as an error (`Circular import detected: 'a.lu' is already
  loading`) instead of recursing.
- `import "util.lu"` is an older spelling of `use "util.lu"`: it brings in the exported names
  only.
- An imported name is a copy of the module's binding made when `use` runs. If the module
  later reassigns an exported variable, the importer's copy does not change. Export a function
  that reads or updates the state instead (see below).

## State shared between modules

Exported constants and functions are safe to share. A module's own variables stay in the
module, and its exported functions see and change them:

```javascript
# counter.lu
let n = 0
export func bump() {
    n += 1
    return n
}
```

For state that several modules change, keep the data in a list or map owned by one module
and pass it to the functions that need it.

## Tests

- `test/test_modules.lu`: constants, cross-module calls, persistent state through a wrapper
- `test/test_use_file.lu`: `use "file.lu"` and `use { name } from` on the same module
- `test/test_use_private_error.lu`: a non-exported name is undefined after `use "file.lu"`
- `test/test_import_private_error.lu`: the same for `import`
- `test/test_qol.lu`: `import` and `use` together
