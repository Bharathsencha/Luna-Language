import gc
import time
import gcstats  # noqa: F401  (registers stats at exit)


class Node:
    __slots__ = ("edges", "value")

    def __init__(self, value):
        self.edges = []
        self.value = value


start = time.perf_counter()
nodes = [Node(i) for i in range(100000)]

for i in range(100000):
    n = nodes[i]
    for e in range(4):
        other = nodes[(i * 31 + e * 17) % 100000]
        n.edges.append(other)

for i in range(10000):
    a = nodes[(i * 97) % 100000]
    b = nodes[(i * 53) % 100000]
    if len(a.edges) > 0:
        a.edges[(i * 13) % len(a.edges)] = b

total = 0
for i in range(100000):
    total = total + len(nodes[i].edges)
elapsed = time.perf_counter() - start
print("object_graph", total)
print("Time:", f"{elapsed:.3f}", "seconds")