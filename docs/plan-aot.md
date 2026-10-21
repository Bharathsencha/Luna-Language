# Plan: Luna as an AOT compiler to LLVM IR

Goal: Luna is only a compiler. `luna build prog.lu -o prog` reads Luna, writes LLVM IR, and
clang makes a native binary. No VM, no interpreter, no C or Rust source in the repository, and
no C output backend. Nothing in this plan has been done yet; it lists what is removed, what must
exist first, and how each step is checked.

## End state

| Keep | Why |
| --- | --- |
| `compiler/*.lu` (lexer, parser, lint, analysis, `emit_llvm`, `emit_native`, driver) | the compiler |
| `compiler/runtime/*.lu` | the runtime, in Luna |
| `bootstrap/lunac.ll`, `bootstrap/luna_rt_native.ll` | seeds: the only way to build the first compiler |
| `test/*.lu` plus expected output files | regression suite |
| `docs/`, `Makefile`, `README.md`, `CLAUDE.md` | rewritten for the AOT tool |
| libc, libm, clang (LLVM 18) | external, called through `extern` |

| Remove | Size |
| --- | --- |
| `src/*.c`, `include/*.h` (VM, bytecode compiler, legacy interpreter, GC, stdlib, lexer, parser) | about 17,000 lines of C |
| `gui/*.c` (OpenGL, GLFW, audio, models) | about 5,000 lines of C |
| `rust/data_rt`, `rust/unsafe_rt`, `Cargo` files, `lib/*.a` | about 1,400 lines of Rust |
| `compiler/emit_c.lu`, `cstate.lu`, `scope.lu`, `luna_rt_abi.c`, `luna_rt.h`, `lunac.sh` | the C backend |
| `src/*.lu` (the interpreter written in Luna), `luna_in_luna` tests, `make test-lil` | an interpreter has no place in an AOT tool |
| `test_runner.sh`, `LUNA_GC_*` VM knobs, `docs/internals/{vm,arena,gc,performance}.md`, `docs/results/` | VM material |

## Decisions (from the owner)

1. GUI, 3D and audio: dropped for now. They return later as Luna programs that call GLFW and
   OpenGL through the foreign-call builtin (workstream B).
2. `luna run file.lu` builds and executes. No REPL, no interpreter, no VM.
3. The Rust rule checks move to Luna (step 3).
4. The VM is removed first, so every benchmark measures AOT code only.
5. No syntax changes. New abilities arrive as builtin functions and compiler inference, never
   as new operators, literals or keywords.
6. Target: as close to Go as possible on speed and on the shape of the toolchain (one command
   builds a static-looking native binary).

## Workstreams after the VM is gone

These come from the known weaknesses of the language today. Each has a check.

**A. Bit operations and wide constants without new syntax.** `band bor bxor shl shr sar bnot`
become ordinary builtins of the language (today only the native dialect has them) and compile
to single IR instructions. Hex and binary values are written with `int("0xFF", 16)` style
builtins that the compiler folds at compile time when the argument is a literal.
Check: a test using all of them, and the runtime sources lose their decimal-mask workarounds.

**B. Foreign calls without new syntax.** A builtin `extern_call("lib", "symbol", "i32(ptr,i64)",
args...)` (and `extern_lib("glfw3")` to name a link library) lowers to a typed IR `call` with
the right ABI. Pointers are the existing `unsafe` pointers or boxes. Check: call `getpid`,
`snprintf` and `qsort` with a Luna callback from a normal program. This is what later enables
GUI and audio.

**C. Structure without new syntax.** `bloc`, `template` and `data` already exist, so the compiler
lays `bloc` values out inline and gives constant field offsets, and the native dialect gets
named layouts generated from one table instead of hand-written offsets. Check: the runtime's
layout constants come from one generated file and a test compares them with the IR types.

**D. Speed, in order of payoff.**

| Step | Change | Expected effect |
| --- | --- | --- |
| D1 | `llvm-link` the program IR with the runtime IR and run `-O2` plus LTO, so `lvp_*` inline | removes call overhead on every operator |
| D2 | emit the integer and float fast paths (tag check, add with overflow flag, compare) inline in the IR | loops stop calling the runtime |
| D3 | type inference inside a function: a local that is only ever assigned ints (or floats, bools) lives in a raw `i64` or `double` register with overflow checks that raise the same Luna errors | numeric loops run at C speed |
| D4 | argument specialization: a function called with ints gets an int-typed clone chosen at the call site or guarded at entry | recursive numeric code (`fib`) approaches Go |
| D5 | allocation: bump allocation for small objects, strings and lists without a size-class lookup, a faster sweep | `binary_trees`-style code |
| D6 | lists of numbers stored unboxed (dense) when every element is a number | `mat_mul`, `nbody` |

Benchmarks (kept in `docs/results/aot.md`, run against a Go build of the same program):
`fib(35)`, a 100M-iteration integer loop, `nbody`, `spectral_norm`, `binary_trees`, `fannkuch`,
`mandelbrot`, string building and map insertion. Report the ratio to Go for each step. Go is the
yardstick, not a promise: dynamic typing sets a floor on code that cannot be inferred.

Semantics never change for speed: integer overflow still raises an error, division by zero
still raises, conditions must still be booleans. Inference only removes checks it can prove.

## Revised order

1. Freeze the test oracle (record expected output of every test from the VM, then compare against
   files).
2. Remove the VM, the interpreter, the GUI, the C backend and `src/*.lu`. `luna` becomes the
   compiled `lunac` (`build`, `run`, `emit`). The build needs only clang, plus the Rust libraries
   until step 4.
3. Benchmark baseline (before any optimization).
4. Port the Rust rule checks to Luna; delete `rust/` and `lib/`.
5. Workstreams A, B, C, then D1 to D6 with a benchmark after each.
6. Rewrite `Makefile` and the docs.

## Risks

- Deleting the VM removes the only independent implementation, so a compiler bug would no
  longer show up as a disagreement. Mitigation: the frozen expected outputs, plus keeping the
  seeds in git so any commit can rebuild the previous compiler.
- The seeds are the single point of failure of bootstrap. Always run `make seed` and `make
  bootstrap` before committing compiler changes, and never delete both seeds.
- The runtime currently depends on the system `libm` and `libc` behavior for formatting floats
  (`%.6g`) and `strtod`. Output is only as portable as those libraries.
- Programs that print GC-dependent timing or memory figures will differ; keep such checks out of
  the expected-output suite.

## Order of work and effort

Steps 1 and 2 are the gate and take the least time. Step 3 (rule checks in Luna) is the largest.
Step 6 is mechanical once the earlier steps pass. GUI and audio, if kept, are a separate project
after step 8.
