# CLAUDE.md

Configuration and operational instructions for Claude Code when working in the Luna Language repository.

## Commands

### Build Commands
- Full build: `make` (compiles C core, builds Rust runtimes via Cargo, links `bin/luna`)
- Multi-core build: `make -j$(nproc)`
- Dependency verification: `make check-deps`
- Clean all build artifacts: `make clean` (removes `obj/`, `bin/`, and cleans Cargo workspaces)
- Clean C artifacts only: `make clean-c`

### Test Commands
- Run complete test suite: `make test`
- Run bash test runner directly: `./test_runner.sh`
- Run Rust structured data runtime tests: `make test-rust-data` (or `cargo test --manifest-path rust/data_rt/Cargo.toml`)
- Run Rust unsafe runtime tests: `cargo test --manifest-path rust/unsafe_rt/Cargo.toml`
- Run single test script: `./bin/luna test/test_core.lu`
- Run GC stress verification: `env LUNA_GC_STRESS=1 LUNA_GC_VERIFY=1 ./bin/luna test/test_gc_safety.lu`

### Run Commands
- Run arbitrary script: `./bin/luna <path_to_script.lu>`
- Run default entry script: `make run` (runs `main.lu`)
- Interactive REPL: `make repl` (or `./bin/luna`)
- Run Car Game demo: `make run-cargame` (runs `cargame/cargame.lu`)
- Run Music Player demo: `make run-music` (runs `musicplayer/music.lu`)

---

## Global Execution Guardrails

1. STRICTLY NO EMOJIS: Do not use emojis anywhere in generated code, comments, documentation, commit messages, terminal outputs, or user interfaces. Use clean ASCII identifiers, standard markdown, and text prefixes such as `[OK]`, `[ERROR]`, `[WARN]`, `[INFO]`.
2. Preserving Existing Architecture: Luna is written in C11 with Rust runtime libraries linked statically. Do not introduce C++ or extraneous runtime dependencies without explicit authorization.
3. Verification Required: Always run `make` and relevant tests (`make test` or target tests) after modifying code before considering a task completed.
4. Memory Safety: Respect the division between the bump-allocated AST arena, the tracing GC heap, and the non-GC unsafe memory blocks.

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

### C11 Core (`src/`, `include/`, `gui/`, `vm/`)
- Standard: ISO C11 (`-std=c11`).
- Compiler Flags: Strict warnings (`-Wall -Wextra -Wno-unused-parameter`). Code must compile cleanly.
- Memory Allocation:
  - AST nodes must be allocated via the bump arena (`src/arena.c`). Do not call `malloc` directly for AST elements.
  - Runtime GC values must be registered with the tri-color collector (`src/gc.c`).
  - Maintain SATB (Snapshot-at-the-Beginning) write barriers: whenever an object field reference is overwritten while GC is in mark phase, shade the old reference gray.
  - Never allow raw pointers from `unsafe` blocks to be stored directly inside GC-managed containers without tracking.
- String Handling: Use string interning (`src/intern.c`) for identifiers and property names to enable O(1) pointer-equality checks.
- Header Guards: Use standard `#ifndef LUNA_<NAME>_H` guards in all header files.

### Rust Runtimes (`rust/unsafe_rt/`, `rust/data_rt/`)
- Edition: Rust 2021.
- FFI Interfaces: All exposed C-callable functions must be marked `#[no_mangle] pub extern "C"`.
- Static Linking: Libraries are compiled as `staticlib` (`libluna_memory_rt.a`, `libluna_data_rt.a`) and placed in `lib/`.
- Unsafe Rules: Enforce the 12 memory safety rules in `unsafe_rt` (no use-after-free, bounds enforcement, alignment checks, lifetime validation).

### Bytecode VM (`vm/`)
- Stack Discipline: Ensure VM stack balance across opcode dispatch in `vm/luna_vm.c`.
- Compiler Pass: The AST-to-bytecode compiler (`vm/luna_compiler.c`) must emit verified jump offsets and constant table indices.

---

## Claude Code Workflow Protocol

When executing tasks in this codebase:
1. Understand First: Read relevant source and header files before modifying code.
2. Search Methodically: Use ripgrep or file search to verify symbol definitions across `src/`, `include/`, `gui/`, and `rust/`.
3. Minimal Edits: Make targeted, surgical changes rather than rewriting entire files.
4. Verify Build and Tests: Run `make` and `./test_runner.sh` before presenting final solutions.
5. Zero Emojis: Ensure all messages, summaries, and code comments contain no emojis.
