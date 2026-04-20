# Zig Test Suite

This document explains Luna's Zig-based internal test suite in `zig-test/`.

It does not replace the existing Luna `assert()` tests.
It exists to give precise failure location when engine internals regress.

## Why I Added This

I wanted a stronger internal test layer, and Zig is a good fit because it can call Luna's C code directly without much friction.

The script tests in `test/` are still valuable, but they mainly answer:

- did the language behavior break?

The Zig test suite answers a different question:

- which engine subsystem broke?
- what exact token, AST node, runtime value, or error line is wrong?

That makes it much easier to debug parser and interpreter regressions after adding new language features.

## What It Tests

The first version focuses on four layers:

1. Lexer tests
   Validate token sequence, token kinds, and line information.
2. Parser tests
   Validate AST shape and node kinds.
3. Interpreter tests
   Validate runtime state changes and final values.
4. Error tests
   Validate exact error type, line, column, and message.

## Folder Layout

File structure:

- `zig-test/build.zig`
- `zig-test/src/main.zig`
- `zig-test/src/lexer_tests.zig`
- `zig-test/src/parser_tests.zig`
- `zig-test/src/interpreter_tests.zig`
- `zig-test/src/error_tests.zig`
- `zig-test/cases/`

## C APIs Used By Zig

The Zig test suite talks to Luna through small public C helpers:

- [include/luna_runtime.h](/home/bharath/Desktop/Luna-Language%20%20(main)/include/luna_runtime.h)
- [include/luna_test.h](/home/bharath/Desktop/Luna-Language%20%20(main)/include/luna_test.h)
- [include/luna_error.h](/home/bharath/Desktop/Luna-Language%20%20(main)/include/luna_error.h)

Important helpers:

- `luna_parse_source(...)`
- `luna_run_source(...)`
- `luna_lex_source(...)`
- `error_get_last(...)`

These helpers let Zig inspect internals directly without going through `main.c`.

## Running It

From the repo root:

```bash
make zig-test
```

The Makefile uses repo-local Zig cache directories so the test suite can run cleanly without polluting your global cache.

## When To Add Zig Tests

Add or extend Zig tests when you change:

- tokenization rules
- parser grammar
- AST construction
- interpreter control flow
- error line/column reporting
- any bug where a Luna script only tells you "something failed" but not where the engine actually regressed

## Relationship To `assert()`

Keep both layers.

- Luna `assert()` tests verify user-visible language behavior.
- Zig tests verify internal engine correctness.

That combination gives Luna both broad regression coverage and precise debugging signal.
