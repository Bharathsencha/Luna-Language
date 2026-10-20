# lunac: the self-hosted Luna compiler

`lunac` is a compiler for Luna written in Luna. It turns a program into
LLVM IR (`.ll`) or C (`.c`) and, with `-o`, builds a native binary. No
interpreter is involved in the output.

## Usage

```javascript
// lunac prog.lu -o prog             build a binary (LLVM IR, clang)
// lunac prog.lu -o prog --emit=c    build through C instead
// lunac prog.lu -o prog -S          also keep C/prog.ll
// lunac in.lu out.ll [cwd]          positional mode: write IR or C only
```

The extension of the output chooses the backend: `.ll` emits LLVM IR, anything
else emits C. Generated files go to `C/` (gitignored) so they can be reviewed
before a binary is built.

## Modules (`compiler/`)

| File | Role |
| --- | --- |
| `lunac.lu` | driver: argument parsing, build command |
| `state.lu`, `util.lu`, `token.lu` | shared state, helpers, token kinds |
| `lexer.lu`, `parser.lu`, `lint.lu` | front end |
| `analysis.lu` | name walks, exports, closure captures (both backends) |
| `cstate.lu`, `scope.lu`, `emit_c.lu` | C backend |
| `ll_state.lu`, `ll_scope.lu`, `emit_llvm.lu` | LLVM IR backend |
| `runtime/luna_rt*.c`, `luna_gc.c` | C runtime and mark-sweep GC |
| `runtime/luna_rt_ir.c` | pointer-ABI wrappers (`lvp_*`) used by IR |

Modules share mutable state through maps (`ST`), because `use` copies
bindings by value. Circular imports are an error.

## LLVM IR design

- Every value is an `LV` in memory (`%LV = { i64 tag, i64 payload }`). An
  expression compiles to a pointer to its `LV` slot; slots are entry-block
  allocas.
- IR never passes an `LV` by value to the runtime. `luna_rt_ir.c` wraps each
  call with pointers, avoiding C ABI struct-splitting differences.
- Globals are `internal global %LV`. The GC scans the data segment and stack
  conservatively, so globals and allocas are roots.
- Variables captured by closures live in heap cells.

## Make targets

| Target | Effect |
| --- | --- |
| `make build-c` | write `C/lunac.c` and `C/lil.c` for review |
| `make build-ll` | VM compiles lunac to `C/lunac.ll` |
| `make lunac` | C self-host, stage 1 and stage 2 must emit identical C |
| `make lunac-ll` | IR self-host, `lunac_ll1` must reproduce the IR byte for byte |
| `make seed` | refresh `bootstrap/lunac.ll` |
| `make bootstrap` | build `bin/lunac/lunac` from the seed with clang only |
| `make test-lunac`, `make test-ll` | 82 differential tests, C and IR |

`LUNAC_BACKEND=llvm compiler/run_tests.sh` runs the suite through IR.

## Requirements

`clang` (LLVM 18). `opt-18 -passes=verify x.ll -disable-output` checks IR.
Only the C runtime is linked; the Rust runtimes are not part of lunac output.
