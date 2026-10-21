# Getting Started

Luna is an ahead-of-time compiler. A program is compiled to LLVM IR by the Luna compiler,
and `clang` turns the IR into a native executable. There is no interpreter, REPL or VM.

## Requirements

Linux. The project is developed and tested on x86-64 with glibc.

| Tool | Used for |
|---|---|
| `clang` (LLVM 18) | Compiling the generated IR and the runtime, and building the first compiler from the seeds |
| `make` | Building `bin/luna` and running the tests |

Nothing else is needed: the compiler and the runtime are written in Luna, and compiled
programs link only libc, libm, libpthread and libdl.

On Ubuntu or Debian:

```bash
sudo apt install build-essential clang
```

## Build

```bash
make            # builds bin/luna (parallel by default)
make test       # compiles and runs every test
make seed       # refreshes bootstrap/*.ll after changing compiler/ or compiler/runtime/
make clean      # removes bin/ and C/
```

`make` does this, in order:

1. Builds a seed compiler from `bootstrap/lunac.ll` and `bootstrap/luna_rt_native.ll` with clang.
2. Uses it to compile the runtime and the compiler sources of the working tree.
3. Builds that compiler (stage 1), uses it to compile the compiler again (stage 2), and
   requires the two IR files to be identical.
4. Copies stage 1 to `bin/luna`.

If the seeds are older than the sources, `make` prints a `[WARN]`. Run `make seed` and commit
the new seeds together with the change that needs them. See [internals/lunac.md](internals/lunac.md).

## Use

```bash
./bin/luna build prog.lu -o prog     # native binary
./prog

./bin/luna run prog.lu a b           # build into C/run/prog and run it with arguments a and b
./bin/luna emit prog.lu              # write the LLVM IR to C/prog.ll
./bin/luna build prog.lu -S          # same, through build
```

| Command | Effect |
|---|---|
| `luna build prog.lu -o out` | Write `C/prog.ll`, then build `out` with clang. Without `-o` the output is named after the file |
| `luna build prog.lu -S` | Write `C/prog.ll` and stop |
| `luna run prog.lu [args...]` | Build into `C/run/prog`, run it, and exit with its status |
| `luna emit prog.lu [-o out.ll]` | Write the IR only; the default is `C/prog.ll` |

Generated IR is written to a `C/` directory in the current directory (it is git-ignored in this
repository), so it can be read before a binary exists. Set `CC` to use another C compiler
driver for the final step (the default is `clang`).

### Where luna finds the runtime

`luna` has no installed location. It looks for `compiler/runtime` and
`compiler/build_runtime.sh` under the directory named by `LUNA_HOME`, or the current directory
when that is not set. To compile from another directory:

```bash
export LUNA_HOME=/path/to/Luna-Language
/path/to/Luna-Language/bin/luna run prog.lu
```

The first build also compiles the runtime to `$LUNA_HOME/C/luna_rt_native.o`; it is rebuilt
only when a runtime or compiler source is newer.

### A first program

```javascript
let name = input("Name: ")
print("Hello, {name}!")

func main() {           # runs automatically after the top level
    for (let i in range(3)) {
        print("tick {i}")
    }
}
```

`input` returns one line from standard input, without the newline (or `""` at end of input).
`args()` returns the command-line arguments after the program name as a list of strings.
`exit(code)` stops the program with that status.

### Errors

A syntax error stops the compile: `luna` prints the message and exits with status 1, and no
binary is written. A runtime error (type error, index error, division by zero, ...) prints the
message with the source line to standard error and the program exits with status 1:

```
Type Error in prog.lu at line 1:
  cannot use '+' on string and int

   1 | print("a" + 1)

Hint: Both sides must be numbers, or both strings. Build text with "{x}" or to_string(x).
```

## Verify

```bash
make test
compiler/run_tests.sh test/test_core.lu     # one test
```

See [testing.md](testing.md).

## Next

- [language/syntax.md](language/syntax.md): the language
- [stdlib/core.md](stdlib/core.md): builtins
- [internals/architecture.md](internals/architecture.md): how the pieces fit
