# lunac: the self-hosted compiler

`lunac` compiles Luna to native code. It is written in Luna, it compiles itself, and its
runtime is written in Luna too. The output is LLVM IR; clang turns it into a binary. The
built compiler is `bin/luna`.

## Usage

```
luna build prog.lu -o prog              build a binary (LLVM IR, then clang)
luna build prog.lu -o prog -S           write C/prog.ll and stop
luna run prog.lu [args...]              build into C/run/ and run
luna emit prog.lu [-o out.ll]           write the LLVM IR only
luna input.lu output.ll [cwd]           positional form: write the IR only
luna --native a.lu b.lu -o out.ll       compile runtime source (see below)
```

`luna` finds the runtime sources under `$LUNA_HOME` (default: the current directory). Generated
files go to `C/` (gitignored) so they can be read before a binary exists. The positional form is
what the test runner and the bootstrap use; `cwd` is the directory that relative import paths
resolve against.

## Modules (`compiler/`)

| File | Role |
| --- | --- |
| `lunac.lu` | driver: arguments, build command |
| `state.lu`, `shared.lu`, `util.lu`, `token.lu` | shared state, the builtin name list, helpers, token tables |
| `lexer.lu`, `parser.lu`, `lint.lu` | front end |
| `analysis.lu` | name walks, exports, closure captures |
| `ll_state.lu`, `ll_scope.lu`, `emit_llvm.lu` | LLVM IR backend |
| `emit_native.lu` | native backend, used to compile the runtime |
| `build_runtime.sh` | builds the runtime object and prints the link inputs |
| `runtime/*.lu` | the runtime, in Luna |

Modules share mutable state through maps (`ST`), because `use` copies bindings by value.
Circular imports are an error.

## The runtime is written in Luna

`compiler/runtime/*.lu` is the runtime of every compiled program: values, strings, lists, maps,
operators, builtins, the garbage collector, `data`/`template`/`bloc`/`box` and the `unsafe`
rules. It is compiled by the native backend and linked as `C/luna_rt_native.o`.

| File | Contents |
| --- | --- |
| `luna_gc.lu` | conservative mark-sweep collector (size classes, large objects, root scan) |
| `luna_rt_core.lu` | cell layouts, errors, strings, lists, maps, to-string |
| `luna_rt_ops.lu` | operators, indexing, iteration, calls, program entry |
| `luna_rt_lib.lu` | conversions, map, list and string builtins |
| `luna_rt_math.lu` | math, files, dense lists, vectors, random numbers |
| `luna_rt_data.lu` | data, template, bloc, box and unsafe |
| `luna_rt_rules.lu` | the validation rules behind data, template, bloc, box and unsafe |

Still outside Luna: libc and libm (called through `extern`), and clang for code generation.

### The native dialect

A native file is ordinary Luna syntax with one type, the 64-bit word. Integers, pointers and
doubles (as bit patterns) are words, and memory is read and written through intrinsics.

```javascript
const CELL = 16                          // compile-time constant
let counter = 0                          // module-level word
extern("snprintf", "i32", "ptr,i64,ptr,...")   // declare a C function
space("lv_depth", 4)                     // zeroed global bytes
table("sizes", "i32", 16, 32, 48)        // constant array

func lvp_int(o, i) {                     // exported; a leading _ makes it internal
    st64(o, 2)                           // tag word
    st64(o + 8, i)                       // payload word
}

func lb_len(self_p, argc_i, argv_p) {    // _p is a pointer parameter, _i a C int
    let o = stack_buf(16)
    st64(o, 2)
    st64(o + 8, ld64(ld64(argv_p + 8) + 0))
    retp(o)                              // return the LV at o by value
}
```

| Group | Intrinsics |
| --- | --- |
| Memory | `ld8 ld16 ld32 ld64 lds8 lds16 lds32 st8 st16 st32 st64`, `p[i]` and `p[i] = v` on words |
| Bits | `band bor bxor shl shr sar bnot` |
| Unsigned | `ult ule ugt uge udiv urem` |
| Overflow | `addof subof mulof` (1 when the signed result overflows) |
| Doubles | `fadd fsub fmul fdiv fneg flt fle fgt fge feq fne i2f f2i`, `dbl(x)` to pass a double to a variadic call |
| Symbols | `cstr("text")`, `addr_of("sym")`, `fn_addr("fn")` |
| Frames | `stack_buf(n)`, `frame_addr`, `stack_ptr`, `spill_regs` |
| Calls | `call_ptr(f, args...)`, `callv(out, f, args...)` for LV-returning functions |
| Returns | `retv(tag, payload)`, `retp(ptr)` |

`and`/`or` short-circuit, `/` and `%` are signed, and calls may span lines. A function containing
`retv` or `retp` returns a 16-byte LV in two registers, as the generated code expects.

## LLVM IR design

- Every value is an `LV` in memory (`%LV = { i64 tag, i64 payload }`). An expression compiles to
  a pointer to its `LV` slot; slots are entry-block allocas.
- Generated code never passes an `LV` by value to the runtime: it passes pointers (`lvp_*`).
- Globals are `internal global %LV`. The GC scans the data segment and stack conservatively, so
  globals and allocas are roots.
- Variables captured by closures live in heap cells.

## Make targets

| Target | Effect |
| --- | --- |
| `make` | build `bin/luna` from the seeds with clang only; stage 1 and stage 2 must emit identical IR |
| `make seed` | refresh `bootstrap/lunac.ll` and `bootstrap/luna_rt_native.ll` |
| `make test` | run every test in `test/` against the recorded output, then the native unit tests |
| `make clean` | remove `bin/` and `C/` |

In a compiled program, `LUNA_RT_GC_STRESS=N` collects every N allocations and
`LUNA_RT_GC_STATS=1` prints counts at exit. `opt-18 -passes=verify x.ll -disable-output` checks IR.

## Bootstrap

The repository keeps two seeds, `bootstrap/lunac.ll` (the compiler) and
`bootstrap/luna_rt_native.ll` (the runtime). `make` builds a compiler from them (the seed
compiler), uses it to compile the current sources (stage 1), uses stage 1 to compile them again
(stage 2), and requires the two compiler outputs to match. After changing anything in `compiler/`, run `make seed` and commit the seeds.

## Requirements

`clang` (LLVM 18). Nothing else.
