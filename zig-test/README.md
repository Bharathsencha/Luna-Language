# Zig Test Suite

This folder is Luna's Zig-based internal test suite.

It does not replace the existing Luna `assert()` tests.
It exists to catch regressions inside the engine itself:

- lexer token shape and line info
- parser AST shape
- interpreter state changes
- structured error line and message checks

It is now split into two parts:

- `src/` for the Zig test code and shared helpers
- `cases/` for file-based regression fixtures used by the tests

Run it with:

```bash
make zig-test
```

Current layout:

- `build.zig`
- `README.md`
- `src/support.zig`
- `src/lexer_tests.zig`
- `src/parser_tests.zig`
- `src/interpreter_tests.zig`
- `src/error_tests.zig`
- `cases/lexer/`
- `cases/parser/`
- `cases/interpreter/`
- `cases/error/`

What is covered now:

- exact token ordering and lexeme stability
- parser block layout and expression precedence
- interpreter assignments, branches, and function results
- parser/runtime error type, line, and message capture
