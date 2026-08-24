#!/usr/bin/env python3
"""Simple Go vs Luna GC benchmark: 3-run averages only.

One row per benchmark per language:
    Timestamp, Benchmark, Language, User Time (s), GC Max Pause (ms),
    GC Total (ms), Max RSS (MB)

Usage: make test-gc-three
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


BENCHMARKS = ["alloc_heavy", "long_live", "cycles", "strings"]
RUNS = 3
CSV_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "stress_results.csv")
LUNA_BIN = "./bin/luna"

USER_RE = re.compile(r"User time \(seconds\):\s*([\d.]+)")
RSS_RE = re.compile(r"Maximum resident set size \(kbytes\):\s*(\d+)")
LUNA_GC_RE = re.compile(r"LUNA_GC_STATS,([\d.]+),([\d.]+),(\d+)")
GCTRACE_RE = re.compile(r"gc \d+ @[\d.]+s \d+%: ([\d.]+)\+[\d.]+\+([\d.]+) ms clock")


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
    return user, rss_mb, proc.stderr


def bench_luna(name):
    env = os.environ.copy()
    env["LUNA_GC_STATS"] = "1"
    user, rss, err = run_timed([LUNA_BIN, f"test_gc/{name}.lu"], env)
    gc_total = gc_max = 0.0
    m = LUNA_GC_RE.search(err)
    if m:
        gc_total = float(m.group(1))
        gc_max = float(m.group(2))
    return user, rss, gc_total, gc_max


def bench_go(name):
    exe = f"test_gc/{name}_go"
    if not os.path.exists(exe):
        subprocess.run(["go", "build", "-o", exe, f"go/{name}.go"], check=True)
    env = os.environ.copy()
    env["GODEBUG"] = "gctrace=1"
    user, rss, err = run_timed([exe], env)
    gc_total = 0.0
    pauses = []
    for m in GCTRACE_RE.finditer(err):
        a = float(m.group(1))
        c = float(m.group(2))
        pauses.append(a)
        pauses.append(c)
        gc_total += a + c
    gc_max = max(pauses) if pauses else 0.0
    return user, rss, gc_total, gc_max


def avg(vals):
    return sum(vals) / len(vals) if vals else 0.0


def main():
    if not os.path.exists(LUNA_BIN):
        print(f"error: {LUNA_BIN} not found; run `make` first", file=sys.stderr)
        return 1

    stamp = time.strftime("%Y-%m-%d %H:%M:%S")
    rows = []
    log(f"Go vs Luna GC — {RUNS}-run averages — {stamp}")
    log("=" * 72)
    print(f"{'benchmark':<12} {'lang':<5} {'user(s)':>8} {'gc_max(ms)':>11} {'gc_total(ms)':>13} {'rss(MB)':>9}")

    for name in BENCHMARKS:
        lr = [bench_luna(name) for _ in range(RUNS)]
        gr = [bench_go(name) for _ in range(RUNS)]

        luna = [avg([r[i] for r in lr]) for i in range(4)]
        go = [avg([r[i] for r in gr]) for i in range(4)]

        rows.append([stamp, name, "Luna", f"{luna[0]:.3f}", f"{luna[3]:.3f}", f"{luna[2]:.3f}", f"{luna[1]:.1f}"])
        rows.append([stamp, name, "Go", f"{go[0]:.3f}", f"{go[3]:.3f}", f"{go[2]:.3f}", f"{go[1]:.1f}"])

        print(f"{name:<12} {'Luna':<5} {luna[0]:>8.3f} {luna[3]:>11.3f} {luna[2]:>13.3f} {luna[1]:>9.1f}")
        print(f"{'':12} {'Go':<5} {go[0]:>8.3f} {go[3]:>11.3f} {go[2]:>13.3f} {go[1]:>9.1f}")
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
