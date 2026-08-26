import subprocess
import re
import sys
import os
import csv

benchmarks = ["alloc_heavy", "long_live", "cycles", "strings"]

# Compile Go benchmarks
print("Compiling Go benchmarks...")
for bench in benchmarks:
    src = f"go/{bench}.go"
    out = f"test_gc/{bench}_go"
    print(f"Building {src} -> {out}")
    subprocess.run(["go", "build", "-o", out, src], check=True)

results = {
    "luna": {b: {"user": [], "sys": [], "rss": [], "gc_total": [], "gc_max": [], "gc_events": []} for b in benchmarks},
    "go": {b: {"user": [], "sys": [], "rss": []} for b in benchmarks}
}

user_re = re.compile(r"User time \(seconds\):\s*([\d\.]+)")
sys_re = re.compile(r"System time \(seconds\):\s*([\d\.]+)")
rss_re = re.compile(r"Maximum resident set size \(kbytes\):\s*(\d+)")
gc_re = re.compile(r"LUNA_GC_STATS,([\d\.]+),([\d\.]+),(\d+)")

def parse_time_output(stderr_data):
    user_match = user_re.search(stderr_data)
    sys_match = sys_re.search(stderr_data)
    rss_match = rss_re.search(stderr_data)
    
    user = float(user_match.group(1)) if user_match else 0.0
    sys_val = float(sys_match.group(1)) if sys_match else 0.0
    rss = float(rss_match.group(1)) / 1024.0 if rss_match else 0.0 # Convert KB to MB
    
    return user, sys_val, rss

# Run benchmarks 3 times
runs = 3
for run in range(1, runs + 1):
    print(f"\n--- Run {run} of {runs} ---")
    for bench in benchmarks:
        # Run Go
        print(f"Running Go {bench}...")
        cmd = ["/usr/bin/time", "-v", f"test_gc/{bench}_go"]
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        user, sys_val, rss = parse_time_output(proc.stderr)
        results["go"][bench]["user"].append(user)
        results["go"][bench]["sys"].append(sys_val)
        results["go"][bench]["rss"].append(rss)
        print(f"  Go: User={user:.2f}s, Sys={sys_val:.2f}s, RSS={rss:.2f}MB")

        # Run Luna
        print(f"Running Luna {bench}...")
        cmd = ["/usr/bin/time", "-v", "./bin/luna", f"test_gc/{bench}.lu"]
        env = os.environ.copy()
        env["LUNA_GC_STATS"] = "1"
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env, text=True)
        user, sys_val, rss = parse_time_output(proc.stderr)
        
        # Parse GC Stats
        gc_match = gc_re.search(proc.stderr)
        if gc_match:
            gc_total = float(gc_match.group(1))
            gc_max = float(gc_match.group(2))
            gc_events = int(gc_match.group(3))
        else:
            gc_total, gc_max, gc_events = 0.0, 0.0, 0
            
        results["luna"][bench]["user"].append(user)
        results["luna"][bench]["sys"].append(sys_val)
        results["luna"][bench]["rss"].append(rss)
        results["luna"][bench]["gc_total"].append(gc_total)
        results["luna"][bench]["gc_max"].append(gc_max)
        results["luna"][bench]["gc_events"].append(gc_events)
        print(f"  Luna: User={user:.2f}s, Sys={sys_val:.2f}s, RSS={rss:.2f}MB, GC Total={gc_total:.2f}ms, GC Max={gc_max:.2f}ms, GC Events={gc_events}")

# Print Averages and Write to CSV
csv_path = "test_gc/benchmark_results.csv"
print("\n" + "="*50)
print("             BENCHMARK RESULTS (AVERAGE OF 3 RUNS)")
print("="*50)

print("\n| Benchmark | Language | User Time (s) | Sys Time (s) | Max RSS (MB) | GC Total (ms) | GC Max (ms) | GC Events |")
print("|---|---|---|---|---|---|---|---|")

csv_rows = []
for bench in benchmarks:
    go_user = sum(results["go"][bench]["user"]) / runs
    go_sys = sum(results["go"][bench]["sys"]) / runs
    go_rss = sum(results["go"][bench]["rss"]) / runs
    
    luna_user = sum(results["luna"][bench]["user"]) / runs
    luna_sys = sum(results["luna"][bench]["sys"]) / runs
    luna_rss = sum(results["luna"][bench]["rss"]) / runs
    luna_gc_total = sum(results["luna"][bench]["gc_total"]) / runs
    luna_gc_max = sum(results["luna"][bench]["gc_max"]) / runs
    luna_gc_events = sum(results["luna"][bench]["gc_events"]) / runs

    print(f"| **{bench}** | Luna | {luna_user:.3f} | {luna_sys:.3f} | {luna_rss:.2f} | {luna_gc_total:.3f} | {luna_gc_max:.3f} | {luna_gc_events:.1f} |")
    print(f"| | Go | {go_user:.3f} | {go_sys:.3f} | {go_rss:.2f} | - | - | - |")
    
    csv_rows.append({
        "Benchmark": bench,
        "Language": "Luna",
        "User Time (s)": f"{luna_user:.3f}",
        "Sys Time (s)": f"{luna_sys:.3f}",
        "Max RSS (MB)": f"{luna_rss:.2f}",
        "GC Total (ms)": f"{luna_gc_total:.3f}",
        "GC Max (ms)": f"{luna_gc_max:.3f}",
        "GC Events": f"{luna_gc_events:.1f}"
    })
    csv_rows.append({
        "Benchmark": bench,
        "Language": "Go",
        "User Time (s)": f"{go_user:.3f}",
        "Sys Time (s)": f"{go_sys:.3f}",
        "Max RSS (MB)": f"{go_rss:.2f}",
        "GC Total (ms)": "-",
        "GC Max (ms)": "-",
        "GC Events": "-"
    })

# Write CSV
headers = ["Benchmark", "Language", "User Time (s)", "Sys Time (s)", "Max RSS (MB)", "GC Total (ms)", "GC Max (ms)", "GC Events"]
with open(csv_path, mode="w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=headers)
    writer.writeheader()
    writer.writerows(csv_rows)

print(f"\nResults successfully written to {csv_path}\n")
