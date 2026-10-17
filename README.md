<p align="center">
  <img src="assets/luna.png" alt="Luna Logo" width="140">
</p>

<h1 align="center">Luna</h1>

<p align="center">
  A small scripting language with a friendly syntax, a fast garbage collector,
  and graphics and sound built right in.
</p>

---

## About Luna

Luna is a scripting language written from scratch in C, with a syntax close to JavaScript.
Scripts are compiled to bytecode and run on a small virtual machine.

- **Smooth garbage collection.** Pauses are usually around 0.1 ms, so games and animations
  don't stutter.
- **Graphics and sound built in.** 2D drawing, 3D scenes, music and sound effects with
  nothing extra to install.
- **Fast math.** Vector and matrix operations run on native SIMD code.
- **Modules, closures and custom data types**, plus `unsafe` blocks for manual memory.

## Getting Started

### Install

Luna runs on Linux (x86-64). On Ubuntu, Debian or Mint:

```bash
sudo apt install build-essential nasm libx11-dev libgl1-mesa-dev
curl https://sh.rustup.rs -sSf | sh     # only if you don't have cargo
```

### Build

```bash
git clone https://github.com/Bharathsencha/Luna-Language
cd Luna-Language
make
make test        # ends with "All tests passed!"
```

If something is missing, `make check-deps` lists it. More details:
[docs/getting-started.md](docs/getting-started.md).

### Hello World

Save this as `hello.lu`:

```javascript
print("Hello, World!")
```

Run it:

```bash
./bin/luna hello.lu
```

Run `./bin/luna` with no file to open the REPL.

## Language Tour

### Variables

Use `let` for things that change and `const` for things that don't.

```javascript
let score = 10
const VERSION = "0.1"
score += 5
```

### Strings

Put any expression inside `{ }` and Luna fills it in for you.

```javascript
let hp = 42
print("You have {hp} HP, next level at {hp + 8}")
```

### Functions

Parameters can have default values.

```javascript
func greet(name, greeting = "Hello") {
    return "{greeting}, {name}!"
}

print(greet("Astra"))         # Hello, Astra!
print(greet("Astra", "Hi"))   # Hi, Astra!
```

Functions can also remember things. This is called a closure:

```javascript
func make_counter() {
    let count = 0
    return func() {
        count += 1
        return count
    }
}

let next = make_counter()
print(next())   # 1
print(next())   # 2
```

### Lists

```javascript
let nums = [1, 2, 3, 4, 5]
append(nums, 6)
print(nums[-1])   # 6, negative indexes count from the end
```

`map`, `filter` and `reduce` work just like you'd expect:

```javascript
let doubled = map(nums, func(n) { return n * 2 })
let evens = filter(nums, func(n) { return n % 2 == 0 })
```

### Maps

```javascript
let player = {"name": "Astra", "hp": 100}
print(player.name)
map_set(player, "hp", 125)
```

### Loops and choices

```javascript
for (let i in range(3)) {
    print(i)
}

if (player.hp > 100) {
    print("Feeling strong!")
} else {
    print("Keep going")
}
```

### Your own types

`data` gives you a quick way to bundle values together.

```javascript
data Vec2 { x, y }

let p = Vec2(3, 4)
print("({p.x}, {p.y})")   # (3, 4)
```

### Modules

Split your code across files. Mark what you want to share with `export`:

```javascript
# utils.lu
export func greet(name) {
    return "Hello, {name}!"
}
```

Then bring the file in with `use`. Only the exported names are visible:

```javascript
# main.lu
use "utils.lu"
print(greet("Luna"))
```

If you only need some names, pick them: `use { greet } from "utils.lu"`.

### Cleaning up automatically

`defer` runs something when the current function finishes. It's handy for closing files.

```javascript
func save_log(path, text) {
    let file = open(path, "w")
    defer(close(file))
    write(file, text)
}
```

There's a lot more in [docs/language/syntax.md](docs/language/syntax.md).

## Graphics

Here is a circle gliding around a window:

```javascript
init_window(800, 600, "Hello Luna")
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

And a tiny 3D scene with a light:

```javascript
init_window(1280, 720, "Luna 3D")
let cam = create_camera_3d([0, 5, -10], [0, 0, 0], [0, 1, 0], 45.0)
create_light(LIGHT_DIRECTIONAL, [10, 20, 10], [0, 0, 0], rgb(255, 250, 240))

while (window_open()) {
    begin_drawing()
    clear_background(rgb(15, 15, 25))
    begin_mode_3d(cam)
        draw_cube([0, 0.5, 0], [1, 1, 1], rgb(100, 180, 255))
        draw_grid(10, 1)
    end_mode_3d()
    end_drawing()
}

close_window()
```

Everything you can draw and play is listed in [docs/stdlib/gui.md](docs/stdlib/gui.md).

## Examples

Full programs written in Luna, each with a page explaining how it works.

```bash
make run-cargame             # a top-down car game
make run-music               # a music player with a live spectrum
make run-racer               # a 3D racer that changes seasons as you drive
make run-fun NAME=galaxy     # lots of small visual demos
```

Have a look at [docs/examples/](docs/examples/README.md).

## GC Performance

Longest single pause on each workload, in milliseconds, compared with Go, Java and Python.
Smaller is smoother.

| Workload | Luna | Go | Java | Python |
|---|---:|---:|---:|---:|
| alloc_heavy | 0.13-0.22 | 0.03 | 10.2 | 0 |
| long_live | 0.12-0.16 | 0.03 | 6.0 | 0 |
| cycles | 0.10-0.13 | 0 | 0 | 1.3 |
| strings | 0.10-0.13 | 0.05 | 8.4 | 0 |
| object_graph | 0.20-0.26 | - | 0 | 5.9 |

A 0 means that language did not need to collect on that workload. Python frees most objects
as it goes, so it rarely pauses.

Go has the shortest pauses. Luna stays under a millisecond on every workload. Memory use and
raw speed are the next areas to improve. Full results, and how the pauses went from 4 ms to
0.1 ms: [docs/results/gc-results.md](docs/results/gc-results.md).

## Documentation

Everything starts at [docs/README.md](docs/README.md). Some good places to begin:

- **New to Luna?** [Getting started](docs/getting-started.md), then the
  [syntax guide](docs/language/syntax.md).
- **Looking up a function?** [Core](docs/stdlib/core.md), [strings](docs/stdlib/string.md),
  [math](docs/stdlib/math.md), [files](docs/stdlib/file.md),
  [graphics and audio](docs/stdlib/gui.md).
- **Curious how it works inside?** [Architecture](docs/internals/architecture.md),
  [the VM](docs/internals/vm.md), [the garbage collector](docs/internals/gc.md),
  [unsafe memory](docs/internals/unsafe.md).
- **Want to help?** Start with [Testing](docs/testing.md).

## Project Layout

```
src/ include/   the language: parser, compiler, VM, GC, standard library
gui/            graphics, audio, and the vendored libraries they use
rust/           two small Rust runtimes
asm/            assembly (the clock)
lib/            prebuilt GLFW and the Rust libraries
test/           Luna tests
zig-test/       lower-level tests written in Zig
assets/         shared fonts and models
docs/           documentation and example programs
```

## License

Luna is free software under the **GNU General Public License v3.0 or later**. See
[LICENSE](LICENSE).

## Credits

Luna uses these open source libraries:

- [GLFW](https://github.com/glfw/glfw) for windows and input (zlib/libpng license)
- [stb](https://github.com/nothings/stb) by Sean Barrett for images, fonts and screenshots
  (MIT or public domain)
- [miniaudio](https://github.com/mackron/miniaudio) by David Reid for sound (MIT-0 or public
  domain)
- [tinyobjloader-c](https://github.com/syoyo/tinyobjloader-c) for loading 3D models (MIT)

The music and artwork in the example programs belong to their artists and are included only
for demonstration. The full list is in
[docs/examples/musicplayer/CREDITS.md](docs/examples/musicplayer/CREDITS.md) and
[docs/examples/fun/README.md](docs/examples/fun/README.md#credits). Playback icons are by
[Icons8](https://icons8.com).
