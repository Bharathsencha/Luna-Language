# Luna in Luna

`src/*.lu` is the Luna language implemented in Luna: lexer, parser, interpreter and standard
library, split into modules named after the C files they mirror. Run a program with:

```bash
./bin/luna src/main.lu program.lu [arguments...]
```

`make test-lil` compiles `src/main.lu` with lunac (`compiler/lunac.sh`) and checks every program in
`test/` against the bytecode VM (`compiler/run_interp_tests.sh`).

## Modules, lowest layer first

| File | Role |
|---|---|
| `state.lu` | `RT`, the state shared between modules, and `HOOK`, late-bound functions |
| `error.lu` | Output streams, error formatting, `rt_error`, `type_error`, ... |
| `token.lu`, `lexer.lu`, `parser.lu` | Source text to AST |
| `value.lu` | Value kinds, `tostr`, equality, strict arithmetic |
| `unsafe_runtime.lu` | `unsafe` blocks, pointers, rules R1-R12 |
| `box.lu` | `box` buffers: slots, generation-checked handles, scope checks |
| `data_runtime.lu` | `data`, `template` and `bloc` |
| `interpreter.lu` | Scopes, statement execution, calls, indexing |
| `statements.lu`, `expressions.lu` | One handler per statement and expression kind |
| `library.lu` | Builtin tables (`SPEC`, `HOST`, `IMPL`), argument checks, core builtins |
| `string_lib.lu`, `math_lib.lu`, `list_lib.lu`, `file_lib.lu`, `memory_lib.lu` | Builtins by area; each has a `register_*()` |
| `module_runtime.lu` | `use` / `import`: resolution, run-once cache, cycle errors |
| `lint.lu` | Static check for rule R12 |
| `main.lu` | Fills the handler tables and hooks, then runs the program |

## Rules for these files

- Imports form a tree with no cycles. A lower layer that needs a higher one goes through `HOOK`.
- An imported name is a copy, so a value that several modules assign lives in `RT`
  (`RT.cur_line`, `RT.func_depth`, ...), never in a top-level `let`.
- Handlers are registered in `main.lu` (`EV`, `EX`, `SPEC`/`HOST`/`IMPL`), not called by name.
- The files must stay inside the subset lunac compiles: a literal brace in a string is `\{`, and
  `data`, `from`, `in`, `input` and `print` are never identifiers.
