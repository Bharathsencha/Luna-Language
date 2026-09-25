import gc
import time
import gcstats  # noqa: F401  (registers stats at exit)

start = time.perf_counter()
out = []
for i in range(500000):
    out.append("luna" * 16 + "-" + str(i))
elapsed = time.perf_counter() - start
print("strings", len(out))
print("Time:", f"{elapsed:.3f}", "seconds")