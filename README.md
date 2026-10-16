<p align="center">
  <img src="assets/luna.png" alt="Luna Logo" width="140">
</p>

<h1 align="center">Luna</h1>

<p align="center">
  A scripting language built from scratch in C. JavaScript-like syntax, a bytecode VM, a
  sub-millisecond garbage collector, and its own 2D/3D renderer and audio engine.
</p>

---

## What Luna is

Luna is a dynamically typed scripting language. The core is about 20k lines of C11, with two
Rust static libraries for memory-safety rules. A script is lexed, parsed into an arena-allocated
AST, compiled to register bytecode, and run on a VM with computed-goto dispatch.

- **Familiar syntax:** `let`, `func`, closures, `for-in`, `switch`, default parameters, string
  interpolation (`"HP: {hp + bonus}"`), modules with `use { name } from "file.lu"`.
- **Sub-millisecond GC:** an incremental, generational, tri-color tracing collector with SATB
  write barriers. The worst recorded pause is 0.10-0.65 ms, against Go at 0.03-0.05 ms, Java G1
  at up to 12 ms, and CPython's cyclic GC at up to 18 ms.
  [Results](docs/results/gc-results.md).
- **Native speed where it counts:** `dense_list` stores raw floats contiguously; `vec_mul`
  matches `-O3` C, and the OpenMP `mat_mul` beats NumPy on 300x300 matrices.
  [Benchmarks](docs/results/benchmarks.md).
- **Graphics and audio built in:** an OpenGL 3.3 batch renderer, a lit 3D pipeline (lights,
  fog, OBJ models, transform stack, picking), and miniaudio streaming with a live FFT. GLFW is
  linked statically, so the only runtime dependencies are OpenGL and X11.
- **Structured data:** `data` constructors, plus `bloc` (small inline values), `box` (manual
  scoped buffers) and `template` (GC-managed schema objects), validated by a Rust runtime.
- **Manual memory when you need it:** `unsafe { }` blocks with `alloc`, `load`, `store` and
  `ptr_offset`, guarded by 12 safety rules (no use-after-free, no pointer escape, and so on).

## Build

Linux, x86-64. Install the toolchain (Ubuntu, Debian or Mint shown):

```bash
sudo apt install build-essential nasm libx11-dev libgl1-mesa-dev
curl https://sh.rustup.rs -sSf | sh       # Rust, if cargo is missing

git clone https://github.com/Bharathsencha/Luna-Language
cd Luna-Language
make check-deps                           # reports anything missing
make                                      # builds rust/ crates, then bin/luna
make test                                 # should end with "All tests passed!"
```

GLFW, stb, miniaudio and tinyobj are vendored, so there is nothing else to install. More in
[docs/getting-started.md](docs/getting-started.md).

## Use

```bash
./bin/luna hello.lu     # run a script
./bin/luna              # REPL
```

```javascript
# hello.lu
let name = input("What's your name? ")
print("Welcome to Luna, {name}!")
```

## Syntax tour

```javascript
# Variables and types
let x = 10              # int
let y = 1.5             # float
let ok = true
const VERSION = "0.1"
x += 5
x++

# Strings interpolate any expression
let score = 42
print("score {score}, next {score + 1}")

# Functions, default parameters, closures
func greet(name, greeting = "Hello") {
    return "{greeting}, {name}!"
}

func make_counter() {
    let count = 0
    return func() {
        count += 1
        return count
    }
}
let next = make_counter()
print(next())           # 1

# Lists, maps, higher-order functions
let nums = [1, 2, 3, 4, 5]
let doubled = map(nums, func(n) { return n * 2 })
let evens = filter(doubled, func(n) { return n % 4 == 0 })
let total = reduce(evens, func(acc, n) { return acc + n }, 0)
print(total)            # 12

let player = {"name": "Astra", "hp": 100}
map_set(player, "hp", player.hp + 25)

# Control flow
for (let i in range(3)) {
    print(i)
}

switch (player.hp) {
    case 125:
        print("healed")
        break
    default:
        print("unchanged")
}

# Data constructors
data Vec2 { x, y }
let p = Vec2(3, 4)
print("{shape(p)} ({p.x}, {p.y})")    # Vec2 (3, 4)
```

Modules:

```javascript
# utils.lu
export func greet(name) {
    return "Hello, {name}!"
}

# main.lu
use { greet } from "utils.lu"
print(greet("Luna"))
```

Scope cleanup and manual memory:

```javascript
func save_log(path, text) {
    let file = open(path, "w")
    defer(close(file))          # runs when the function returns
    write(file, text)
}

unsafe {
    let buf = alloc(4)
    defer(buf)                  # freed when the block exits
    for (let i in range(4)) {
        store(ptr_offset(buf, i), i * i)
    }
    print("{load(ptr_offset(buf, 3))}")    # 9
}
```

The full language is in [docs/language/syntax.md](docs/language/syntax.md).

## Graphics

```javascript
init_window(800, 600, "Luna 2D")
set_fps(60)
let t = 0.0

while (window_open()) {
    t += get_delta_time()
    begin_drawing()
    clear_background(rgb(20, 20, 30))
    draw_circle(400 + cos(t) * 150, 300 + sin(t) * 100, 40, rgb(100, 200, 255))
    end_drawing()
}
close_window()
```

```javascript
init_window(1280, 720, "Luna 3D")
let cam = create_camera_3d([0, 5, -10], [0, 0, 0], [0, 1, 0], 45.0)
create_light(LIGHT_DIRECTIONAL, [10, 20, 10], [0, 0, 0], rgb(255, 250, 240))
set_ambient_light([30, 30, 50])

while (window_open()) {
    begin_drawing()
    clear_background(rgb(15, 15, 25))
    begin_mode_3d(cam)
        draw_cube([0, 0.5, 0], [1, 1, 1], rgb(100, 180, 255))
        draw_sphere([2, 0.5, 0], 0.5, rgb(255, 160, 80))
        draw_grid(10, 1)
    end_mode_3d()
    end_drawing()
}
close_window()
```

The full API (2D, 3D, input, audio) is in [docs/stdlib/gui.md](docs/stdlib/gui.md).

## Example programs

```bash
make run-cargame             # top-down car game with menus
make run-music               # music player: album art, seek bar, 32-band spectrum
make run-racer               # 3D seasonal racer across 8 modules
make run-fun NAME=galaxy     # any program in docs/examples/fun/
```

Every example has a page explaining how it works: [docs/examples/](docs/examples/README.md).

## GC results

Worst single pause per workload in milliseconds. These are 3-run averages; Luna runs on the VM.

| Workload | Luna | Go | Java G1 | CPython |
|---|---:|---:|---:|---:|
| alloc_heavy | 0.13-0.22 | 0.03 | 10.2 | 0 (refcount) |
| long_live | 0.12-0.16 | 0.03 | 6.0 | 0 (refcount) |
| cycles | 0.10-0.13 | no GC | no GC | 1.3 |
| strings | 0.10-0.13 | 0.05 | 8.4 | 0 (refcount) |
| map_churn | 0.52-0.65 | - | 3.5 | 0 (refcount) |
| object_graph | 0.20-0.26 | - | no GC | 5.9 |

Go pauses less and runs 1.2-1.3x faster on allocation loops. Java runs 3-4x faster with
millisecond pauses. Luna uses more memory than any of them. Throughput, RSS, stress runs,
method and the 4 ms to 0.1 ms history are in
[docs/results/gc-results.md](docs/results/gc-results.md).

## Documentation

Start at [docs/README.md](docs/README.md).

| Section | Pages |
|---|---|
| Start | [Getting started](docs/getting-started.md), [Syntax](docs/language/syntax.md), [Examples](docs/examples/README.md) |
| Language | [Syntax](docs/language/syntax.md), [Modules](docs/language/modules.md), [Data types](docs/language/data-types.md) |
| Standard library | [Core](docs/stdlib/core.md), [Strings](docs/stdlib/string.md), [Math](docs/stdlib/math.md), [Files](docs/stdlib/file.md), [Graphics and audio](docs/stdlib/gui.md) |
| Internals | [Architecture](docs/internals/architecture.md), [VM](docs/internals/vm.md), [GC](docs/internals/gc.md), [Arena](docs/internals/arena.md), [Unsafe](docs/internals/unsafe.md), [Performance](docs/internals/performance.md) |
| Results | [GC vs Go/Java/Python](docs/results/gc-results.md), [Native math](docs/results/benchmarks.md) |
| Project | [Testing](docs/testing.md), [Known issues](docs/known-issues.md), [Rust runtimes](rust/readme.md) |

## Project layout

```
src/ include/   C core: lexer, parser, compiler, VM, GC, standard library
gui/            OpenGL and audio backends, vendored single-header libraries
rust/           unsafe_rt and data_rt crates (built into lib/)
asm/            NASM sources
test/           Luna script tests        zig-test/   host-side Zig tests
docs/           documentation and example programs
3D/ assets/     standalone 3D scene, shared fonts and models
```

## Testing

```bash
make test            # Luna script tests + Rust crate tests
make test-gc-safety  # GC safety scripts under stress + heap verification
make zig-test        # Zig tests against the C API (needs zig)
```

See [docs/testing.md](docs/testing.md). Open bugs are tracked in
[docs/known-issues.md](docs/known-issues.md).

## License

Luna is licensed under the **GNU General Public License v3.0 or later**. See
[LICENSE](LICENSE).

## Credits

Third-party code vendored in this repository, under its own licenses:

| Library | Use | License |
|---|---|---|
| [GLFW 3.3.10](https://github.com/glfw/glfw) | Window, context, input (`lib/libglfw3.a`) | zlib/libpng |
| [stb_image, stb_truetype, stb_image_write](https://github.com/nothings/stb) | Images, fonts, screenshots | MIT or public domain |
| [miniaudio](https://github.com/mackron/miniaudio) | Audio playback and capture | MIT-0 or public domain |
| [tinyobjloader-c](https://github.com/syoyo/tinyobjloader-c) | OBJ / MTL parsing | MIT |

Media: all songs and third-party assets in the example programs belong to their artists and
are included for demonstration only. See
[docs/examples/musicplayer/CREDITS.md](docs/examples/musicplayer/CREDITS.md) and
[docs/examples/fun/README.md](docs/examples/fun/README.md#credits). Playback icons are by
[Icons8](https://icons8.com).
