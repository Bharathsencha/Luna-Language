import gc
import time
import sys
import gcstats  # noqa: F401  (registers stats at exit)


class Node:
    __slots__ = ("left", "right", "value")

    def __init__(self, left, right, value):
        self.left = left
        self.right = right
        self.value = value


def build(depth):
    if depth == 0:
        return None
    return Node(build(depth - 1), build(depth - 1), 1)


def checksum(node):
    if node is None:
        return 0
    return node.value + checksum(node.left) + checksum(node.right)


sys.setrecursionlimit(2000)
start = time.perf_counter()
total = 0
for i in range(12):
    root = build(17)
    total = total + checksum(root)
    root = None
elapsed = time.perf_counter() - start
print("binary_trees", total)
print("Time:", f"{elapsed:.3f}", "seconds")