# Bytecode VM

Programs run on a register-based bytecode VM. The AST is compiled once, then executed with
computed-goto dispatch (`goto *dispatch_table[*ip++]`). Compilers without GNU labels-as-values
fall back to a `switch` loop.

| File | Role |
|---|---|
| `src/luna_compiler.c` | AST to bytecode: register allocation, jump patching, constant pool, closures |
| `src/luna_vm.c` | Dispatch loop, call frames, scopes, `defer`, unsafe checks, imports |
| `src/luna_chunk.c`, `include/luna_chunk.h` | Chunk storage: code, line map, constants, nested function subchunks |
| `include/luna_opcode.h` | Opcode set and operand layout |
| `src/luna_vm_gc.c` | GC roots for every live VM (registers, frames, upvalues, deferred calls) |

## Instruction format

Each instruction is a one-byte opcode followed by operands. Register and argument-count
operands are 8 bits; constant indices and jump offsets are 16 bits (offsets are signed);
`LOAD_INT` and `LOAD_FLOAT` carry a 64-bit immediate.

```
VM_OP_ADD        dst, lhs, rhs
VM_OP_LOAD_CONST dst, const_idx16
VM_OP_JUMP_IF_FALSE cond, offset16
VM_OP_CALL       dst, func, argc
```

The set has 53 opcodes, grouped as follows:

| Group | Opcodes |
|---|---|
| Loads | `LOAD_INT`, `LOAD_FLOAT`, `LOAD_CONST`, `LOAD_TRUE`, `LOAD_FALSE`, `LOAD_NULL`, `MOVE` |
| Arithmetic | `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `NEG`, `NOT`, `FMT` |
| Compare | `EQ`, `NEQ`, `LT`, `LTE`, `GT`, `GTE` |
| Control | `JUMP`, `JUMP_IF_TRUE`, `JUMP_IF_FALSE`, `RETURN`, `HALT`, `SAFEPOINT` |
| Variables | `GET_GLOBAL`, `SET_GLOBAL`, `GET_UPVAL`, `SET_UPVAL` |
| Collections | `NEW_LIST`, `LIST_APPEND`, `INDEX_GET`, `INDEX_SET`, `NEW_MAP`, `MAP_SET`, `FIELD_GET`, `FIELD_SET`, `BOX_ALLOC` |
| Calls | `CALL`, `CALL_NAMED`, `CLOSURE`, `DEFER`, `HAS_ARG` |
| Scopes | `SCOPE_BEGIN`, `SCOPE_EXIT`, `UNSAFE_BEGIN`, `UNSAFE_END` |
| Unsafe | `ADDR_OF`, `ADDR_OF_GLOBAL` |
| Modules | `IMPORT` |
| Output | `PRINT` |

Locals live in registers in the current call frame. Top-level `let` and `func` bind to globals,
so modules, the REPL and the automatic `main()` call all see them.

## Notable mechanics

- **String fusion (`FMT`).** The compiler recognizes `repeat(const, N) + to_string(x)` and emits
  one `FMT` instruction that builds the result in a single allocation. This pattern dominates
  the string-heavy GC workloads.
- **Interned name cache.** Global names are interned once per chunk, so `GET_GLOBAL` compares
  pointers instead of strings.
- **Closures.** `CLOSURE` captures upvalues. Open upvalues point at live registers and are
  closed (copied into the upvalue) when the frame exits, as in Lua and clox.
- **Default parameters.** `HAS_ARG` tells the callee whether the caller supplied an argument, and
  the default expression runs only when it did not.
- **Scopes.** `SCOPE_BEGIN` / `SCOPE_EXIT` bracket blocks that own `box` buffers or deferred
  calls. When no boxes are live, the exit is O(1).
- **Imports.** `IMPORT` runs the module's chunk in a nested VM with its own environment, rooted at
  the importer, and caches it so each module runs once. `luna_vm_gc.c` keeps a registry of
  active VMs so an outer VM's stack stays rooted while a nested one runs.
- **Safepoints.** Emitted at loop headers and every 16 statements; the VM calls into the GC at
  every 1024th one. See [gc.md](gc.md#collection).

## Limits

| Limit | Value | Defined in |
|---|---:|---|
| Call depth | 64 frames | `FRAMES_MAX`, `include/luna_vm.h` |
| Value stack | 1024 slots | `STACK_MAX` |
| Nested scopes | 256 | `VM_SCOPE_MAX` |
| Pending deferred calls | 64 | `VM_DEFERRED_MAX` |
| Jump distance | +-32767 bytes | 16-bit offset |
| Constants per chunk | 65536 | 16-bit index |

The 64-frame call depth is the limit you will hit first, with deep recursion. Self tail
calls are optimized only in the legacy interpreter.

## Legacy interpreter

`src/interpreter.c` is the original tree-walking interpreter. It is still used by the embedding
API (`src/luna_runtime.c`) and by the Zig tests, and it runs host-side native callbacks
(dispatching VM closures through `luna_vm_call_closure`). Set `LUNA_USE_INTERPRETER=1` to run a
whole script on it.

The two engines currently differ. The interpreter accepts the aliases `ptr_add`, `deref` and
`addr`, and enforces unsafe rules R5, R9 and R11; the VM does not. See
[unsafe.md](unsafe.md#engine-status).
