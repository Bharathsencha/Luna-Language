import gc
import time
import gcstats  # noqa: F401  (registers stats at exit)


class Node:
    __slots__ = ("name", "next")

    def __init__(self, name, next_):
        self.name = name
        self.next = next_


start = time.perf_counter()
nodes = [Node("n" + str(i), None) for i in range(50000)]

for i in range(len(nodes)):
    next_i = (i + 1) % len(nodes)
    nodes[i].next = nodes[next_i]

elapsed = time.perf_counter() - start
print("cycles", len(nodes))
print("Time:", f"{elapsed:.3f}", "seconds")