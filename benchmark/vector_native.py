import time

size = 1000000
A = [1.5] * size
B = [2.5] * size
C = [0.0] * size

start = time.perf_counter()

for i in range(size):
    C[i] = A[i] * B[i]

end = time.perf_counter()

print(f"Python Native Vector Time: {end - start:.4f} seconds")
