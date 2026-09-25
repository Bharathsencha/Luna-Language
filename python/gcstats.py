"""CPython GC measurement hook (mirrors Luna's LUNA_GC_STATS line).

CPython reclaims most objects instantly via reference counting, so those
allocations never appear as a "pause" - their cost shows up in user time.
Only the periodic cyclic-GC (which collects reference cycles) has a
measurable stop.  This module times every cyclic collection and prints a
single stat line at exit:

    PY_GC_STATS,<collections>,<processed>,<max_pause_ms>,<total_ms>
"""
import atexit
import gc
import time

_start = None
_collections = 0
_processed = 0
_max_pause = 0.0
_total = 0.0


def _run(phase, info):
    global _start, _collections, _processed, _max_pause, _total
    if phase == "start":
        _collections += 1
        _start = time.perf_counter()
        return
    processed = info.get("collected", 0)
    if info.get("uncollectable"):
        processed += info["uncollectable"]
    _processed += processed
    dt = (time.perf_counter() - _start) * 1000.0
    _total += dt
    if dt > _max_pause:
        _max_pause = dt


def _report():
    print(f"PY_GC_STATS,{_collections},{_processed},{_max_pause:.4f},{_total:.4f}", flush=True)


gc.callbacks.append(_run)
atexit.register(_report)