#!/usr/bin/env python3
"""Java (G1) vs Luna GC benchmark — 3 runs each, simple summary CSV.

Usage: make test-java-gc
Output: java/java_vs_luna.csv
"""
import csv
import os
import re
import subprocess
import sys
import time
from pathlib import Path

try:
    sys.stdout.reconfigure(line_buffering=True)
    sys.stderr.reconfigure(line_buffering=True)
except AttributeError:
    pass

ROOT = Path(__file__).parent.parent
JAVA_DIR = ROOT / "java"
JAVA_OUT = JAVA_DIR / "out"
CSV_PATH = JAVA_DIR / "java_vs_luna.csv"
LUNA_BIN = "./bin/luna"

BENCHMARKS = ["alloc_heavy", "long_live", "cycles", "strings",
              "binary_trees", "map_churn", "object_graph", "concurrent_map"]
JAVA_OPTS = ["-Xms256m", "-Xmx256m", "-XX:+UseG1GC",
             "-Xlog:gc:stderr:time,uptime"]

USER_RE = re.compile(r"User time \(seconds\):\s*([\d.]+)")
RSS_RE = re.compile(r"Maximum resident set size \(kbytes\):\s*(\d+)")
LUNA_GC_RE = re.compile(r"LUNA_GC_STATS,([\d.]+),([\d.]+),(\d+)")
BENCH_RE = re.compile(r"BENCH (\w+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)")
PAUSE_RE = re.compile(r"\[(\d+\.\d+)s\].*Pause.*?([\d.]+)ms")


def log(msg):
    print(msg, flush=True)


def bench_java():
    """One JVM: all four benchmarks, 3 measured runs each."""
    if not (JAVA_OUT / "GcBench.class").exists():
        JAVA_OUT.mkdir(exist_ok=True)
        r = subprocess.run(["javac", "-d", str(JAVA_OUT), str(JAVA_DIR / "GcBench.java")],
                           capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stderr, file=sys.stderr)
            sys.exit(1)

    proc = subprocess.run(["java"] + JAVA_OPTS + ["-cp", str(JAVA_OUT), "GcBench"],
                          capture_output=True, text=True, cwd=str(ROOT))
    if proc.returncode != 0:
        print(proc.stderr, file=sys.stderr)
        sys.exit(1)

    pauses = []
    for m in PAUSE_RE.finditer(proc.stderr):
        t_ms = float(m.group(1)) * 1000.0
        pauses.append((t_ms, float(m.group(2))))

    runs = {}
    for m in BENCH_RE.finditer(proc.stdout):
        name = m.group(1)
        run = int(m.group(2))
        user_ms = int(m.group(3))
        gc_count = int(m.group(4))
        gc_total = int(m.group(5))
        up0 = int(m.group(6))
        up1 = int(m.group(7))
        gc_max = max((p for t, p in pauses if up0 <= t <= up1), default=0.0)
        runs.setdefault(name, []).append(
            {"user": user_ms / 1000.0, "gc_count": gc_count,
             "gc_total": gc_total, "gc_max": gc_max})
    return runs


def bench_luna(name):
    env = os.environ.copy()
    env["LUNA_GC_STATS"] = "1"
    out = []
    for _ in range(3):
        full = ["/usr/bin/time", "-v", LUNA_BIN, f"test_gc/{name}.lu"]
        p = subprocess.run(full, capture_output=True, text=True, env=env)
        user = rss = 0.0
        m = USER_RE.search(p.stderr)
        if m:
            user = float(m.group(1))
        m = RSS_RE.search(p.stderr)
        if m:
            rss = float(m.group(1)) / 1024.0
        gc_total = gc_max = 0.0
        gc_count = 0
        m = LUNA_GC_RE.search(p.stderr)
        if m:
            gc_total = float(m.group(1))
            gc_max = float(m.group(2))
            gc_count = int(m.group(3))
        out.append({"user": user, "rss": rss, "gc_total": gc_total,
                    "gc_max": gc_max, "gc_count": gc_count})
    return out


def avg(vals):
    return sum(vals) / len(vals) if vals else 0.0


def main():
    if not os.path.exists(LUNA_BIN):
        print(f"error: {LUNA_BIN} not found; run `make` first", file=sys.stderr)
        return 1

    stamp = time.strftime("%Y-%m-%d %H:%M:%S")
    rows = []
    log(f"Java (G1, 256MB) vs Luna GC — 3-run averages — {stamp}")
    log("=" * 78)
    print(f"{'benchmark':<16} {'lang':<5} {'user(s)':>8} {'gc_max(ms)':>11} "
          f"{'gc_total(ms)':>13} {'gc_count':>9} {'rss(MB)':>9}")

    jr = bench_java()
    for name in BENCHMARKS:
        log(f"[{name}] ...")
        lr = bench_luna(name)
        j = jr.get(name, [])
        j_avg = lambda k: avg([r[k] for r in j])
        l_avg = lambda k: avg([r[k] for r in lr])

        for r in j:
            rows.append([stamp, name, "Java", f"{r['user']:.3f}", f"{r['gc_max']:.3f}",
                         f"{r['gc_total']:.3f}", str(r["gc_count"]), ""])
        for r in lr:
            rows.append([stamp, name, "Luna", f"{r['user']:.3f}", f"{r['gc_max']:.3f}",
                         f"{r['gc_total']:.3f}", str(r["gc_count"]), f"{r['rss']:.1f}"])

        print(f"{name:<16} {'Java':<5} {j_avg('user'):>8.3f} {j_avg('gc_max'):>11.3f} "
              f"{j_avg('gc_total'):>13.3f} {j_avg('gc_count'):>9.1f} {'—':>9}")
        print(f"{'':16} {'Luna':<5} {l_avg('user'):>8.3f} {l_avg('gc_max'):>11.3f} "
              f"{l_avg('gc_total'):>13.3f} {l_avg('gc_count'):>9.1f} {l_avg('rss'):>9.1f}")
        log("-" * 78)

    header = ["Timestamp", "Benchmark", "Language", "User Time (s)",
              "GC Max Pause (ms)", "GC Total (ms)", "GC Count", "Max RSS (MB)"]
    new_file = not CSV_PATH.exists()
    with open(CSV_PATH, "a", newline="") as f:
        w = csv.writer(f)
        if new_file:
            w.writerow(header)
        w.writerows(rows)
    log(f"Appended {len(rows)} rows to {CSV_PATH}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
