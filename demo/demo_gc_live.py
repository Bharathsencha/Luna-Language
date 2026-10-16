#!/usr/bin/env python3
"""Live GC pause monitor for the Luna VM — classroom demo.

Runs every Luna GC benchmark once, streams each GC phase pause over
Server-Sent Events to a browser, and draws a live chart of pause time
next to reference lines for the rival languages (Go, Java G1, Python)
read from test_gc/gc_all_results.csv.

Usage:
    make demo-gc                 # build + launch (opens the browser)
    python3 demo/demo_gc_live.py
    python3 demo/demo_gc_live.py --no-browser   # server only
    python3 demo/demo_gc_live.py --port 9000

No third-party dependencies. Press Ctrl-C to stop (kills bin/luna cleanly).
"""
import argparse
import json
import os
import queue
import re
import signal
import subprocess
import sys
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEMO_DIR = os.path.dirname(os.path.abspath(__file__))
LUNA_BIN = os.path.join(ROOT, "bin", "luna")
TESTGC_DIR = os.path.join(ROOT, "test_gc")
CSV_PATH = os.path.join(TESTGC_DIR, "gc_all_results.csv")
BENCHMARKS = ["alloc_heavy", "long_live", "cycles", "strings",
              "binary_trees", "map_churn", "object_graph", "concurrent_map"]
PAUSE_TARGET_MS = 0.064  # LUNA_GC_PAUSE_TARGET_US default (64us)

PAUSE_RE = re.compile(r"^GC_PAUSE ([\d.]+)ms phase=(\w+) minor=([01])$")
STATS_RE = re.compile(r"^LUNA_GC_STATS,([\d.]+),([\d.]+),(\d+)$")

PHASE_COLORS = {"mark": "#4f8ef7", "update_refs": "#46c0c0",
                "sweep": "#7ac142", "minor_collect": "#e58b1e"}
RIVAL_COLORS = {"Java (G1)": "#e5484d", "Go": "#30a46c", "Python": "#ffc53d"}


def load_refs():
    """Return {bench: {lang: max_pause_ms}} for the most recent CSV run."""
    refs = {b: {} for b in BENCHMARKS}
    if not os.path.exists(CSV_PATH):
        return refs
    by_ts = {}
    with open(CSV_PATH, newline="") as fh:
        import csv
        for row in csv.DictReader(fh):
            by_ts.setdefault(row["Timestamp"], []).append(row)
    if not by_ts:
        return refs
    latest = max(by_ts)
    for row in by_ts[latest]:
        bench = row["Benchmark"]
        lang = row["Language"]
        if bench in refs and lang in RIVAL_COLORS:
            try:
                refs[bench][lang] = float(row["GC Max Pause (ms)"])
            except ValueError:
                pass
    return refs


class SseHub:
    """Fan-out of JSON messages to all connected EventSource clients."""

    def __init__(self):
        self._clients = set()
        self._lock = threading.Lock()

    def add(self, q):
        with self._lock:
            self._clients.add(q)

    def remove(self, q):
        with self._lock:
            self._clients.discard(q)

    def broadcast(self, obj):
        payload = ("data: " + json.dumps(obj) + "\n\n").encode()
        with self._lock:
            clients = list(self._clients)
        for q in clients:
            try:
                q.put(payload)
            except Exception:
                pass


# Shared live state (guarded by its lock) between the runner thread, the
# HTTP handlers, and the SSE broadcaster.
class State:
    def __init__(self, refs):
        self.lock = threading.Lock()
        self.refs = refs
        self.proc = None
        self.bench = None
        self.bench_idx = 0
        self.bench_total = len(BENCHMARKS)
        self.running = False
        self.done = False
        self.restart = False
        self.events = []
        self.finished = {}
        self.started_at = time.monotonic()


def run_one_benchmark(state, hub, name):
    env = os.environ.copy()
    env["LUNA_GC_STATS"] = "1"
    env["LUNA_GC_PAUSE_TRACE"] = "1"
    env["LUNA_GC_PAUSE_TRACE_US"] = "1"
    t0 = time.monotonic()
    proc = subprocess.Popen(
        [LUNA_BIN, os.path.join(TESTGC_DIR, name + ".lu")],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True, env=env, bufsize=1)
    with state.lock:
        state.proc = proc

    stats = {}

    def read_stream(fh, is_err):
        for line in fh:
            line = line.strip()
            if not is_err:
                continue
            m = PAUSE_RE.match(line)
            if m:
                ms = float(m.group(1))
                pause = {"ms": ms, "phase": m.group(2),
                         "minor": m.group(3) == "1",
                         "t": round(time.monotonic() - t0, 4)}
                with state.lock:
                    state.events.append(pause)
                hub.broadcast({"type": "pause", "bench": name, **pause})
                continue
            m = STATS_RE.match(line)
            if m and "stats" not in stats:
                stats["stats"] = {"total": float(m.group(1)),
                                  "max": float(m.group(2)),
                                  "events": int(m.group(3))}

    readers = [threading.Thread(target=read_stream, args=(proc.stdout, False),
                                daemon=True),
               threading.Thread(target=read_stream, args=(proc.stderr, True),
                                daemon=True)]
    for t in readers:
        t.start()
    for t in readers:
        t.join()
    _, _, ru = os.wait4(proc.pid, 0)
    with state.lock:
        state.proc = None
    res = {"user": round(ru.ru_utime, 3),
           "rss": round(ru.ru_maxrss / 1024.0, 1),
           "wall": round(time.monotonic() - t0, 3),
           **stats.get("stats", {"total": 0.0, "max": 0.0, "events": 0})}
    return res


def run_benchmark_loop(state, hub):
    while True:
        with state.lock:
            state.restart = False
            state.done = False
            state.finished = {}
            state.events = []
            state.started_at = time.monotonic()
        hub.broadcast({"type": "reset", "benches": BENCHMARKS})
        for idx, name in enumerate(BENCHMARKS):
            with state.lock:
                if state.restart or STOP.is_set():
                    break
                state.bench = name
                state.bench_idx = idx
                state.running = True
                state.events = []
            hub.broadcast({"type": "bench_start", "bench": name, "idx": idx,
                           "total": len(BENCHMARKS)})
            print(f"[demo] running {name} ...", flush=True)
            res = run_one_benchmark(state, hub, name)
            with state.lock:
                state.finished[name] = res
            print(f"[demo] {name}: user={res['user']}s "
                  f"gc_total={res['total']}ms gc_max={res['max']}ms "
                  f"events={res['events']} rss={res['rss']}MB", flush=True)
            hub.broadcast({"type": "bench_done", "bench": name, "res": res})
        with state.lock:
            state.bench = None
            state.running = False
            state.done = True
        hub.broadcast({"type": "all_done"})
        while not state.restart and not STOP.is_set():
            time.sleep(0.05)
        if STOP.is_set():
            return


STOP = threading.Event()
HUB = None
STATE = None


def on_sigint(signum, frame):
    """Clean shutdown path: stop the loop and signal the running bin/luna
    child. The child is reaped by run_one_benchmark's wait4 (or by init on
    process exit), so we never double-reap it here."""
    STOP.set()
    with STATE.lock:
        proc = STATE.proc
    if proc is not None and proc.poll() is None:
        try:
            proc.terminate()
        except Exception:
            pass
        threading.Timer(1.5, _kill_late, (proc,)).start()


def _kill_late(proc):
    if proc.poll() is None:
        try:
            proc.kill()
        except Exception:
            pass


class DemoHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/events":
            self._stream_events()
        elif path == "/data":
            self._send_json(self._snapshot())
        elif path == "/ctl":
            if self.path.split("?", 1)[-1] == "restart=1":
                with STATE.lock:
                    STATE.restart = True
            self._send_json({"ok": True})
        elif path == "/":
            self._send_file("index.html")
        elif path in ("/style.css", "/app.js"):
            self._send_file(path.lstrip("/"))
        else:
            self.send_error(404)

    def _stream_events(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        q = queue.Queue()
        HUB.add(q)
        try:
            while True:
                item = q.get()
                if item is None:
                    break
                self.wfile.write(item)
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass
        finally:
            HUB.remove(q)

    def _snapshot(self):
        with STATE.lock:
            return {"refs": STATE.refs, "benches": BENCHMARKS,
                    "finished": STATE.finished,
                    "current": STATE.bench, "idx": STATE.bench_idx,
                    "total": STATE.bench_total, "running": STATE.running,
                    "done": STATE.done,
                    "events": STATE.events[-400:]}

    def _send_json(self, obj):
        body = json.dumps(obj).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_file(self, name):
        path = os.path.join(DEMO_DIR, name)
        try:
            with open(path, "rb") as fh:
                body = fh.read()
        except OSError:
            self.send_error(404)
            return
        ctype = "text/html; charset=utf-8" if name.endswith(".html") else (
            "text/css; charset=utf-8" if name.endswith(".css")
            else "application/javascript")
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


def server_thread(server):
    server.serve_forever()


def main():
    ap = argparse.ArgumentParser(description="Live GC pause monitor")
    ap.add_argument("--port", type=int, default=int(os.environ.get("PORT", 8000)))
    ap.add_argument("--no-browser", action="store_true",
                    help="do not auto-open the browser")
    args = ap.parse_args()

    if not os.path.exists(LUNA_BIN):
        sys.exit("bin/luna not found — run `make` first")
    global HUB, STATE
    HUB = SseHub()
    STATE = State(load_refs())

    signal.signal(signal.SIGINT, on_sigint)
    signal.signal(signal.SIGTERM, on_sigint)

    server = ThreadingHTTPServer(("127.0.0.1", args.port), DemoHandler)
    server.daemon_threads = True
    threading.Thread(target=server_thread, args=(server,), daemon=True).start()
    url = f"http://127.0.0.1:{args.port}/"
    print(f"[demo] live GC monitor at {url}")
    if not args.no_browser:
        threading.Timer(0.4, lambda: webbrowser.open(url)).start()

    run_benchmark_loop(STATE, HUB)
    for q in list(HUB._clients):
        q.put(None)
    server.shutdown()
    print("[demo] stopped")


if __name__ == "__main__":
    main()