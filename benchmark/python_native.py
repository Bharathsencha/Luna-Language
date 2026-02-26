import time

m_size = 300
A = [[1.5] * m_size for _ in range(m_size)]
B = [[2.5] * m_size for _ in range(m_size)]
C = [[0.0] * m_size for _ in range(m_size)]

start = time.perf_counter()

for i in range(m_size):
    Ai = A[i]
    Ci = C[i]
    for k in range(m_size):
        r = Ai[k]
        Bk = B[k]
        for j in range(m_size):
            Ci[j] += r * Bk[j]

end = time.perf_counter()

print(f"Python Native Time: {end - start:.4f} seconds")