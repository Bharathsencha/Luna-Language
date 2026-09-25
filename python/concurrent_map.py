import gc
import time
import gcstats  # noqa: F401  (registers stats at exit)

start = time.perf_counter()
m = {}
for i in range(1000000):
    key = str((i * 2654435761) % 100000)
    op = i % 3
    if op == 0:
        m[key] = i
    elif op == 1:
        m.pop(key, None)
    else:
        _ = m.get(key)
elapsed = time.perf_counter() - start
print("concurrent_map", len(m))
print("Time:", f"{elapsed:.3f}", "seconds")