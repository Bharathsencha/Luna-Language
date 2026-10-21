# Testing

All tests are Luna programs. Each one is compiled to a native binary, run, and its output is
compared with a recorded copy.

| Command | What it does |
|---|---|
| `make test` | Builds `bin/luna` if needed, runs `compiler/run_tests.sh` on every `test/*.lu` file, then runs the native unit tests |
| `compiler/run_tests.sh test/test_core.lu` | Runs only the named files |
| `RECORD=1 compiler/run_tests.sh test/test_core.lu` | Re-records the expected output of the named files |
| `LUNA_RT_GC_STRESS=1 compiler/run_tests.sh test/test_gc_safety.lu` | Runs with a collection on every allocation (see [internals/gc.md](internals/gc.md)) |
| `sh test/native/run.sh` | Unit tests of the rule functions in `compiler/runtime/luna_rt_rules.lu`, compiled in the native dialect; prints `[OK] N checks passed` |

The runner prints one `[PASS]`, `[FAIL]` or `[SKIP]` line per test and a summary such as
`passed 110, failed 0, skipped 0`. It exits non-zero if any test failed, and so does `make test`.

## How a test passes

For each `test/<name>.lu`, `compiler/run_tests.sh`:

1. Compiles it with `bin/luna` (the positional form `luna input.lu output.ll <root>`).
2. Links the IR with the runtime object using clang.
3. Runs the binary from the repository root, with standard input closed and a 60 second limit.
4. Compares standard output, standard error and the exit status with the files in
   `test/expected/`: `<key>.out`, `<key>.err` and `<key>.rc`. The key is the path with every
   character other than letters, digits, `.`, `_` and `-` replaced by `_`
   (`test/test_core.lu` becomes `test_test_core.lu`).

Timing figures are masked before comparing: `1.23 seconds` becomes `N seconds`, and a line made
only of a long decimal number becomes `N`. Keep output that depends on time, addresses or memory
size out of the tests.

A test that is expected to fail with a runtime error is recorded like any other: its `.err` file
holds the error message and its `.rc` file holds `1`.

A file with a `<key>.skip` entry in `test/expected/` is reported as `[SKIP]`. A source with a
`.golden` file next to it is instead compared against that file (standard output only).

### Files made of cases

A file made of `# CASE <name>` sections is split: each section is compiled and run as its own
program, and has its own expected files. This is how error messages are tested, because a
runtime error ends the program. For example `test/test_unsafe_errors.lu` has one case per safety
rule, and `test/test_strict_errors.lu` has one case per strict-operator error.

```
# CASE string plus int
let s = "a" + 1
# CASE int plus bool
let b = 1 + true
```

## What the tests cover

| File | Covers |
|---|---|
| `test_core.lu` | Variables, types, chars, loops, `switch`, operators |
| `test_strict.lu`, `test_strict_errors.lu` | Strict operators, integer division, boolean conditions, and their errors |
| `test_functions.lu`, `test_call_args.lu` | Functions, recursion, closures, argument evaluation order |
| `test_qol.lu` | Compound assignment, negative indexing, default parameters, `for-in`, `range`, maps, interpolation, `data` |
| `test_strings.lu`, `test_math.lu`, `test_vectors.lu`, `test_file_io.lu` | Standard library |
| `test_modules.lu`, `util.lu`, `import_helper.lu`, `modules_fixtures/` | Module resolution, run-once rule, module state |
| `test_use_file.lu`, `test_use_private_error.lu`, `test_import_private_error.lu` | `use` and `import` forms, and the error for a name that is not exported |
| `test_bloc.lu`, `test_box.lu`, `test_template.lu`, `test_data_types.lu` | Structured data types |
| `test_unsafe.lu` | `unsafe` blocks and pointer operations |
| `test_rules.lu` | `bloc` / `box` / `template` / `unsafe` behavior that must work: 10k blocs and boxes, byte access, loop scopes, aliases |
| `test_unsafe_errors.lu` | One case per rule violation (R1 to R12, box, bloc, template) |
| `test_rules_runtime.lu` | Cases for the rule functions in `luna_rt_rules.lu`, run through the language (pointer bounds, box handles, schemas) |
| `test_gc_safety.lu`, `test_gc_large_safety.lu`, `test_gc_callbacks.lu`, `test_gc_recursion.lu` | Collector liveness under churn, large objects, callbacks and deep recursion |
| `test_compiler_blockers.lu` | Features the compiler's own source depends on |
| `test_exit.lu` | `args()` and `exit()` |
| `test_doc_examples.lu` | Examples from the docs |
| `balls.lu` | Keyword aliases |

## Adding a test

1. Create `test/test_<feature>.lu` that prints what it checks. Use `assert()` for conditions;
   a failed assertion exits with status 1, which changes the recorded output.
2. Run `RECORD=1 compiler/run_tests.sh test/test_<feature>.lu`.
3. Read the new files under `test/expected/` (or `git diff test/expected`) before committing:
   a recorded output is only as correct as you checked it to be.
4. Run `compiler/run_tests.sh test/test_<feature>.lu` again; it must pass.

## The compiler's own check

`make` compiles the compiler with itself twice and requires identical IR (stage 1 against
stage 2). That is a consistency check of the compiler, in addition to the tests above.

## Coverage gaps

- There is no independent implementation to compare against. The recorded outputs are the
  oracle, so a wrong recording stays wrong until someone reads it.
- No test covers memory use or speed.
