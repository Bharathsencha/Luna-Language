# Modules

A module is any `.lu` file. It marks its public names with `export`; other files pull them in
with `use`.

```javascript
# math_mod.lu
export let BASE = 10
export func scale(x) {
    return x * BASE
}
```

```javascript
# main.lu
use { BASE, scale } from "math_mod.lu"
print(scale(4))     # 40
```

## Rules

- **Paths are relative to the importing file**, not the working directory. A module in
  `sub/wrapper.lu` that writes `use { x } from "../helper.lu"` finds `helper.lu` next to `sub/`.
- **Each file runs once.** The first `use` executes the module and caches its environment.
  Later imports, from any file, reuse the cached bindings. Module-level state therefore
  persists: an exported counter function keeps counting across every importer.
- **Only exported names are visible.** Importing a name the module does not export is an
  error (`test/test_import_private_error.lu` checks this).
- **Circular imports are reported** as an error, instead of recursing.
- `import "util.lu"` (legacy form) runs a file and brings in all of its top-level names.

Resolution, the cache and cycle detection live in `src/module_runtime.c`. On the VM each
module's chunk runs in a nested VM (see [internals/vm.md](../internals/vm.md)).

## State shared between modules

Exported constants and functions are safe to share. For state that several modules change,
keep the data in a list owned by the main script and pass it to module functions. That is the
pattern `docs/examples/fun/racer3d/` uses (see its [page](../examples/fun/racer3d.md)).

## Tests

- `test/test_modules.lu`: constants, cross-module calls, persistent state through a wrapper
- `test/test_qol.lu`: `import` and `use` together
- `docs/examples/fun/modtest/`: script-relative resolution from a different working directory
