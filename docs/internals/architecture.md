# Architecture

Luna is an ahead-of-time compiler. Both the compiler and the runtime that compiled programs
link against are written in Luna. The only external pieces are clang (LLVM 18), libc and libm.

```
prog.lu
  |  compiler/lexer.lu        tokens
  |  compiler/parser.lu       AST (maps and lists)
  |  compiler/lint.lu         static check (rule R12)
  |  compiler/analysis.lu     names used, exports, closure captures
  |  compiler/emit_llvm.lu    LLVM IR text
  v
C/prog.ll ----------------+
                          |   clang -O1
C/luna_rt_native.o -------+-------------->  prog   (native executable, links libc and libm)
  ^
  |  clang -c
C/luna_rt_native.ll
  ^
  |  luna --native compiler/runtime/*.lu    (compiler/emit_native.lu)
compiler/runtime/*.lu
```

`luna build`, `luna run` and `luna emit` are the driver, `compiler/lunac.lu`. See
[getting-started.md](../getting-started.md) for the commands and [lunac.md](lunac.md) for the
compiler in detail.

## The compiler (`compiler/`)

| File | Role |
|---|---|
| `lunac.lu` | Driver: `build`, `run`, `emit`, and the internal `--native` and positional forms. Builds the clang command line |
| `lexer.lu`, `token.lu` | Source text to tokens; keyword and operator tables, including the keyword aliases |
| `parser.lu` | Recursive descent with precedence climbing; produces the AST as maps |
| `lint.lu` | The static check for `free()` in conditional flow inside `unsafe` (rule R12) |
| `analysis.lu` | Which names a function reads and writes, exports, and which variables closures capture |
| `emit_llvm.lu`, `ll_state.lu`, `ll_scope.lu` | The LLVM IR backend: expressions, statements, functions, modules, scopes and deferred calls |
| `emit_native.lu` | The native backend, used to compile the runtime (see [lunac.md](lunac.md)) |
| `state.lu`, `shared.lu`, `util.lu` | Shared state, the list of builtin names (`BUILTINS`), path and file helpers, error formatting |
| `build_runtime.sh` | Builds `C/luna_rt_native.o` from the runtime sources when they changed and prints the link input |
| `run_tests.sh` | The test runner behind `make test` |

The compiler reads imported modules while compiling, so one IR file holds the program and every
module it uses.

## The runtime (`compiler/runtime/`)

The runtime is compiled by the native backend into `C/luna_rt_native.ll`, then to an object,
and linked into every program.

| File | Contents |
|---|---|
| `luna_gc.lu` | The garbage collector. See [gc.md](gc.md) |
| `luna_rt_core.lu` | Value layouts, errors, strings, lists, maps, `to_string` |
| `luna_rt_ops.lu` | Operators, indexing, iteration, calls, recursion limit, program entry |
| `luna_rt_lib.lu` | Builtins: conversions, maps, lists, strings, `args`, `exit`, `system`, `clock` |
| `luna_rt_math.lu` | Math, files, dense lists, vector math, random numbers |
| `luna_rt_data.lu` | `data`, `template`, `bloc`, `box` and `unsafe` |
| `luna_rt_rules.lu` | The validation rules behind them: bloc and template schemas, the box handle table, the allocation table and the twelve pointer rules. Each rule returns 0 or an error code, which `luna_rt_data.lu` turns into the error message |

Every builtin `name` listed in `compiler/shared.lu` is a runtime function `lb_name(self, argc,
argv)` that returns a value. Operators are runtime calls too (`lvp_add_fast`, `lvp_lt_fast` and
so on), which is where the strict type checks and the overflow checks live.

### Values

Every Luna value is a 16-byte cell, `%LV = { i64 tag, i64 payload }`. Generated code keeps each
value in a stack slot and passes pointers to the runtime.

| Tag | Kind | Payload |
|---|---|---|
| 0 | null | none |
| 1 | boolean | 0 or 1 |
| 2 | int | 64-bit integer |
| 3 | float | the bit pattern of a double |
| 4 | char | the byte |
| 5 | string | pointer to the string object (length and bytes) |
| 6 | list | pointer to a list object (count and item cells) |
| 7 | map | pointer to a map object |
| 8 | function | pointer to a function object: code, name, captured cells; native builtins use the same kind |
| 9 | file | pointer to a file object |
| 10 | dense list | pointer to a float array object |
| 11, 13 | bloc type, `data` type | the type descriptor |
| 12 | bloc | pointer to a bloc object |
| 14 | template | pointer to a template object |
| 15 | box | a handle with a slot index and a generation |
| 16 | pointer | a raw pointer, only inside `unsafe` |

The tags are constants at the top of `luna_rt_core.lu`.

### Program start

`lv_main` (in `luna_rt_ops.lu`) initializes the data runtime, seeds the random generator from OS
entropy, then runs the program on a new thread with a 1 GiB stack, so deep recursion works. The
program thread registers its stack with the collector, runs the generated `entry` function, and
flushes standard output. The recursion limit is 100000 nested calls.

### Errors

Runtime errors are raised by `lv_error` in `luna_rt_core.lu`: it prints the error class
(`Type Error`, `Runtime Error`, ...), the source file and line, the message, the source line and
a hint, to standard error, and exits with status 1. The line comes from position data that
generated code stores before calls and operations.

## Bootstrap

`bootstrap/lunac.ll` and `bootstrap/luna_rt_native.ll` are the LLVM IR of the compiler and the
runtime, kept in git. They are the only way to build the first compiler without an existing
`luna`. `make` builds from the seeds, then from its own output, and requires the second output
to equal the first; `make seed` refreshes the seeds. See [lunac.md](lunac.md#bootstrap).

## Repository layout

| Path | Contents |
|---|---|
| `compiler/` | The compiler, the test runner, and `compiler/runtime/` |
| `bootstrap/` | The seed IR files |
| `test/` | Test programs, `test/expected/`, and `test/native/` (unit tests of runtime functions) |
| `docs/` | Documentation |
| `assets/` | The logo |
| `bin/`, `C/` | Build output: the `luna` binary, and generated IR and objects (git-ignored) |
