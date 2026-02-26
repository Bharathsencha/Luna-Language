# Luna JIT Compiler Game Plan

Building a JIT compiler is the holy grail of language design — it's how JavaScript (V8) and LuaJIT hit near-C speeds. Don't jump straight from the AST-evaluator to raw machine code though. That's a massive leap. Here's how to do it in stages.

---

## Phase 1: Bytecode VM (The Pre-Requisite)

Right now, Luna walks an AST. If there's a loop running 1,000 times, a C function physically traverses the tree 1,000 times — that tree-walk is the bottleneck.

The fix is two parts. First, write a **compiler** that flattens the AST into a sequential array of simple 1-byte instructions — Bytecode. Something like `LOAD_CONST 1`, `LOAD_CONST 2`, `ADD`, `STORE x`. Second, build a **VM** — which is really just a blazing fast `while (true)` loop with a big `switch` statement in C that reads and executes those Bytecode instructions sequentially.

This alone will make standard loops and conditionals **3x–5x faster** than the current tree-walker. Luna becomes a Bytecode VM, like Python or Java.

---

## Phase 2: Profiling (The "Just-In-Time" Part)

A JIT doesn't compile everything — that would be slow upfront and wasteful. It only compiles what actually matters.

While the VM runs, it keeps a **heat map counter** on every function and loop. A function called once? Just run the Bytecode normally. A `while` loop that hits 1,000 executions? The VM flags it as **Hot Code** and prepares to do something about it.

---

## Phase 3: Machine Code Emission (The Hard Part)

This is where it gets real.

Use `mmap` on Linux to grab a chunk of RAM and mark it as **executable** (`PROT_EXEC`). Then write a backend that takes the Bytecode for that hot loop and translates it into literal hex bytes representing raw **x86_64 or ARM assembly**. You're writing things like `0x48 0x01 0xD0` directly into memory — that's `add rax, rdx` at the bare metal level.

---

## Phase 4: The Handoff

Once the VM finishes writing the assembly into that executable memory block, it changes course. Instead of going back to the Bytecode `switch` statement, it **jumps the CPU's instruction pointer directly into that block**. The processor takes over and runs the loop at native hardware speed — the Luna interpreter is completely bypassed.

---

## The Full Flow

Text → Tokens → AST → Bytecode → VM runs it → VM spots a hot loop → translates it to raw machine code → CPU executes it natively.

That's exactly how V8 makes JavaScript fast enough to run complex 3D games in a browser. It's doable. Just build it one phase at a time.