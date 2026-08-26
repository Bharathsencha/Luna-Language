#!/usr/bin/env python3
"""GC benchmark: runs Luna and Go workloads, records GC pause stats to CSV.

Usage:
    python3 test_gc/gc_bench.py            # one run per benchmark
    LUNA_GC_BENCH_RUNS=3 python3 ...       # average of N runs

Appends a timestamped block of rows to test_gc/gc_benchmark_results.csv and
prints a comparison table.
"""
import csv
import os
import re
import subprocess
import sys
import time

# Unbuffered stdout so progress is visible when redirected (make test-gc, logs).
try:
    sys.stdout.reconfigure(line_buffering=True)
except AttributeError:
    pass


def log(msg):
    print(msg, flush=True)

BENCHMARKS = ["alloc_heavy", "long_live", "cycles", "strings"]
RUNS = int(os.environ.get("LUNA_GC_BENCH_RUNS", "1"))
CSV_PATH = os.path.join(os.path.dirname(__file__), "gc_benchmark_results.csv")
LUNA_BIN = "./bin/luna"

USER_RE = re.compile(r"User time \(seconds\):\s*([\d.]+)")
RSS_RE = re.compile(r"Maximum resident set size \(kbytes\):\s*(\d+)")
LUNA_GC_RE = re.compile(r"LUNA_GC_STATS,([\d.]+),([\d.]+),(\d+)")
# gctrace: "gc N @t s%: A+B+C ms clock, ..."  A and C are the STW pauses.
GCTRACE_RE = re.compile(r"gc \d+ @[\d.]+s \d+%: ([\d.]+)\+[\d.]+\+([\d.]+) ms clock")


def run_timed(cmd, env=None):
    """Run cmd under /usr/bin/time -v, return (user_s, rss_mb, stderr_text)."""
    full = ["/usr/bin/time", "-v"] + cmd
    proc = subprocess.run(full, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True, env=env)
    user = 0.0
    rss_mb = 0.0
    m = USER_RE.search(proc.stderr)
    if m:
        user = float(m.group(1))
    m = RSS_RE.search(proc.stderr)
    if m:
        rss_mb = float(m.group(1)) / 1024.0
    return user, rss_mb, proc.stderr


def bench_luna(name):
    env = os.environ.copy()
    env["LUNA_GC_STATS"] = "1"
    user, rss, err = run_timed([LUNA_BIN, f"test_gc/{name}.lu"], env)
    gc_total = gc_max = 0.0
    gc_events = 0
    m = LUNA_GC_RE.search(err)
    if m:
        gc_total = float(m.group(1))
        gc_max = float(m.group(2))
        gc_events = int(m.group(3))
    return {"user": user, "rss": rss, "gc_total": gc_total,
            "gc_max": gc_max, "gc_events": gc_events}


def bench_go(name):
    exe = f"test_gc/{name}_go"
    # Build if missing.
    if not os.path.exists(exe):
        subprocess.run(["go", "build", "-o", exe, f"go/{name}.go"], check=True)
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


def average(runs):
    out = {}
    for key in runs[0]:
        out[key] = sum(r[key] for r in runs) / len(runs)
    return out


def main():
    if not os.path.exists(LUNA_BIN):
        print(f"error: {LUNA_BIN} not found; run `make` first", file=sys.stderr)
        return 1

    stamp = time.strftime("%Y-%m-%d %H:%M:%S")
    rows = []
    log(f"GC benchmark ({RUNS} run(s))  —  {stamp}")
    log("=" * 78)
    for name in BENCHMARKS:
        luna_runs = []
        for r in range(RUNS):
            log(f"[{name}] Luna run {r + 1}/{RUNS}...")
            luna_runs.append(bench_luna(name))
        luna = average(luna_runs)
        go_runs = []
        for r in range(RUNS):
            log(f"[{name}] Go run {r + 1}/{RUNS}...")
            go_runs.append(bench_go(name))
        go = average(go_runs)
        rows.append([stamp, name, "Luna", f"{luna['user']:.3f}", f"{luna['rss']:.2f}",
                     f"{luna['gc_total']:.3f}", f"{luna['gc_max']:.3f}",
                     f"{int(luna['gc_events'])}"])
        rows.append([stamp, name, "Go", f"{go['user']:.3f}", f"{go['rss']:.2f}",
                     "-", f"{go['gc_max']:.3f}", f"{int(go['gc_events'])}"])
        log(f"{name:12s} Luna: user={luna['user']:.3f}s rss={luna['rss']:.1f}MB "
            f"gc_total={luna['gc_total']:.2f}ms gc_max={luna['gc_max']:.3f}ms "
            f"events={int(luna['gc_events'])}")
        log(f"{name:12s} Go  : user={go['user']:.3f}s rss={go['rss']:.1f}MB "
            f"gc_max={go['gc_max']:.3f}ms events={int(go['gc_events'])}")
        log("-" * 78)

    header = ["Timestamp", "Benchmark", "Language", "User Time (s)", "Max RSS (MB)",
              "GC Total (ms)", "GC Max Pause (ms)", "GC Events"]
    new_file = not os.path.exists(CSV_PATH)
    with open(CSV_PATH, "a", newline="") as f:
        w = csv.writer(f)
        if new_file:
            w.writerow(header)
        w.writerows(rows)
    log(f"Appended {len(rows)} rows to {CSV_PATH}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
