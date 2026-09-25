#!/usr/bin/env python3
"""Python vs Luna GC benchmark — 3-run averages, summary CSV.

One row per benchmark per language:
    Timestamp, Benchmark, Language, User Time (s), GC Max Pause (ms),
    GC Total (ms), Max RSS (MB)

Metrics for Python come from two sources:
  - user time / RSS:  /usr/bin/time -v (same as the Go/Java drivers)
  - GC stats:         PY_GC_STATS printed at exit by python/gcstats.py,
                      which times every cyclic-GC collection.  Refcount
                      frees are instantaneous and never appear as a pause.

Usage: make test-py
Output: python/python_vs_luna.csv
"""
import csv
import os
import re
import subprocess
import sys
import time

try:
    sys.stdout.reconfigure(line_buffering=True)
except AttributeError:
    pass


def log(msg):
    print(msg, flush=True)


BENCHMARKS = ["alloc_heavy", "long_live", "cycles", "strings",
              "binary_trees", "map_churn", "object_graph", "concurrent_map"]
RUNS = 3
CSV_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        os.path.pardir, "python", "python_vs_luna.csv")
LUNA_BIN = "./bin/luna"
PYTHON = "python3"

USER_RE = re.compile(r"User time \(seconds\):\s*([\d.]+)")
RSS_RE = re.compile(r"Maximum resident set size \(kbytes\):\s*(\d+)")
LUNA_GC_RE = re.compile(r"LUNA_GC_STATS,([\d.]+),([\d.]+),(\d+)")
PY_GC_RE = re.compile(r"PY_GC_STATS,(\d+),(\d+),([\d.]+),([\d.]+)")


def run_timed(cmd, env=None):
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
    return user, rss_mb, proc.stdout + proc.stderr


def bench_luna(name):
    env = os.environ.copy()
    env["LUNA_GC_STATS"] = "1"
    user, rss, out = run_timed([LUNA_BIN, f"test_gc/{name}.lu"], env)
    gc_total = gc_max = 0.0
    m = LUNA_GC_RE.search(out)
    if m:
        gc_total = float(m.group(1))
        gc_max = float(m.group(2))
    return user, rss, gc_total, gc_max


def bench_python(name):
    env = os.environ.copy()
    user, rss, out = run_timed([PYTHON, os.path.join("python", f"{name}.py")], env)
    gc_total = gc_max = 0.0
    m = PY_GC_RE.search(out)
    if m:
        gc_total = float(m.group(4))
        gc_max = float(m.group(3))
    return user, rss, gc_total, gc_max


def avg(vals):
    return sum(vals) / len(vals) if vals else 0.0


def main():
    if not os.path.exists(LUNA_BIN):
        print(f"error: {LUNA_BIN} not found; run `make` first", file=sys.stderr)
        return 1

    csv_dir = os.path.dirname(CSV_PATH)
    if not os.path.isdir(csv_dir):
        os.makedirs(csv_dir, exist_ok=True)

    stamp = time.strftime("%Y-%m-%d %H:%M:%S")
    rows = []
    log(f"Python vs Luna GC — {RUNS}-run averages — {stamp}")
    log("=" * 72)
    print(f"{'benchmark':<12} {'lang':<6} {'user(s)':>8} {'gc_max(ms)':>11} {'gc_total(ms)':>13} {'rss(MB)':>9}")

    for name in BENCHMARKS:
        lr = [bench_luna(name) for _ in range(RUNS)]
        pr = [bench_python(name) for _ in range(RUNS)]

        luna = [avg([r[i] for r in lr]) for i in range(4)]
        py = [avg([r[i] for r in pr]) for i in range(4)]

        rows.append([stamp, name, "Luna", f"{luna[0]:.3f}", f"{luna[3]:.3f}", f"{luna[2]:.3f}", f"{luna[1]:.1f}"])
        rows.append([stamp, name, "Python", f"{py[0]:.3f}", f"{py[3]:.3f}", f"{py[2]:.3f}", f"{py[1]:.1f}"])

        print(f"{name:<12} {'Luna':<6} {luna[0]:>8.3f} {luna[3]:>11.3f} {luna[2]:>13.3f} {luna[1]:>9.1f}")
        print(f"{'':12} {'Python':<6} {py[0]:>8.3f} {py[3]:>11.3f} {py[2]:>13.3f} {py[1]:>9.1f}")
        log("-" * 72)

    header = ["Timestamp", "Benchmark", "Language", "User Time (s)",
              "GC Max Pause (ms)", "GC Total (ms)", "Max RSS (MB)"]
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