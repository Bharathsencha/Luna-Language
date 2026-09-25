<p align="center">
  <img src="assets/luna.png" alt="Luna Logo" width="140">
</p>

<h1 align="center">Luna</h1>

<p align="center">
  A high-performance scripting language built in C — JavaScript-like syntax, bare-metal speed.
</p>

---

Luna is a scripting language built from scratch in C. It gives you expressive, JavaScript-style syntax while pushing performance through SIMD vectorization, memory arenas, and a tracing GC with sub-millisecond max pauses. It ships its own 2D/3D OpenGL rendering backend and audio layer, making it a complete platform for creative coding and high-speed scripting.

---

## Luna

Most scripting languages make you choose between ease of use and performance. Luna doesn't.

- Write clean, readable code with a familiar JS-inspired syntax
- Get C and NumPy-level throughput on heavy workloads through native SIMD bridges
- Ship interactive 2D/3D applications with the built-in OpenGL 3.3 + miniaudio backend
- Trust the GC — current benchmarks show sub-ms max pause across all shipped workloads
- Drop into `unsafe` blocks for manual memory control when you need it

---

## Feature Highlights

- **Bare-metal performance** — SIMD-accelerated `vec_mul` / `mat_mul` match or beat NumPy. The parser uses a bump-allocated memory arena for O(1) AST allocation, constant folding at parse time, O(1) variable lookups via a djb2 hash table, string interning for pointer-fast name comparisons, and tail-call optimization for self-recursive functions.

- **Sub-ms garbage collector** — incremental tri-color tracing GC with SATB write barriers, young/old generation tracking, remembered-set support, and deadline-bounded incremental marking/sweeping. Worst-case pause is **~0.1–0.15ms** across all shipped benchmark workloads (Go's GC measures 0.03–0.06ms on the same tests). AST and `unsafe` memory are intentionally kept outside the collector's domain.

- **2D/3D graphics and audio** — custom OpenGL 3.3 Core Profile renderer on statically linked GLFW with a 65,536-vertex batch renderer, full 2D shape primitives, FBO render-to-texture, `stb_truetype` font atlas, and Blinn-Phong lit 3D scenes (cameras, lights, meshes, grids). Audio via miniaudio with streaming, one-shot SFX, and real-time FFT. Zero runtime graphics dependencies beyond system OpenGL and X11.

- **Familiar syntax, powerful features** — JavaScript-inspired `let` / `func` / `for` / `switch` with closures, first-class functions, string interpolation (`"HP: {hp + bonus}"`), higher-order `map()` / `filter()` / `reduce()`, named module exports (`use {greet} from "utils.lu"`), and `data` constructors for tagged structured values.

- **Three-tier structured data** — `bloc` for tiny immutable inline values, `box` for manual non-GC heap buffers, `template` for schema-backed GC-managed objects. All three are validated by a Rust static runtime (`libluna_data_rt.a`).

- **Manual memory when you need it** — `unsafe` blocks expose `alloc`, `store`, `load`, `ptr_offset`, `address_of`, and `defer` with 12 enforced safety rules (no use-after-free, no pointer escape, no GC container contamination, etc.) backed by a Rust rule engine. Scope-based `defer(close(file))` is available in normal code too.

---

## Setup

```bash
git clone https://github.com/Bharathsencha/Luna-Language
cd Luna-Language
```

**Dependencies (Ubuntu/Debian):**

```bash
sudo apt install gcc build-essential git cmake nasm libx11-dev libgl1-mesa-dev
```

**Build:**

```bash
make
```

**Run:**

```bash
./bin/luna your_script.lu
```

All graphics dependencies (GLFW, stb_image, stb_truetype, miniaudio) are vendored — no extra installs needed.

---

## Quick Start

### Hello World

```javascript
print("Hello, Luna!")

let name = input("What's your name? ")
let greeting = "Welcome to Luna, {name}!"
print(greeting)
```

### Variables, Types, and String Interpolation

Luna infers types from assigned values. String literals can embed any Luna expression inside `{ }`.

```javascript
let x     = 10        # int
let y     = 1.5       # float
let name  = "Luna"    # string
let ok    = true      # boolean

let score = 42
let msg   = "Player {name} scored {score} — next: {score + 1}"
print(msg)
# Player Luna scored 42 — next: 43
```

### Functions and Recursion

```javascript
func factorial(n) {
    if (n <= 1) { return 1 }
    return n * factorial(n - 1)
}

func greet(name) {
    return "Hello, {name}!"
}

print(factorial(6))       # 720
print(greet("Astra"))     # Hello, Astra!
```

### Lists, Maps, and Higher-Order Functions

```javascript
let numbers = [1, 2, 3, 4, 5]

let doubled = map(numbers, func(x) { return x * 2 })
let evens   = filter(doubled, func(x) { return x % 4 == 0 })
let total   = reduce(evens, func(acc, x) { return acc + x }, 0)

print(doubled)  # [2, 4, 6, 8, 10]
print(evens)    # [4, 8]
print(total)    # 12

# Maps — string-keyed hash tables
let player = {"name": "Astra", "hp": 100, "zone": "forest"}
map_set(player, "hp", map_get(player, "hp") + 25)
print("Keys:", map_keys(player))
```

### Modules

```javascript
# utils.lu
export func greet(name) {
    return "Hello, {name}!"
}
```

```javascript
# main.lu
use {greet} from "utils.lu"
print(greet("Luna"))
```

### Data Constructors

```javascript
data Vec2   { x, y }
data Entity { name, hp, pos }

let origin = Vec2(0, 0)
let hero   = Entity("Astra", 100, Vec2(3, 4))

print(shape(hero))              # Entity
print("pos = ({hero["pos"]["x"]}, {hero["pos"]["y"]})")
```

### File I/O with Scope Cleanup

```javascript
func save_log(path, text) {
    let file = open(path, "w")
    defer(close(file))          # runs automatically when scope exits
    write(file, text)
}

save_log("run.log", "started\n")

let f = open("run.log", "r")
print(read(f))
close(f)
```

### 2D Window with OpenGL

```javascript
init_window(800, 600, "Luna 2D")
set_fps(60)

let t = 0.0

while (window_open()) {
    begin_drawing()
    clear_background(rgb(20, 20, 30))

    let cx = 400 + int(cos(t) * 150)
    let cy = 300 + int(sin(t) * 100)
    draw_circle(cx, cy, 40, rgb(100, 200, 255))
    draw_rectangle(50, 50, 200, 80, rgb(255, 100, 80))

    t = t + get_delta_time()
    end_drawing()
}

close_window()
```

### 3D Scene with Lighting

```javascript
init_window(1280, 720, "Luna 3D")
set_fps(60)

let cam = create_camera_3d([0, 5, -10], [0, 0, 0], [0, 1, 0], 45.0)
let sun = create_light(0, [10, 20, 10], [0, 0, 0], rgb(255, 250, 240))
set_ambient_light([30, 30, 50])

let angle = 0.0

while (window_open()) {
    angle = angle + get_delta_time()
    let px = cos(angle) * 3
    let pz = sin(angle) * 3

    begin_drawing()
    clear_background(rgb(15, 15, 25))

    begin_mode_3d(cam)
        draw_cube([px, 0.5, pz], [1, 1, 1], rgb(100, 180, 255))
        draw_sphere([0, 0.5, 0], 0.5, 12, 12, rgb(255, 160, 80))
        draw_plane([0, 0, 0], [10, 10], rgb(40, 40, 60))
        draw_grid(10, 1)
    end_mode_3d()

    end_drawing()
}

close_window()
```

### Manual Memory with `unsafe`

```javascript
unsafe {
    let buf = alloc(4)
    defer(buf)                          # freed automatically at block exit

    for (let i = 0; i < 4; i++) {
        store(ptr_offset(buf, i), i * i)
    }

    for (let i = 0; i < 4; i++) {
        print("slot", i, "=", load(ptr_offset(buf, i)))
    }
}
# slot 0 = 0 | slot 1 = 1 | slot 2 = 4 | slot 3 = 9
```

---

## Language Reference

| Feature | Syntax |
|---|---|
| Variable | `let x = 10` |
| Function | `func add(a, b) { return a + b }` |
| Closure | `func(x) { return x * 2 }` |
| Control flow | `if` / `else` / `while` / `for` / `switch` |
| List | `let arr = [1, 2, 3]` |
| Map | `let cfg = {"width": 1280}` |
| String interpolation | `"Score: {score + bonus}"` |
| Data constructor | `data Vec2 { x, y }` |
| Module | `use {greet} from "utils.lu"` |
| Dense array | `dense_list(1000000, 0.0)` |
| SIMD multiply | `vec_mul(A, B)` |
| Matrix multiply | `mat_mul(A, B)` |
| Scope cleanup | `defer(close(file))` |

---

## Architecture

Luna compiles to bytecode and runs on a register-based VM:

- **Lexer → Parser → AST** — recursive-descent parser with Pratt-style expression precedence and a constant-folding pass at parse time
- **Bytecode compiler + VM** — the AST compiles to a custom 50+ opcode bytecode executed with computed-goto dispatch; includes opcode fusion (`repeat(...) + to_string(i)` builds in a single allocation), per-chunk interned-name caching, closures with upvalue capture, module imports via nested VMs, and scoped `defer`
- **Arena** — slab allocator for all AST nodes; single `arena_reset()` tears down the parse tree instantly
- **GC** — incremental tracing collector; strings, lists, maps, closures, and dense arrays are GC-managed
- **Unsafe runtime** — Rust static library (`libluna_memory_rt.a`) that enforces 12 pointer-safety rules before the C side performs any memory operation
- **Data runtime** — Rust static library (`libluna_data_rt.a`) implementing kind tags, bloc layout validation, fixed-size box buffers, and template schema descriptors
- **GUI layer** — OpenGL 3.3 batch renderer + miniaudio, bridged to Luna scripts through `gui_lib.c`

---

## Testing

```bash
make test          # Luna script tests with assert() and golden output
make zig-test      # Host-side Zig tests: lexer, parser, AST shape, error line/column
make test-gc       # GC benchmark suite with pause profiling
make test-gc-three # Go vs Luna GC benchmark, 3-run averages (stress_test/stress_results.csv)
make test-py       # Python vs Luna GC benchmark, 3-run averages (python/python_vs_luna.csv)
```

Luna has two complementary test layers. The Luna script tests verify observable language behavior. The Zig suite calls internal C APIs directly to pinpoint exactly which subsystem regressed and on which line.

## GC vs Go vs Java (3-run averages)

Classic workloads (same shapes in all three languages; Java on G1GC @ 256MB):

| Benchmark | Luna user | Java user | Go user | Luna GC max | Java GC max | Go GC max |
|---|---|---|---|---|---|---|
| alloc_heavy | 0.113s | 0.034s | 0.227s | **0.091ms** | 6.573ms | 0.033ms |
| long_live | 0.070s | 0.021s | 0.110s | **0.104ms** | 1.880ms | 0.057ms |
| cycles | 0.010s | 0.003s | 0.000s | **0.103ms** | 0.000ms | 0.000ms |
| strings | 0.090s | 0.034s | 0.163s | **0.081ms** | 6.506ms | 0.037ms |

New GC-shape workloads (Java vs Luna; run with `make test-java-gc`):

| Benchmark | Java user | Luna user | Java GC max | Luna GC max |
|---|---|---|---|---|
| binary_trees | 0.048s | 0.530s | 6.950ms | 0.000ms* |
| map_churn | 0.185s | 0.933s | 2.855ms | **0.248ms** |
| object_graph | 0.029s | 0.093s | 0.000ms | 2.190ms |
| concurrent_map | 0.039s | 0.257s | 0.636ms | **0.103ms** |

**Reading it:** where Luna's collector actually runs, its worst pause is **0.08–0.25ms** — better than Java G1 (1.9–6.9ms) and in Go's range (0.03–0.06ms). Java/Go win on raw user time (JIT/AOT vs bytecode VM) and collect far less often (big adaptive young gens vs Luna's fixed 1–8MB).

\* `binary_trees` Luna shows no GC events: recursion-heavy code doesn't reach the safepoint counter — a known limitation (allocation-driven triggering has a liveness bug under investigation).

Python vs Luna (3-run averages; run with `make test-py`, CSV in `python/python_vs_luna.csv`):

| Benchmark | Luna user | Python user | Luna GC max | Python GC max |
|---|---|---|---|---|
| alloc_heavy | 0.290s | 0.227s | **0.153ms** | 0.000ms\*\* |
| long_live | 0.150s | 0.167s | **0.120ms** | 0.000ms\*\* |
| cycles | **0.030s** | 0.083s | **0.101ms** | 1.300ms |
| strings | 0.253s | 0.170s | **0.129ms** | 0.000ms\*\* |
| binary_trees | 0.753s | 0.710s | 0.000ms\* | **18.285ms** |
| map_churn | **1.020s** | 1.067s | **0.518ms** | 0.000ms\*\* |
| object_graph | **0.093s** | 0.137s | **0.196ms** | 5.888ms |
| concurrent_map | **0.280s** | 0.290s | **0.271ms** | 0.000ms\*\* |

\*\* CPython frees most objects by reference counting — instant, no stop-the-world pause — so the cost shows up in user time instead. Where CPython's cyclic GC does run (`cycles`, `object_graph`, `binary_trees`), Luna is 2.8x/1.5x faster and its worst pause is 0.10–0.52ms vs Python's **1.3–18.3ms** (up to ~100x better).

Regenerate: `make test-gc-three` (Go) · `make test-java-gc` (Java) · `make test-py` (Python).

---

## Documentation

| Doc | What it covers |
|---|---|
| [docs/arch.md](docs/arch.md) | Full source file inventory and architectural decisions |
| [docs/gc.md](docs/gc.md) | Collector model, SATB barrier, benchmark suite, pause profile |
| [docs/arena.md](docs/arena.md) | Memory arena design and cache-locality benefits |
| [docs/memory.md](docs/memory.md) | `unsafe` block API and all 12 enforced safety rules |
| [docs/data_types.md](docs/data_types.md) | `bloc`, `box`, and `template` reference |
| [docs/gui.md](docs/gui.md) | Full 2D/3D GUI and audio API reference |
| [docs/benchmarks.md](docs/benchmarks.md) | Speed comparisons vs Python, NumPy, C, and Go |
| [docs/installation.md](docs/installation.md) | Build dependencies and setup guide |
| [docs/string.md](docs/string.md) | String library reference |
| [docs/math.md](docs/math.md) | Math, SIMD, and random number library reference |
| [docs/file.md](docs/file.md) | File I/O library reference |
| [rust/readme.md](rust/readme.md) | Rust runtime crates, FFI surface, and build layout |
| [zig.md](docs/zig.md) | Zig test suite design and usage |

---

## Legal

All songs and assets used in the demonstration projects belong to their respective artists. See [musicplayer/CREDITS.md](musicplayer/CREDITS.md) for a full list.

---

## License

GNU General Public License v3.0 or later (GPLv3+). See [LICENSE](LICENSE).
