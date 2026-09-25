import gc
import time
import gcstats  # noqa: F401  (registers stats at exit)

start = time.perf_counter()
table = []
for i in range(200000):
    table.append("value-" * 2 + str(i))
for j in range(400000):
    _ = "scratch-" * 2 + str(j)
elapsed = time.perf_counter() - start
print("long_live", len(table))
print("Time:", f"{elapsed:.3f}", "seconds")