# Known Issues

Bugs found while writing these docs (2026-10-16). None are fixed yet. They are listed roughly
by impact, so they can be fixed one at a time.

## Bytecode VM

| # | Issue | Repro |
|---|---|---|
| 1 | `continue` inside a C-style `for (init; cond; step)` loop skips the step, so the loop never advances | `for (let k = 0; k < 3; k++) { if (k == 1) { continue } }` hangs |
| 2 | Recursion depth is capped at 64 frames (`FRAMES_MAX`), and the error has no line number: `VM Error: stack overflow` | A recursive function 200 levels deep. The interpreter handles 5000 |
| 3 | Allocation never triggers a collection; only safepoints at loops and every 16 statements do. Recursion-heavy code never collects | `binary_trees` grows to 273 MB with 0 GC events |
| 4 | Unsafe rules R5, R9 and R11 are not enforced | `address_of(1 + 2)` gives a Name Error; `left < right` across allocations and user calls inside `unsafe` pass |
| 5 | The aliases `deref`, `ptr_add` and `addr` are undefined | `load` / `ptr_offset` / `address_of` work |
| 6 | `print(a, b)` prints one value per line; the interpreter prints them space-separated on one line | `print(1, 2)` |

## Examples

| # | Issue | Where |
|---|---|---|
| 7 | `set_fog` is called as `set_fog(mode, color, density)`, but the API is `set_fog(color, density)`, so the fog does not apply | `docs/examples/fun/orbrush.lu`, `docs/examples/fun/racer3d/main.lu` |
| 8 | `draw_sphere` is called as `(center, r, rings, slices, color)`, but the API is `(center, r, color, rings, slices)`, so the color argument is ignored | `3D/main.lu` |

## Smaller items

| # | Issue |
|---|---|
| 9 | `hsl(h, s, v)` performs an HSV conversion despite its name |
| 10 | The R3 error message says `ptr_add()` even though the function is `ptr_offset()` |
| 11 | `bloc` / `box` size errors report `line 0` |
| 12 | `make` prints existing warnings in `src/env.c`, `src/value.c`, `src/library.c` (missing `<string.h>` for `strcspn`), `src/interpreter.c`, `src/gc.c`, `src/luna_vm.c` and `src/luna_vm_gc.c` |
| 13 | `musicplayer/assets/vinyl.png` and `neon.jpg`, and three tracks in `fun/music/`, are not used by any program |
