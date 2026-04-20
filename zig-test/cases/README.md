# Fixture Layout

This folder now holds the file-based regression fixtures used by the Zig suite.

Current structure:

- `lexer/` token stream fixtures
- `parser/` AST shape fixtures
- `interpreter/` runtime state fixtures
- `errors/` expected line and message fixtures

Guideline:

- keep each fixture small
- isolate one parser/runtime behavior per file when possible
- add a new fixture when a bug is easier to explain with source code than with an inline string
