# Technical Discussion Log and Architecture Decision Records (ADRs)

Persistent technical ledger for the Luna programming language repository. This document records architectural decisions, technical trade-offs, and session handoff notes for Claude Code and development agents.

---

## 1. Operating Guidelines for this Document

- Role: Acts as the persistent memory between separate Claude Code sessions. When a significant technical decision is made, an RFC is evaluated, or a multi-turn task concludes, update this document.
- Invariant: STRICTLY NO EMOJIS in this document, in records, or in session entries. Use standard ASCII markers like `[STATUS: ACCEPTED]`, `[STATUS: PROPOSED]`, `[x]`, `[ ]`.

---

## 2. Architecture Decision Records (ADRs)

### ADR-001: Hybrid C11 Core with Statically Linked Rust Safety Runtimes
- Date: Initial Architecture
- Status: [STATUS: ACCEPTED]
- Context:
  Scripting languages traditionally face a trade-off between bare-metal speed (often written in C or C++) and safety verification (to prevent segfaults, use-after-free, and container contamination). Writing the entire runtime in Rust introduces FFI friction for low-level SIMD intrinsics and C OpenGL/audio libraries, while writing the entire runtime in pure C leaves memory safety difficult to guarantee mathematically.
- Decision:
  Implement the core execution engine, parser, AST, interpreter, and graphics backends in C11. Offload complex memory safety verification to isolated, statically linked Rust libraries (`rust/unsafe_rt` and `rust/data_rt`).
- Consequences:
  - High performance: C11 handles the tight execution loops, SIMD operations, and graphics batching without FFI overhead.
  - Provable safety: Rust enforces 12 memory invariants for manual pointer operations in `unsafe { ... }` blocks and verifies structured data tags (`bloc`, `box`, `template`).
  - Build dependency: Building Luna requires `cargo` in addition to `gcc` and `nasm`.

### ADR-002: Arena Bump Allocation for AST Outside GC Domain
- Date: Initial Architecture
- Status: [STATUS: ACCEPTED]
- Context:
  Tree-walk interpreters and AST compilers allocate millions of small AST nodes during parsing and execution. Subjecting these nodes to a tracing garbage collector causes high GC overhead, cache fragmentation, and long pause times.
- Decision:
  Allocate all AST nodes and parsed identifiers in linear, bump-allocated memory arenas (`src/arena.c`). Keep the AST arena strictly isolated from the tracing garbage collector.
- Consequences:
  - Allocation cost is O(1) pointer addition (`arena->offset += size`).
  - AST nodes are never individually freed; entire pages are reclaimed simultaneously when execution completes or during arena reset.
  - The GC never traverses or marks AST nodes, dramatically reducing tracing pause durations.

### ADR-003: Incremental Tri-Color Garbage Collector with SATB Write Barriers
- Date: Initial Architecture
- Status: [STATUS: ACCEPTED]
- Context:
  Stop-the-world garbage collectors create visible pauses that ruin real-time audio and 60 FPS graphics applications. Luna targets creative coding, game development, and interactive scripting.
- Decision:
  Implement an incremental tri-color tracing garbage collector (`src/gc.c`, `src/gc_visit.c`) using a Snapshot-at-the-Beginning (SATB) write barrier and generational tracking.
- Consequences:
  - GC marking and sweeping are interleaved in small bounded increments during memory allocations.
  - Maximum pause time is kept under 0.15ms on benchmark workloads.
  - Invariant: Whenever an existing pointer reference on the heap is overwritten during a mark phase, the old reference must be shaded gray via the SATB write barrier to prevent premature collection.

### ADR-004: Direct Vendored OpenGL 3.3 Batch Renderer and Miniaudio Integration
- Date: Initial Architecture
- Status: [STATUS: ACCEPTED]
- Context:
  External package managers and dynamic library dependencies frequently cause build breakage across different Linux distributions and developer machines.
- Decision:
  Vendor all graphics and audio dependencies directly inside the repository: static GLFW (`lib/libglfw3.a`), miniaudio (`gui/miniaudio.h`), and STB image/font libraries (`gui/stb_*.h`). Use a batched 2D vertex renderer supporting up to 65,536 vertices per draw call.
- Consequences:
  - Zero external third-party package dependencies required at runtime beyond system OpenGL and X11 libraries.
  - Deterministic builds and minimal driver overhead.
  - High rendering throughput for games, particle simulations, and visualizers.

### ADR-005: Strict Anti-Vibe-Coding Policy for Developer Tools and UI
- Date: 2026-10-16
- Status: [STATUS: ACCEPTED]
- Context:
  AI-generated interfaces frequently suffer from recognizable visual tropes: purple/indigo gradients, ambient glowing backdrop blobs, floating pill badges with sparkle icons, generic 3-column card grids with icons in top-left rounded squares, oversized bubbly radiuses, and generic AI copywriting. These patterns look amateurish and unsuited for production systems engineering.
- Decision:
  Mandate a strict anti-vibe-coding standard across all graphical applications, developer tools, documentation, and user interfaces built for or with Luna. Prohibit all listed AI tropes. Enforce high information density, monochromatic zinc/slate palettes, crisp 1px borders, subtle 2px-4px radiuses, monospace tabular typography for metrics, and complete state design (hover, pressed, focus-visible, disabled, skeleton, error).
- Consequences:
  - Interfaces remain professional, accessible, and aligned with tools like Linear, Raycast, and JetBrains.
  - Consistent visual craft across all Luna demos and tooling.

---

## 3. Active Technical RFCs and Design Proposals

### RFC-101: Bytecode VM Optimization and Register-Based Evaluation
- Status: [STATUS: UNDER DISCUSSION]
- Context:
  Luna currently has both a tree-walk interpreter (`src/interpreter.c`) and a stack-based bytecode VM (`vm/luna_vm.c`).
- Proposal:
  Investigate transitioning the stack-based VM to a register-based VM or implementing threaded code dispatch (computed `goto`) in GCC to reduce dispatch loop overhead.
- Trade-offs:
  Register-based dispatch requires more complex compiler passes but reduces instruction count and stack manipulation overhead by 30-45%.

### RFC-102: Module System Isolation and Hermetic Import Resolution
- Status: [STATUS: UNDER DISCUSSION]
- Context:
  Module imports (`use {foo} from "file.lu"`) in `src/module_runtime.c` currently rely on relative filesystem resolution.
- Proposal:
  Introduce path normalization, circular dependency detection, and a virtual namespace registry for sandboxed scripts.

---

## 4. Session Continuity and Handoff Ledger

Use this section to record progress between Claude Code invocations.

### Entry: 2026-10-16 - Environment and Memory Documentation Setup
- Operator: Antigravity Agent
- Objectives:
  Establish canonical Claude Code configuration, architecture context, prompt playbooks, and decision logs.
- Deliverables:
  - `CLAUDE.md` (and aliases `claude.md`, `calude.md`): Commands, build instructions, anti-vibe-coding UI directives, zero emoji rules.
  - `context.md` (and alias `contex.md`): Deep technical architecture, memory domain separation, subsystem responsibilities, toolchain specs.
  - `prompt.md` (and alias `pormt.md`): Prompt templates for language features, debugging, performance, and non-vibe-coded UI development.
  - `discussion.md`: ADRs 001 through 005, active RFCs, and session ledger.
- Verification:
  - Verified symlinks and file integrity.
  - Verified strict absence of emojis across all documentation files.
- Active Working State:
  Repository is fully configured for agentic workflows with Claude Code.
