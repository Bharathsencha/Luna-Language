#!/usr/bin/env python3
"""Unified GC benchmark: Luna vs Go, Luna vs Java (G1), and Luna stress tests.

Runs four phases:
  1. Luna vs Go        — 4 core benchmarks, N runs each
  2. Luna vs Java (G1) — 8 benchmarks, N runs each (single JVM invocation)
  3. Luna stress 3x    — 4 core benchmarks, 1 run, reduced heap
  4. Luna stress 8x    — 4 core benchmarks, 1 run, aggressive reduced heap

Outputs a single append-only CSV (test_gc/gc_all_results.csv) and a summary table.

Env vars:
  LUNA_GC_BENCH_RUNS=N   Normal run count per language (default: 3)
  LUNA_GC_BENCH_CSV=path Override output CSV path
"""
import csv
import os
import re
import shutil
import subprocess
import sys
import time

try:
    sys.stdout.reconfigure(line_buffering=True)
except AttributeError:
    pass

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LUNA_BIN = os.path.join(ROOT, "bin", "luna")
RUNS = int(os.environ.get("LUNA_GC_BENCH_RUNS", "3"))
CSV_PATH = os.environ.get(
    "LUNA_GC_BENCH_CSV",
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "gc_all_results.csv"),
)

CORE_BENCHMARKS = ["alloc_heavy", "long_live", "cycles", "strings"]
ALL_BENCHMARKS = ["alloc_heavy", "long_live", "cycles", "strings",
                  "binary_trees", "map_churn", "object_graph", "concurrent_map"]

JAVA_DIR = os.path.join(ROOT, "java")
JAVA_OUT = os.path.join(JAVA_DIR, "out")
JAVA_OPTS = ["-Xms256m", "-Xmx256m", "-XX:+UseG1GC", "-Xlog:gc:stderr:time,uptime"]

# --- Regex patterns ---
USER_RE = re.compile(r"User time \(seconds\):\s*([\d.]+)")
RSS_RE = re.compile(r"Maximum resident set size \(kbytes\):\s*(\d+)")
LUNA_GC_RE = re.compile(r"LUNA_GC_STATS,([\d.]+),([\d.]+),(\d+)")
GCTRACE_RE = re.compile(r"gc \d+ @[\d.]+s \d+%: ([\d.]+)\+[\d.]+\+([\d.]+) ms clock")
JAVA_BENCH_RE = re.compile(r"BENCH (\w+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)")
JAVA_PAUSE_RE = re.compile(r"\[(\d+\.\d+)s\].*Pause.*?([\d.]+)ms")


# ──────────────────────────────────────────────────────────────────────────────
# Helpers
# ──────────────────────────────────────────────────────────────────────────────

def log(msg):
    print(msg, flush=True)


def banner(text):
    log("")
    log("=" * 78)
    log(f"  {text}")
    log("=" * 78)


def run_timed(cmd, env=None):
    """Run cmd under /usr/bin/time -v, return (user_s, rss_mb, stderr_text)."""
    full = ["/usr/bin/time", "-v"] + cmd
    proc = subprocess.run(full, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True, env=env)
    user = rss_mb = 0.0
    m = USER_RE.search(proc.stderr)
    if m:
        user = float(m.group(1))
    m = RSS_RE.search(proc.stderr)
    if m:
        rss_mb = float(m.group(1)) / 1024.0
    return user, rss_mb, proc.stderr


def avg_dicts(rows):
    """Average a list of dicts (same keys) into one dict."""
    if not rows:
        return {}
    keys = rows[0].keys()
    return {k: sum(r[k] for r in rows) / len(rows) for k in keys}


# ──────────────────────────────────────────────────────────────────────────────
# Luna runner
# ──────────────────────────────────────────────────────────────────────────────

def bench_luna(name, extra_env=None):
    """Run one Luna benchmark, return dict with user/rss/gc metrics."""
    env = os.environ.copy()
    env["LUNA_GC_STATS"] = "1"
    if extra_env:
        env.update(extra_env)
    user, rss, err = run_timed([LUNA_BIN, os.path.join("test_gc", f"{name}.lu")], env)
    gc_total = gc_max = 0.0
    gc_events = 0
    m = LUNA_GC_RE.search(err)
    if m:
        gc_total = float(m.group(1))
        gc_max = float(m.group(2))
        gc_events = int(m.group(3))
    return {"user": user, "rss": rss, "gc_total": gc_total,
            "gc_max": gc_max, "gc_events": gc_events}


# ──────────────────────────────────────────────────────────────────────────────
# Go runner
# ──────────────────────────────────────────────────────────────────────────────

def bench_go(name):
    """Run one Go benchmark, return dict with user/rss/gc_max."""
    exe = os.path.join("test_gc", f"{name}_go")
    go_src = os.path.join("go", f"{name}.go")
    if not os.path.exists(exe):
        r = subprocess.run(["go", "build", "-o", exe, go_src], cwd=ROOT,
                           capture_output=True, text=True)
        if r.returncode != 0:
            log(f"  [go build failed: {r.stderr.strip()}]")
            return None
    env = os.environ.copy()
    env["GODEBUG"] = "gctrace=1"
    user, rss, err = run_timed([exe], env)
    pauses = []
    for m in GCTRACE_RE.finditer(err):
        pauses.append(float(m.group(1)))
        pauses.append(float(m.group(2)))
    gc_max = max(pauses) if pauses else 0.0
    return {"user": user, "rss": rss, "gc_total": 0.0,
            "gc_max": gc_max, "gc_events": len(pauses) // 2}


# ──────────────────────────────────────────────────────────────────────────────
# Java runner
# ──────────────────────────────────────────────────────────────────────────────

def compile_java():
    """Compile GcBench.java if needed. Returns True on success."""
    cls = os.path.join(JAVA_OUT, "GcBench.class")
    if os.path.exists(cls):
        return True
    if not shutil.which("javac"):
        return False
    os.makedirs(JAVA_OUT, exist_ok=True)
    r = subprocess.run(
        ["javac", "-d", JAVA_OUT, os.path.join(JAVA_DIR, "GcBench.java")],
        capture_output=True, text=True, cwd=ROOT,
    )
    if r.returncode != 0:
        log(f"  [javac failed: {r.stderr.strip()}]")
        return False
    return True


def bench_java_all():
    """Run all Java benchmarks in a single JVM invocation.

    Returns dict: benchmark_name -> {user, rss, gc_total, gc_max, gc_events}
    """
    if not shutil.which("java"):
        return None
    if not compile_java():
        return None

    proc = subprocess.run(
        ["java"] + JAVA_OPTS + ["-cp", JAVA_OUT, "GcBench"],
        capture_output=True, text=True, cwd=ROOT,
    )
    if proc.returncode != 0:
        log(f"  [java failed: {proc.stderr.strip()[:200]}]")
        return None

    # Parse GC pauses from stderr
    pauses = []
    for m in JAVA_PAUSE_RE.finditer(proc.stderr):
        t_ms = float(m.group(1)) * 1000.0
        pauses.append((t_ms, float(m.group(2))))

    results = {}
    for m in JAVA_BENCH_RE.finditer(proc.stdout):
        name = m.group(1)
        user_ms = int(m.group(3))
        gc_count = int(m.group(4))
        gc_total = int(m.group(5))
        up0 = int(m.group(6))
        up1 = int(m.group(7))
        gc_max = max((p for t, p in pauses if up0 <= t <= up1), default=0.0)
        results[name] = {
            "user": user_ms / 1000.0, "rss": 0.0,
            "gc_total": gc_total, "gc_max": gc_max, "gc_events": gc_count,
        }
    return results


# ──────────────────────────────────────────────────────────────────────────────
# Phases
# ──────────────────────────────────────────────────────────────────────────────

def phase_luna_vs_go():
    """Phase 1: Luna vs Go on 4 core benchmarks, RUNS runs each."""
    banner("Phase 1: Luna vs Go  (4 benchmarks, {} run(s) each)".format(RUNS))
    rows = []
    header_fmt = f"{'benchmark':<14} {'lang':<5} {'user(s)':>8} {'gc_max(ms)':>11} {'gc_total(ms)':>13} {'events':>7} {'rss(MB)':>9}"
    log(header_fmt)
    log("-" * 78)

    for name in CORE_BENCHMARKS:
        luna_runs = [bench_luna(name) for _ in range(RUNS)]
        go_runs = []
        for _ in range(RUNS):
            r = bench_go(name)
            if r:
                go_runs.append(r)
        luna = avg_dicts(luna_runs)
        go = avg_dicts(go_runs) if go_runs else {}

        def row(phase, lang, d):
            return {
                "Phase": phase, "Benchmark": name, "Language": lang,
                "User Time (s)": f"{d['user']:.3f}",
                "Max RSS (MB)": f"{d['rss']:.2f}",
                "GC Total (ms)": f"{d['gc_total']:.3f}",
                "GC Max Pause (ms)": f"{d['gc_max']:.3f}",
                "GC Events": str(int(d.get("gc_events", 0))),
            }

        rows.append(row("normal", "Luna", luna))
        if go:
            rows.append(row("normal", "Go", go))

        log(f"{name:<14} {'Luna':<5} {luna['user']:>8.3f} {luna['gc_max']:>11.3f} "
            f"{luna['gc_total']:>13.3f} {int(luna.get('gc_events',0)):>7} {luna['rss']:>9.1f}")
        if go:
            log(f"{'':14} {'Go':<5} {go['user']:>8.3f} {go['gc_max']:>11.3f} "
                f"{go['gc_total']:>13.3f} {int(go.get('gc_events',0)):>7} {go['rss']:>9.1f}")
        log("-" * 78)

    return rows


def phase_luna_vs_java():
    """Phase 2: Luna vs Java (G1) on 8 benchmarks, RUNS runs each."""
    banner("Phase 2: Luna vs Java G1  (8 benchmarks, {} run(s) each)".format(RUNS))

    if not shutil.which("java") or not shutil.which("javac"):
        log("  Java not found — skipping phase 2.")
        return []

    log("  Running Java (single JVM, all benchmarks)...")
    java_all = bench_java_all()
    if not java_all:
        log("  Java execution failed — skipping phase 2.")
        return []

    rows = []
    header_fmt = f"{'benchmark':<16} {'lang':<5} {'user(s)':>8} {'gc_max(ms)':>11} {'gc_total(ms)':>13} {'events':>7} {'rss(MB)':>9}"
    log(header_fmt)
    log("-" * 78)

    for name in ALL_BENCHMARKS:
        luna_runs = [bench_luna(name) for _ in range(RUNS)]
        luna = avg_dicts(luna_runs)
        j = java_all.get(name, {})

        def row(phase, lang, d):
            return {
                "Phase": phase, "Benchmark": name, "Language": lang,
                "User Time (s)": f"{d['user']:.3f}",
                "Max RSS (MB)": f"{d['rss']:.2f}",
                "GC Total (ms)": f"{d['gc_total']:.3f}",
                "GC Max Pause (ms)": f"{d['gc_max']:.3f}",
                "GC Events": str(int(d.get("gc_events", 0))),
            }

        rows.append(row("java", "Luna", luna))
        if j:
            rows.append(row("java", "Java (G1)", j))

        log(f"{name:<16} {'Luna':<5} {luna['user']:>8.3f} {luna['gc_max']:>11.3f} "
            f"{luna['gc_total']:>13.3f} {int(luna.get('gc_events',0)):>7} {luna['rss']:>9.1f}")
        if j:
            log(f"{'':16} {'Java':<5} {j['user']:>8.3f} {j['gc_max']:>11.3f} "
                f"{j['gc_total']:>13.3f} {int(j.get('gc_events',0)):>7} {'—':>9}")
        log("-" * 78)

    return rows


def phase_stress(label, env_overrides):
    """Run Luna under stress on 4 core benchmarks, 1 run each."""
    banner(f"Luna stress ({label})  —  4 benchmarks, 1 run each")
    rows = []
    log(f"  env: {' '.join(f'{k}={v}' for k, v in env_overrides.items())}")
    log("-" * 78)

    for name in CORE_BENCHMARKS:
        r = bench_luna(name, extra_env=env_overrides)
        row = {
            "Phase": label, "Benchmark": name, "Language": "Luna",
            "User Time (s)": f"{r['user']:.3f}",
            "Max RSS (MB)": f"{r['rss']:.2f}",
            "GC Total (ms)": f"{r['gc_total']:.3f}",
            "GC Max Pause (ms)": f"{r['gc_max']:.3f}",
            "GC Events": str(int(r['gc_events'])),
        }
        rows.append(row)
        log(f"  {name:<14} user={r['user']:.3f}s  gc_max={r['gc_max']:.3f}ms  "
            f"gc_total={r['gc_total']:.2f}ms  events={int(r['gc_events'])}  rss={r['rss']:.1f}MB")
    log("-" * 78)
    return rows


# ──────────────────────────────────────────────────────────────────────────────
# Summary
# ──────────────────────────────────────────────────────────────────────────────

def print_summary(all_rows):
    """Print a compact summary grouped by phase."""
    banner("SUMMARY")
    phases = []
    seen = set()
    for r in all_rows:
        key = r["Phase"]
        if key not in seen:
            phases.append(key)
            seen.add(key)

    log(f"{'Phase':<12} {'Benchmark':<16} {'Language':<10} {'User(s)':>8} "
        f"{'GC Max(ms)':>11} {'GC Total(ms)':>13} {'RSS(MB)':>9}")
    log("-" * 78)
    for phase in phases:
        phase_rows = [r for r in all_rows if r["Phase"] == phase]
        for r in phase_rows:
            log(f"{r['Phase']:<12} {r['Benchmark']:<16} {r['Language']:<10} "
                f"{float(r['User Time (s)']):>8.3f} "
                f"{float(r['GC Max Pause (ms)']):>11.3f} "
                f"{float(r['GC Total (ms)']):>13.3f} "
                f"{float(r['Max RSS (MB)']):>9.1f}")
        log("-" * 78)


# ──────────────────────────────────────────────────────────────────────────────
# CSV
# ──────────────────────────────────────────────────────────────────────────────

CSV_HEADER = ["Timestamp", "Phase", "Benchmark", "Language",
              "User Time (s)", "Max RSS (MB)", "GC Total (ms)",
              "GC Max Pause (ms)", "GC Events"]


def write_csv(all_rows):
    stamp = time.strftime("%Y-%m-%d %H:%M:%S")
    new_file = not os.path.exists(CSV_PATH)
    with open(CSV_PATH, "a", newline="") as f:
        w = csv.writer(f)
        if new_file:
            w.writerow(CSV_HEADER)
        for r in all_rows:
            w.writerow([stamp, r["Phase"], r["Benchmark"], r["Language"],
                        r["User Time (s)"], r["Max RSS (MB)"], r["GC Total (ms)"],
                        r["GC Max Pause (ms)"], r["GC Events"]])
    log(f"Appended {len(all_rows)} rows to {CSV_PATH}")


# ──────────────────────────────────────────────────────────────────────────────
# Main
# ──────────────────────────────────────────────────────────────────────────────

def main():
    if not os.path.exists(LUNA_BIN):
        print(f"error: {LUNA_BIN} not found; run `make` first", file=sys.stderr)
        return 1

    start = time.time()
    stamp = time.strftime("%Y-%m-%d %H:%M:%S")
    log(f"Luna GC — Unified Benchmark Suite  —  {stamp}")
    log(f"  Runs per language (normal): {RUNS}")

    all_rows = []

    # Phase 1: Luna vs Go
    all_rows.extend(phase_luna_vs_go())

    # Phase 2: Luna vs Java
    all_rows.extend(phase_luna_vs_java())

    # Phase 3: Luna stress 3x  (heap_limit / 3, young_limit / 3)
    all_rows.extend(phase_stress("stress_3x", {
        "LUNA_GC_STRESS": "1",
        "LUNA_GC_INITIAL_HEAP_LIMIT": "1365333",
        "LUNA_GC_YOUNG_LIMIT": "559240",
    }))

    # Phase 4: Luna stress 8x  (heap_limit / 8, young_limit / 8)
    all_rows.extend(phase_stress("stress_8x", {
        "LUNA_GC_STRESS": "1",
        "LUNA_GC_INITIAL_HEAP_LIMIT": "524288",
        "LUNA_GC_YOUNG_LIMIT": "65536",
    }))

    elapsed = time.time() - start

    print_summary(all_rows)
    write_csv(all_rows)

    log(f"\nTotal time: {elapsed:.1f}s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
