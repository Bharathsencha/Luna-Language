# Luna Functional Test Suite (`test/`)

This directory contains the language-level functional and regression test suite for the Luna programming language.

For the comprehensive documentation of all testing infrastructure, see [docs/testing.md](../docs/testing.md).

---

## Running the Tests

To run the entire functional test suite alongside the test runner:

```bash
make test
```

Or execute the bash test runner directly:

```bash
./test_runner.sh
```

To run individual test scripts manually:

```bash
./bin/luna test/test_core.lu
./bin/luna test/test_strings.lu
./bin/luna test/test_math.lu
./bin/luna test/test_functions.lu
./bin/luna test/test_vectors.lu
./bin/luna test/test_data_types.lu
./bin/luna test/test_bloc.lu
./bin/luna test/test_box.lu
./bin/luna test/test_template.lu
./bin/luna test/test_unsafe.lu
./bin/luna test/test_file_io.lu
./bin/luna test/test_modules.lu
./bin/luna test/test_qol.lu
```

---

## Test Directory Structure

| Test File | Description |
| :--- | :--- |
| `test_core.lu` | Core syntax, primitive types (`int`, `float`, `long`, `char`, `boolean`, `null`), ASCII conversions, operator precedence, type checking. |
| `test_strings.lu` | String library (`len`, `trim`, `substring`, `slice`, `to_upper`, `to_lower`, `reverse`, `contains`, `index_of`, `split`, `join`, string interpolation). |
| `test_math.lu` | Arithmetic, modulo, logical operators, comparisons, math library functions. |
| `test_vectors.lu` | Vector and matrix math functions (`vec_add`, `vec_sub`, `vec_mul`, etc.) and SIMD operations. |
| `test_functions.lu` | Closures, first-class functions, recursion, variable shadowing, default arguments. |
| `test_data_types.lu` | Integration test across all 4 structured data types (`data`, `bloc`, `box`, `template`). |
| `test_bloc.lu` | Stack-allocated value-type structs (`bloc`), field access, deep value equality. |
| `test_box.lu` | Fixed-capacity unmanaged memory buffers (`box[N]`), RAII/`free()`, scope invalidation. |
| `test_template.lu` | Object templates, field mutation, index access, shape introspection. |
| `test_unsafe.lu` | Unsafe blocks (`unsafe { ... }`), raw pointer `alloc()`, `store()`, `load()`, `ptr_offset()`, `defer()`. |
| `test_file_io.lu` | File I/O (`open`, `read`, `write`, `read_line`, `close`, `defer`). |
| `test_modules.lu` | Module system (`use { ... } from "..."`), exports, module caching, cross-module state. |
| `test_import_private_error.lu` | Golden error test ensuring unexported symbols produce compiler errors (paired with `.expect`). |
| `test_qol.lu` | Quality-of-life syntax (negative indexing `l[-1]`, `+=`, `-=`, `for (let x in list)`). |
| `test_gc_safety.lu` | GC stress safety under closure retention and root graph mutation (`LUNA_GC_STRESS=1`). |
| `test_gc_large_safety.lu` | GC safety with large string payloads under forced collection. |
| `balls.lu` | Physics simulation integration test. |
