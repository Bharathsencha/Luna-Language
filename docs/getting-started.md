# Getting Started

## Requirements

Linux on x86-64 (the timer in `asm/time.asm` makes a raw x86-64 syscall).

| Tool | Used for |
|---|---|
| `gcc` (C11, OpenMP) | The core |
| `nasm` | `asm/time.asm` |
| `cargo` (Rust 2021) | `rust/unsafe_rt`, `rust/data_rt` |
| OpenGL and X11 development headers | Linking the renderer |
| `zig` (optional) | `make zig-test` |

On Ubuntu, Debian or Mint:

```bash
sudo apt install build-essential nasm libx11-dev libgl1-mesa-dev
curl https://sh.rustup.rs -sSf | sh     # if cargo is not installed
make setup-ubuntu                       # optional: full X11/audio dev package set
make check-deps                         # reports anything missing
```

GLFW (`lib/libglfw3.a`), the stb headers, miniaudio and tinyobj are vendored. Nothing else
needs to be installed.

## Build

```bash
make            # parallel by default; builds the Rust crates, then links bin/luna
make clean      # removes obj/, bin/ and the Cargo targets
make clean-c    # C objects only
```

## Run

```bash
./bin/luna script.lu    # run a file
./bin/luna              # REPL (also: make repl)
make run                # runs main.lu
```

A first script:

```javascript
let name = input("Name: ")
print("Hello, {name}!")

func main() {           # runs automatically after the top level
    for (let i in range(3)) {
        print("tick {i}")
    }
}
```

To check that graphics and audio work, run an example:

```bash
make run-cargame
make run-fun NAME=meadow
```

## Verify

```bash
make test         # Luna script tests + Rust crate tests
make zig-test     # host-side Zig tests (needs zig)
```

See [testing.md](testing.md) for details.

## Next

- [language/syntax.md](language/syntax.md): the language
- [stdlib/core.md](stdlib/core.md): builtins
- [examples/README.md](examples/README.md): complete programs
