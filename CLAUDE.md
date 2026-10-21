# CLAUDE.md

Configuration and operational instructions for Claude Code when working in the Luna Language repository.

Luna is an ahead-of-time compiler. `bin/luna` is written in Luna and compiles Luna source to LLVM IR; clang builds the native binary. The runtime that compiled programs link is Luna source too (`compiler/runtime/*.lu`). There is no interpreter, no VM, no REPL and no GUI at the moment. Read `docs/plan-aot.md` for the direction of the project and `docs/internals/architecture.md` for the pipeline.

## Commands

### Build Commands
- Full build: `make` (builds `bin/luna` from the seeds in `bootstrap/` with clang; the compiler must reproduce its own IR, stage 1 against stage 2)
- Refresh the seeds after changing `compiler/` or `compiler/runtime/`: `make seed` (commit `bootstrap/lunac.ll` and `bootstrap/luna_rt_native.ll` with the change; never delete both seeds)
- Clean all build artifacts: `make clean` (removes `bin/` and `C/`)

### Test Commands
- Run complete test suite: `make test` (runs `compiler/run_tests.sh` over `test/*.lu`, then `test/native/run.sh`)
- Run one or more tests: `compiler/run_tests.sh test/test_core.lu`
- Re-record expected output (read `git diff test/expected` afterwards): `RECORD=1 compiler/run_tests.sh test/test_core.lu`
- Run the GC under stress: `LUNA_RT_GC_STRESS=1 compiler/run_tests.sh test/test_gc_safety.lu`
- Native unit tests of the runtime rule functions: `sh test/native/run.sh`

### Compile and Run Commands
- Build a native binary: `./bin/luna build prog.lu -o prog` (add `-S` to write `C/prog.ll` and stop)
- Build and run: `./bin/luna run prog.lu [args...]`
- Write the LLVM IR only: `./bin/luna emit prog.lu [-o out.ll]`
- `luna` finds `compiler/runtime` under `$LUNA_HOME`, or the current directory when it is not set; generated files go to `C/` (git-ignored)
- Check generated IR: `opt-18 -passes=verify C/prog.ll -disable-output`

---

## Global Execution Guardrails

1. STRICTLY NO EMOJIS: Do not use emojis anywhere in generated code, comments, documentation, commit messages, terminal outputs, or user interfaces. Use clean ASCII identifiers, standard markdown, and text prefixes such as `[OK]`, `[ERROR]`, `[WARN]`, `[INFO]`.
2. Preserving Existing Architecture: the compiler and the runtime are written in Luna and built with clang (LLVM 18). Do not introduce C, C++, Rust or extraneous build or runtime dependencies without explicit authorization. libc and libm, called through `extern`, are the only external libraries.
3. Verification Required: Always run `make` and relevant tests (`make test` or `compiler/run_tests.sh <file>`) after modifying code before considering a task completed.
4. Memory Safety: Respect the division between the tracing GC heap (`compiler/runtime/luna_gc.lu`), the box table, and the non-GC `unsafe` memory blocks.
5. Documentation samples: fence Luna code with the `javascript` language tag in markdown. Check every command and sample you document by running it.
6. Do not launch window or audio programs to verify a change; use headless tests.
7. Joke and profane error messages and keyword aliases (`balls`, `grab_balls`, ...) are intentional; never remove or flag them.

---

## Strict UI and Frontend Design Directive: Anti-Vibe-Coding Rules

When building or updating user interfaces (graphical applications, Web/WASM interfaces, tooling dashboards, or developer views), Claude must strictly avoid common AI-generated "vibe-coded" aesthetics. The resulting interface must look deliberate, functional, and built by experienced human systems engineers.

### What Claude Commonly Generates (Forbidden Anti-Patterns)

Claude and typical AI models default to predictable aesthetic tropes that look generic, cheap, and unmistakably AI-generated. The following patterns are strictly prohibited:

1. The Purple/Indigo Gradient Cliché:
   - FORBIDDEN: Glowing purple-to-indigo or violet gradients (`bg-gradient-to-r from-indigo-500 via-purple-500 to-pink-500`), ambient glowing background blobs, or gradient borders on cards.
   - FORBIDDEN: Over-saturated neon accents (electric cyan, hot magenta, lime green) splashed across dark backgrounds with no semantic hierarchy.

2. Floating Eyebrow Badges and Pill Tags:
   - FORBIDDEN: Pill-shaped status chips (`rounded-full bg-indigo-50/10 text-indigo-400 border border-indigo-500/20`) placed above every heading or section title.
   - FORBIDDEN: Sparkle icons, stars, or decorative badges signaling "AI" or "magic".

3. The Cookie-Cutter Three-Card Grid:
   - FORBIDDEN: Repetitive 3-column feature cards with a generic icon inside a rounded square in the top-left corner, followed by a bold title and two lines of filler text.
   - FORBIDDEN: Cards with heavy drop-shadows and subtle glowing borders that lack concrete functional information.

4. Bubbly and Cartoonish Geometry:
   - FORBIDDEN: Exaggerated border radiuses (`rounded-2xl`, `rounded-3xl`, `rounded-full` buttons) on technical tools, developer dashboards, or desktop utilities.
   - FORBIDDEN: Oversized padding that squanders screen real estate and reduces data density.

5. Generic Marketing Copy and AI Slop:
   - FORBIDDEN: Placeholder text such as "Supercharge your workflow", "Unlock next-gen capabilities", "Empower your development", or "All-in-one platform".
   - FORBIDDEN: Arbitrary metric cards displaying fake stats like `+14.2% from last week` with green upward arrows.

6. Lazy Interactions and Unmotivated Animations:
   - FORBIDDEN: Unsolicited bouncy scale-ups on hover (`hover:scale-105 hover:shadow-2xl transition-all duration-300`).
   - FORBIDDEN: Staggered fade-in animations that delay user interaction and make the interface feel sluggish.

7. Typography and Icon Overuse:
   - FORBIDDEN: Relying exclusively on default system Inter without typographical hierarchy or tabular monospacing for data.
   - FORBIDDEN: Scattering Lucide/FontAwesome icons next to every label, button, and header regardless of necessity.
   - FORBIDDEN: Emojis anywhere in headers, buttons, labels, logs, or status indicators.

8. Incomplete State Handling:
   - FORBIDDEN: Styling only the default static state while ignoring hover, pressed, keyboard focus (`:focus-visible`), disabled, empty, skeleton loading, and error states.

### Affirmative UI Craft Standards (Production Engineering Guidelines)

Instead of vibe-coding, implement interfaces adhering to these deliberate engineering standards:

1. High Information Density and Utilitarian Layouts:
   - Model UI after tools like Linear, Raycast, JetBrains, GitHub, and Figma.
   - Prioritize scannability, clear structural grids, and purposeful alignment.
   - Maximize useful screen area without visual clutter.

2. Monochromatic Foundation with Deliberate Accent:
   - Use a disciplined, neutral grayscale foundation (zinc, slate, or true neutral grays) with high contrast ratios.
   - Use a single, deliberate accent color (e.g., precise blue or amber) reserved exclusively for interactive focus, primary actions, or critical states.
   - Keep surface elevation differences subtle and defined by crisp 1px borders rather than diffuse drop shadows.

3. Sharp, Crisp Geometry:
   - Use compact, precise border radiuses: 2px (`rounded-sm`) to 4px (`rounded-md`). Never exceed 6px for desktop and developer utilities.
   - Use 1px borders with explicit border contrast (e.g., `border-zinc-800` on `bg-zinc-900`).

4. Monospaced Precision for Data:
   - Render numbers, memory addresses, benchmarks, execution times, and codes in a monospace font (e.g., JetBrains Mono, Fira Code, or standard system monospace).
   - Use `tabular-nums` (tabular figures) for all numerical metrics to prevent layout jitter during updates.

5. Comprehensive State Design:
   - Normal: Crisp, legible, accessible contrast meeting WCAG AA or AAA.
   - Hover: Subtle background shift (e.g., `bg-zinc-800` to `bg-zinc-750`), not scale transforms.
   - Active/Pressed: Subtle inset or depression effect.
   - Focus-Visible: High-contrast 2px keyboard focus ring with explicit offset for full keyboard accessibility.
   - Disabled: Reduced opacity (40-50%), `cursor-not-allowed`, and pointer-events disabled.
   - Empty State: Practical, informative message with a direct action button, not decorative illustrations.
   - Error State: Clear text explanation with corrective action; never a vague red banner.

---

## Code Style and Architecture Rules

### Compiler (`compiler/*.lu`)
- Pipeline: `lexer.lu`, `parser.lu`, `lint.lu`, `analysis.lu`, then `emit_llvm.lu` (with `ll_state.lu`, `ll_scope.lu`). `lunac.lu` is the driver. `emit_native.lu` is the backend that compiles the runtime.
- The compiler compiles itself, so its source must stay inside the subset it compiles: never use `data`, `from`, `in`, `input`, `print`, `box` or other keywords as identifiers, and write a literal `{` inside a string as `\{`.
- Modules share mutable state through maps (`ST` in `state.lu`), because `use` copies bindings by value. Circular imports are an error.
- The builtin names are the `BUILTINS` list in `compiler/shared.lu`. Each builtin `name` is `lb_name` in `compiler/runtime/`. Add both together.
- Do not change the syntax of the language for speed or convenience; new abilities arrive as builtins and compiler inference (see `docs/plan-aot.md`).
- Semantics never change for speed: integer overflow raises, division by zero raises, conditions must be booleans.

### Runtime (`compiler/runtime/*.lu`)
- Written in the native dialect: one type, the 64-bit word, with memory intrinsics (`ld64`, `st64`, ...), `extern(...)` declarations for libc, and `space`/`table` data. See `docs/internals/lunac.md`.
- Values are 16-byte `LV` cells (tag, payload). Builtins are `lb_<name>(self_p, argc_i, argv_p)` and return a value with `retp`.
- Every runtime object is allocated through `luna_gc.lu`. The collector is conservative and non-moving: it scans the data segment, the stack, registers and registered root ranges. A pointer kept only outside those places is not seen, so register root ranges for any off-heap buffer that holds Luna values. Use `lv_alloc_atomic` for memory without pointers.
- Never allow raw pointers from `unsafe` blocks to be stored inside GC-managed containers (rule R7).
- Error text and format (`Type Error in <file> at line N`, message, source line, hint) are part of the tested behavior; changing them means re-recording `test/expected/`.

### Seeds and bootstrap
- `bootstrap/lunac.ll` and `bootstrap/luna_rt_native.ll` are the only way to build the first compiler. After any change to `compiler/` or `compiler/runtime/`, run `make`, `make seed`, `make` again and `make test`.

### Tests
- Tests are `test/*.lu` programs with recorded stdout, stderr and exit status in `test/expected/`. A file of `# CASE <name>` sections runs each case as its own program. See `docs/testing.md`.

---

## Claude Code Workflow Protocol

When executing tasks in this codebase:
1. Understand First: Read the relevant source files before modifying code.
2. Search Methodically: Use ripgrep or file search to verify symbol definitions across `compiler/`, `compiler/runtime/`, `test/` and `docs/`.
3. Minimal Edits: Make targeted, surgical changes rather than rewriting entire files.
4. Verify Build and Tests: Run `make` and `make test` before presenting final solutions.
5. Zero Emojis: Ensure all messages, summaries, and code comments contain no emojis.
