# Falling Sand (`sand.lu`)

## Why We Made sand.lu

This was built to demonstrate that Luna can interface with native C plugins — not just the GUI layer. The falling sand simulation (where particles of sand, water, acid, stone, and fire interact physically) requires updating tens of thousands of cells per frame. Doing this in pure Luna (a bytecode-interpreted language) would be too slow. So the physics engine was written as a native C plugin (`sand_lib.c`) compiled into Luna, and `sand.lu` is the Luna-side front-end that handles input, rendering, and UI.

## The Grid

```javascript
let GRID_W = 200
let GRID_H = 150
let CELL_SIZE = 4
```
The simulation runs on a 200×150 grid. Each cell is 4×4 pixels on screen, giving a 800×600 display. The grid is managed entirely in C (inside `sand_init`, `sand_set`, `sand_get`, `sand_update`).

## sand_init(), sand_set(), sand_get(), sand_update()

These are **native C functions** registered into Luna's environment. From Luna's perspective they look like any other built-in function, but they actually call compiled C code directly:
- `sand_init()` — allocates the grid in C memory and zeros it out
- `sand_set(x, y, type)` — sets a cell to a particle type (0=empty, 1=sand, 2=water, 3=stone, 4=acid, 5=fire)
- `sand_get(x, y)` — returns the type of a cell
- `sand_update()` — runs one full physics step across the entire grid

## The Physics Logic (in C, called from Luna)

**Sand:** Falls straight down if the cell below is empty. If directly below is blocked, tries to fall diagonally (down-left or down-right). This gives sand its characteristic pile-up behavior.

**Water:** Falls down if possible. If blocked below, spreads sideways left or right — simulating liquid flowing outward. Water has a chance to spread multiple cells in one step, making it flow faster than sand falls.

**Stone:** Does not move at all. It's a permanent solid barrier.

**Acid:** Falls and spreads like water, but additionally has a chance each frame to *destroy* any adjacent non-empty, non-acid cell it touches — simulating chemical dissolution.

**Fire:** Rises upward (opposite of sand/water — fire has negative effective gravity). Randomly spreads to adjacent empty cells. Has a lifespan and disappears after some ticks. Can ignite other materials.

## Mouse Drawing

```javascript
let r = brush_size
let by = -r
while (by <= r) {
    let bx = -r
    while (bx <= r) {
        if (rand(100) < 50) { sand_set(gx + bx, gy + by, current_type) }
```

A square brush of radius `brush_size` is applied around the mouse grid cell. The `rand(100) < 50` check means only ~50% of cells in the brush area get set per frame. This creates a **dithered spray** effect rather than a solid block, making it feel more like pouring material than stamping it.

Right-click erases by setting cells to `TYPE_EMPTY` without the 50% check — erasing is deterministic.

## Rendering

```javascript
let type = sand_get(dx, dy)
if (type != 0) {
    draw_rectangle(dx * CELL_SIZE, dy * CELL_SIZE, CELL_SIZE, CELL_SIZE, col)
}
```

Only non-empty cells are drawn — empty cells are just the dark background. Each cell is a 4×4 rectangle colored by type. The grid is iterated in row-major order (outer loop = rows, inner = columns) to minimize cache misses.

---

[Back to fun/ examples](README.md)
