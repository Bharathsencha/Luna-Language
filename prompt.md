# Prompt Engineering Playbook: Claude Code Directives

Master prompt templates, execution protocols, and task directives for driving Claude Code in the Luna Language repository.

## 1. Operating Protocol for Claude Code

When initiating any session or dispatching a task to Claude Code, adhere to this execution protocol:

1. Step 1: Subsystem Inspection
   - Locate existing headers in `include/` and implementations in `src/`, `vm/`, `gui/`, or `rust/`.
   - Grep for symbol usages and callers across the entire codebase before proposing modifications.
   - Read relevant lines from `context.md` and `CLAUDE.md`.

2. Step 2: Implementation Design
   - Propose changes that match the existing C11 and Rust coding style.
   - Respect memory ownership boundaries (Arena vs Tracing GC vs Unsafe Manual Blocks).
   - Never skip GC write barriers when mutating pointer references on the heap.

3. Step 3: Surgical Execution
   - Avoid massive file rewrites. Make minimal, targeted diffs.
   - Keep comments concise and purely technical.
   - Do not add extraneous dependencies or third-party libraries.

4. Step 4: Verification and Validation
   - Compile cleanly with `make`.
   - Run the full test suite with `make test` or `./test_runner.sh`.
   - For GC or memory changes, run under stress mode:
     `env LUNA_GC_STRESS=1 LUNA_GC_VERIFY=1 ./bin/luna test/test_gc_safety.lu`

5. Step 5: Output Discipline
   - STRICTLY NO EMOJIS in answers, summaries, code comments, commit messages, or terminal output.
   - Use standard markdown formatting with clean text tags (`[OK]`, `[ERROR]`, `[WARN]`, `[NOTE]`).

---

## 2. Reusable Prompt Templates

### Template A: New Language Feature / Syntax Addition
Copy and adapt this template when implementing a new syntax feature, expression type, or language keyword:

```markdown
Task: Implement [Feature Name] in Luna

Context:
Refer to context.md for architectural memory boundaries. We need to implement [feature description] with JavaScript-like syntax and C11 performance.

Required Steps:
1. Lexer: Update src/lexer.c and include/token.h to recognize any new tokens/keywords.
2. Parser: Update src/parser.c and src/arena.c to parse the construct using bump-allocated AST nodes.
3. AST: Update include/ast.h and src/ast.c with new node types and visitor functions.
4. Interpreter: Implement execution logic in src/interpreter.c and environment handling in src/env.c.
5. VM (if applicable): Add opcode in vm/luna_opcode.h, compile bytecode in vm/luna_compiler.c, and dispatch in vm/luna_vm.c.
6. Tests: Add test cases to test/test_[feature].lu and update test_runner.sh.

Constraints:
- Strictly no emojis anywhere in code, comments, or output.
- AST nodes must come from the bump arena, never direct malloc.
- Verify with `make test` before completing.
```

### Template B: Bug Fix and Memory Debugging
Copy and adapt this template when diagnosing segfaults, memory corruption, or GC leaks:

```markdown
Task: Diagnose and Fix [Bug Description / Error Symptom]

Context:
Luna uses an incremental tri-color GC with SATB write barriers alongside bump-allocated AST memory and Rust-managed unsafe blocks. Refer to context.md.

Reproduction:
Command: ./bin/luna [path/to/failing_script.lu]
Expected: [Expected behavior or exit code]
Actual: [Observed failure, crash, or memory anomaly]

Investigation Protocol:
1. Identify the exact failure site and stack trace.
2. Check if a GC barrier was omitted during heap pointer mutation.
3. Check if AST memory is being mistakenly freed or scanned by the GC.
4. Verify whether pointer arithmetic in unsafe blocks violates any of the 12 Rust runtime safety rules.
5. Apply the minimal fix, rebuild with `make`, and run:
   env LUNA_GC_STRESS=1 LUNA_GC_VERIFY=1 ./bin/luna [test_file]
6. Ensure `make test` passes without regressions.

Constraints:
- Strictly no emojis.
```

### Template C: Performance and SIMD Vectorization
Copy and adapt this template when writing performance-critical algorithms or SIMD math routines:

```markdown
Task: Optimize [Function or Subsystem Name]

Context:
Luna achieves bare-metal speed using SIMD vectorization (AVX2/NEON), bump arenas, and tail-call optimization.

Objective:
Optimize [function name] located in [file path] to improve throughput / reduce latency.

Guidelines:
1. Review src/vec_lib.c and asm/ for existing vectorization patterns.
2. Keep inner loops contiguous and cache-friendly; avoid unnecessary indirection or pointer chasing.
3. Benchmark before and after using monotonic timers from src/time_lib.c or benchmark/ suite.
4. Maintain full functional correctness and identical output.
5. Verify build with `make` and run all regression tests.

Constraints:
- Strictly no emojis in code or output.
```

### Template D: Non-Vibe-Coded UI, Tooling, and Demo Development
Copy and adapt this template whenever Claude is asked to build or modify any user interface, graphical application, web interface, or developer tool:

```markdown
Task: Build / Update [UI Component or Graphical Application Name]

Design System Directives (ANTI-VIBE-CODING ENFORCEMENT):
The interface must look intentional, high-craft, and engineered for production systems. It must NOT look vibe-coded or AI-generated.

Forbidden AI UI Tropes:
1. NO purple-to-indigo or violet gradients (`from-indigo-500 to-purple-600`), glowing blurred backdrop blobs, or neon card borders.
2. NO floating pill badges with sparkles (sparkle icons) or emoji status chips above headings.
3. NO generic 3-card grids with icons in rounded squares in the top-left corner.
4. NO cartoonish oversized border radiuses (`rounded-2xl`, `rounded-3xl`). Use 2px to 4px maximum.
5. NO generic marketing copy ("Supercharge your workflow", "Unleash next-gen speed").
6. NO unmotivated hover animations (`hover:scale-105 transition-all`).
7. NO fake metric cards with arbitrary percentage tags (+12% vs last month).
8. NO emojis in buttons, titles, logs, or status indicators.

Affirmative UI Requirements:
1. Layout: High information density, utilitarian grid inspired by Linear, Raycast, JetBrains, and Figma.
2. Palette: Neutral monochromatic scale (zinc/slate/neutral grays) with high contrast. Single restrained accent color used exclusively for primary action and focus.
3. Geometry: Sharp, crisp 1px borders with explicit contrast (`border-zinc-800` on `bg-zinc-900`). Border radius restricted to 2px-4px (`rounded-sm`/`rounded-md`).
4. Typography: Monospace font (`font-mono`) and tabular numbers (`tabular-nums`) for all numeric values, memory addresses, benchmarks, and counters.
5. States: Implement comprehensive states: normal, hover (subtle background tint shift, no scaling), active, focus-visible (high-contrast keyboard outline), disabled, skeleton loading, empty, and actionable error states.

Verification:
Verify layout, contrast, and functionality. Confirm zero emojis are present.
```

---

## 3. Negative Constraints Clause (Copy into Custom Instructions)

Include this snippet in custom prompt headers or tool configurations:

```
[STRICT CONSTRAINTS]
1. Zero emojis policy: Do not output emojis under any circumstances in text, code, comments, or UI.
2. Architecture integrity: Preserve C11 standards, bump-allocated AST outside GC, and Rust FFI safety rules.
3. Anti-vibe-coding: Eliminate purple gradients, floating sparkles, oversized radiuses, and generic AI tropes in UI.
4. Verification: Run `make` and `./test_runner.sh` to validate all changes before concluding.
```
