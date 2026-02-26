import time
import sys

try:
    import numpy as np
except ImportError:
    print("Python NumPy Skipped. 'numpy' library is not installed.")
    sys.exit(0)

size = 1000000
A_np = np.full(size, 1.5, dtype=np.float64)
B_np = np.full(size, 2.5, dtype=np.float64)

_ = A_np * B_np

start_np = time.perf_counter()
C_np = A_np * B_np
end_np = time.perf_counter()

print(f"Python NumPy Vector Time: {end_np - start_np:.6f} seconds")
