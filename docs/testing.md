# Testing

Luna has three test layers. Run all of them before committing a change to the core.

| Command | What it checks |
|---|---|
| `make test` | Luna script tests (`test/`), then `cargo test` for both Rust crates |
| `make test-gc-safety` | The GC safety scripts under `LUNA_GC_STRESS=1 LUNA_GC_VERIFY=1` |
| `make zig-test` | Host-side Zig tests that call the C API directly |
| `make test-rust-data` | Rust `data_rt` tests only |

## Luna script tests (`test/`)

`./test_runner.sh` runs every `test/*.lu` file. A test passes in one of two ways:

- **Assertion:** the script exits cleanly, and every `assert()` held.
- **Output match:** when `test/<name>.expect` exists, stdout must match it exactly. This is
  how error-message tests work (for example `test_import_private_error.expect`).

| File | Covers |
|---|---|
| `test_core.lu` | Variables, types, chars, loops, `switch`, operators |
| `test_functions.lu` | Functions, recursion, closures |
| `test_qol.lu` | Compound assignment, negative indexing, default params, `for-in`, `range`, maps, interpolation, `data` |
| `test_strings.lu`, `test_math.lu`, `test_vectors.lu`, `test_file_io.lu` | Standard library |
| `test_modules.lu`, `util.lu`, `import_helper.lu`, `modules_fixtures/` | Module resolution, caching, state |
| `test_import_private_error.lu` | Importing a non-exported name fails with the expected message |
| `test_bloc.lu`, `test_box.lu`, `test_template.lu`, `test_data_types.lu` | Structured data types |
| `test_unsafe.lu` | `unsafe` blocks and pointer operations |
| `test_gc_safety.lu`, `test_gc_large_safety.lu` | GC liveness under churn and large heaps |
| `test_doc_examples.lu` | Examples from the docs |
| `balls.lu` | Keyword aliases |

To add a test, create `test/test_<feature>.lu` that uses `assert()`. Add a `.expect` file only
when the output itself is what you are testing.

## GC stress mode

```bash
make test-gc-safety
env LUNA_GC_STRESS=1 LUNA_GC_VERIFY=1 ./bin/luna test/test_gc_safety.lu
```

`LUNA_GC_STRESS` divides both collection triggers by 8. `LUNA_GC_VERIFY` re-walks the heap
after every collection and aborts on the first invalid object. Run these after any change to
`src/gc.c`, a write barrier, or a type's trace function.

## Zig tests (`zig-test/`)

The script tests show that something broke. The Zig tests show where: they call
`luna_lex_source`, `luna_parse_source`, `luna_run_source` and `error_get_last`
(`include/luna_runtime.h`, `include/luna_test.h`) and assert on exact tokens, AST node kinds,
runtime values, and error line and column.

| File | Layer |
|---|---|
| `src/lexer_tests.zig` | Token kinds, sequence and lines |
| `src/parser_tests.zig` | AST shape |
| `src/interpreter_tests.zig` | Runtime state |
| `src/error_tests.zig` | Error type, line, column, message |
| `cases/` | Source inputs for the above |

These tests run on the embedding API, which uses the legacy tree-walking interpreter, not the
VM. Add a Zig test when changing tokenization, grammar, AST construction or error reporting.

## Coverage gaps

- No test runs the same scripts on both the VM and the interpreter, so the two engines have
  drifted apart (see [known-issues.md](known-issues.md)).
- No test covers `continue` inside a C-style `for` loop, or recursion deeper than 64 calls.
- Graphics and audio have no automated tests. `docs/examples/fun/scene3d.lu` and
  `scene_models.lu` are manual smoke tests.
