# Luna JIT Plan

Move the bytecode VM to a proper JIT. This document is the working plan; it will
be updated as phases land.

## Where we are

```
source.lu → lexer → parser (AST) → luna_compiler.c → custom bytecode (53 opcodes)
          → luna_vm.c computed-goto dispatch (register VM, 16-byte Values on a stack)
```

- `Value` = 16-byte tagged struct (type enum + union).
- GC = incremental generational Immix-style tracer; precise roots are the VM
  value stack + env + upvalues (marked by `vm_gc_mark_roots`, multi-VM registry).
- Current 3-run numbers (classic workloads): Luna 0.07–0.11s vs Java 0.02–0.03s
  and Go 0.11–0.23s.  GC max pause 0.08–0.10ms (better than Java G1, in Go's range).

Remaining user-time gap vs Java is interpreter overhead: per-opcode dispatch
(2 loads + 1 store + indirect branch), operand decoding, and all values living
in the VM stack instead of registers.

## Strategy: non-speculative copy-and-patch baseline JIT

- Compile **whole subchunks** (functions) to x86-64 machine code.
- Each bytecode opcode translates 1:1 to a machine-code block — no speculative
  optimization in v1, so **no deoptimization machinery is needed**.
- Copy-and-patch: pre-assemble one code template per opcode with relocation
  metadata at JIT init; per function, copy templates into executable memory and
  patch operands/jump targets. (Same technique as the new OpenJDK baseline JITs.)
- Values keep living in the VM stack in v1 (exact same layout → the GC's root
  marking works unchanged).

## Dependencies (must land first)

1. **Fix the allocation-driven GC trigger bug.** Today GC only fires on a
   1024-opcode safepoint counter; compiled code has no opcode counter, so it
   must trigger on allocation (`young_bytes >= young_limit`) at machine-code
   safepoints. The previous attempt exposed a liveness bug under rapid
   stepping (concurrent_map hang) — root-cause that first:
   - repro: allocation-triggered minors + stores during sweep
   - suspected area: remembered-set batching vs promoted objects mid-sweep
   - fix + re-run: 8 GC scripts, full suite, safety (stress+verify), Java bench
2. **Safepoint contract**: define exactly what a GC step needs from compiled
   code (valid `frame->ip` → bytecode offset, `stack_top`, upvalue chain) and
   keep one `vm_gc_mark_roots` ABI.

## Phase 0 — machine-code infrastructure (1–2 days)

- `mmap` executable pages with W^X discipline (write-protect then exec, never
  both). One `CodeBuffer` per function, mprotect-flipped after patching.
- Tiny x86-64 assembler DSL: mov/lea/add/cmp/test, call rel32, jcc rel32,
  jmp rel32, and a relocation list (label fixups) for copy-and-patch.
- `jit/` module: `jit_init`, `jit_compile_chunk(LunaChunk*)`, `jit_invoke(...)`.

## Phase 1 — baseline translator (3–5 days)

- Per-opcode templates; each reads operands via a fixed register base (rbx =
  frame slots, r12 = chunk constants, r13 = VM), calls existing C helpers for
  anything non-trivial (ADD/SUB…/EQ…/INDEX_*/FIELD_*/MAP_*/FMT/CALL/DEFER/
  IMPORT/CLOSURE…), writes the result, falls through or jumps.
- Bytecode jumps → direct machine jumps (single compile pass patches targets).
- `RETURN`/`HALT` tear down the frame; closures call the compiled entry of the
  callee subchunk (ABI: rdi=vm, rsi=slots, rdx=upvalues).
- Safepoints: on loop back-edges and function entry, `test byte [gc_pending]`
  → slow stub that records the bytecode offset and re-enters the interpreter.
- Fallback: functions containing an untranslated opcode run in the interpreter
  (broad coverage isn't needed for the hot loops: ADD/FMT/GET_GLOBAL/
  LIST_APPEND/SAFEPOINT/LOOP-CONTROL cover alloc_heavy/strings/long_live).

Exit criteria: alloc_heavy/long_live/strings run fully JIT'd, correct output,
sub-ms GC pauses preserved.

## Phase 2 — calls, closures, full coverage (2–3 days)

- Natives via direct C calls (`luna_call_value` wrapper).
- VM closures + upvalues through the JIT ABI; `HAS_ARG`/defaults.
- Translate the remaining opcodes (maps, blocs, templates, unsafe, import,
  defer, scopes) until `LUNA_USE_INTERPRETER` fallback is only for debugging.
- REPL + module imports run on the JIT.

## Phase 3 — register promotion & loop opts (1–2 weeks)

- Liveness analysis over bytecode; keep hot locals in real registers between
  opcode blocks (spill to the VM stack at safepoints — roots stay exact).
- Hoist `GET_GLOBAL`/interned-name lookups out of loops (cache `Value*` slot
  + env version check, same trick the interpreter uses).
- Inline micro fast-paths in machine code: int→string (`itoa`), list-append
  barrier, string concat for two strings.

Target: classic workloads within ~1.5x of Java (0.04–0.06s on alloc_heavy).

## Phase 4 — tuning loop (ongoing)

- `perf` profiles on JIT'd runs; address the top frames (allocator fast path,
  barrier inlining, TLAB-like bump alloc for young objects).
- Re-run the full gauntlet each round: `make test`, `make test-gc-three`,
  `make test-java-gc`, GC safety stress+verify.

## Risks / decisions

- **Platform**: x86-64 Linux first; ARM64 (AArch64 copy-and-patch) later.
- **W^X**: no self-modifying code; recompile-only on function change.
- **GC interplay**: every JIT milestone gates on the GC trigger fix + safety
  suite — never on pause regressions.
- **Debugging**: interpreter stays as the reference implementation
  (`LUNA_USE_INTERPRETER=1`), and the JIT prints a side-by-side trace mode for
  divergence hunting (`LUNA_JIT_TRACE=1`).

## Milestones (rough)

| # | Milestone | Exit criteria |
|---|---|---|
| 0 | exec-memory infra + assembler | unit smoke test runs machine code |
| 1 | baseline JIT on hot ops | alloc_heavy/strings correct, GC pauses unchanged |
| 2 | full opcode coverage + closures | `make test` green under JIT default |
| 3 | register promotion | alloc_heavy ≤ ~0.06s |
| 4 | tuning | 3-way bench tables refreshed in README + docs |
