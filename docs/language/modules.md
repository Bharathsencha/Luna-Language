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
- **Each file runs once.** The first `use` executes the module and caches its environment.
  Later imports, from any file, reuse the cached bindings. Module-level state therefore
  persists: an exported counter function keeps counting across every importer.
- **Only exported names are visible.** Using a name the module does not export is an error,
  and so is asking for one with `use { name } from`.
- **Circular imports are reported** as an error, instead of recursing.
- `import "util.lu"` is an older spelling of `use "util.lu"`: it brings in the exported names
  only.

Resolution, the cache and cycle detection live in `src/module_runtime.c`. On the VM each
module's chunk runs in a nested VM (see [internals/vm.md](../internals/vm.md)).

## State shared between modules

Exported constants and functions are safe to share. For state that several modules change,
keep the data in a list owned by the main script and pass it to module functions. That is the
pattern `docs/examples/fun/racer3d/` uses (see its [page](../examples/fun/racer3d.md)).

## Tests

- `test/test_modules.lu`: constants, cross-module calls, persistent state through a wrapper
- `test/test_use_file.lu`: `use "file.lu"` and `use { name } from` on the same module
- `test/test_use_private_error.lu`: a non-exported name is undefined after `use "file.lu"`
- `test/test_import_private_error.lu`: the same for `import`
- `test/test_qol.lu`: `import` and `use` together
- `docs/examples/fun/modtest/`: script-relative resolution from a different working directory
