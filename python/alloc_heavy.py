import gc
import time
import gcstats  # noqa: F401  (registers stats at exit)

start = time.perf_counter()
items = []
for i in range(750000):
    items.append("item-" * 2 + str(i))
elapsed = time.perf_counter() - start
print("alloc_heavy", len(items))
print("Time:", f"{elapsed:.3f}", "seconds")