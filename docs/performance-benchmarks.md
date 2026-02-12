# Matrix Multiplication Performance Comparison

## Overview
Comparison of 200×200 matrix multiplication across three implementations:
- **Naive Python**: Pure Python with nested loops
- **Optimized Python**: NumPy with BLAS libraries
- **Luna Language**: Custom language with C/SIMD optimization

---

## 1. Naive Python Implementation

### 200×200 Matrix

```python
import time

size = 200
A = []
B = []

print("Initializing", size, "x", size, "matrices...")

# Create matrices
for i in range(size):
    rowA = []
    rowB = []
    for j in range(size):
        rowA.append(1.0)
        rowB.append(2.0)
    A.append(rowA)
    B.append(rowB)

print("Matrices initialized. Starting naive mat_mul...")

start = time.perf_counter()

# Naive matrix multiplication
C = [[0.0 for _ in range(size)] for _ in range(size)]
for i in range(size):
    for j in range(size):
        for k in range(size):
            C[i][j] += A[i][k] * B[k][j]

end = time.perf_counter()

print("Matrix Multiplication took:", end - start, "seconds")
print("Verification - Result[0][0]:", C[0][0])
```

**Result**: `2.351878 seconds`

---

### 50×50 Matrix

```python
import time

def create_matrix(rows, cols, val):
    m = []
    for r in range(rows):
        row = []
        for c in range(cols):
            row.append(val)
        m.append(row)
    return m

print("Simulating 50x50 Matrix Multiplication...")

start = time.perf_counter()

size = 50
A = create_matrix(size, size, 2)
B = create_matrix(size, size, 3)
C = create_matrix(size, size, 0)

for i in range(size):
    for j in range(size):
        total = 0
        for k in range(size):
            a_val = A[i][k]
            b_val = B[k][j]
            total = total + (a_val * b_val)
        C[i][j] = total

end = time.perf_counter()

print("Value at [0][0] (expected 300):", C[0][0])
print("Time taken:", end - start, "seconds")
```

**Result**: `0.0237 seconds`

---

## 2. Optimized Python Implementation (NumPy)

### 200×200 Matrix

```python
import numpy as np
import time

size = 200

print("Initializing", size, "x", size, "matrices...")

A = np.full((size, size), 1.0, dtype=np.float64)
B = np.full((size, size), 2.0, dtype=np.float64)

print("Matrices initialized. Starting optimized matmul...")

start = time.perf_counter()
C = A @ B   # Uses optimized BLAS (OpenBLAS / MKL)
end = time.perf_counter()

print("Matrix Multiplication took:", end - start, "seconds")
print("Verification - Result[0][0]:", C[0][0])
```

**Result**: `0.0453 seconds`

---

### 50×50 Matrix

```python
import numpy as np
import time

print("Simulating 50x50 Matrix Multiplication (NumPy)...")

start = time.perf_counter()

size = 50
A = np.full((size, size), 2)
B = np.full((size, size), 3)
C = A @ B

end = time.perf_counter()

print("Value at [0][0] (expected 300):", C[0][0])
print("Time taken:", end - start, "seconds")
```

**Result**: `0.000156 seconds`

---

## 3. Luna Language Implementation

### 200×200 Matrix

```javascript
# matrix_test.lu
let size = 200
let A = []
let B = []

print("Initializing", size, "x", size, "matrices...")

# Create Matrix A and B
for (let i = 0; i < size; i++) {
    let rowA = []
    let rowB = []
    for (let j = 0; j < size; j++) {
        # Using floating point to trigger Dense List optimization
        append(rowA, 1.0) 
        append(rowB, 2.0)
    }
    append(A, rowA)
    append(B, rowB)
}

print("Matrices initialized. Starting native mat_mul...")

# Measure the time of the native C/SIMD path
let start = clock()
let C = mat_mul(A, B)
let end = clock()

print("Matrix Multiplication took:", end - start, "seconds")

# Verify the result: Row 0, Col 0 of C should be (size * 1.0 * 2.0)
print("Verification - Result[0][0]:", C[0][0])
```

**Result**: `0.0169 seconds`

---

### 50×50 Matrix

```javascript
let size = 50
let A = []
let B = []

print("Initializing", size, "x", size, "matrices...")

# Create Matrix A and B
for (let i = 0; i < size; i++) {
    let rowA = []
    let rowB = []
    for (let j = 0; j < size; j++) {
        # Using floating point to trigger Dense List optimization
        append(rowA, 1.0) 
        append(rowB, 2.0)
    }
    append(A, rowA)
    append(B, rowB)
}

print("Matrices initialized. Starting native mat_mul...")

# Measure the time of the native C/SIMD path
let start = clock()
let C = mat_mul(A, B)
let end = clock()

print("Matrix Multiplication took:", end - start, "seconds")

# Verify the result: Row 0, Col 0 of C should be (size * 1.0 * 2.0)
print("Verification - Result[0][0]:", C[0][0])
```

**Result**: `0.000245 seconds`

---

## Performance Comparison

### 200×200 Matrix

| Implementation | Time (seconds) | Relative Speed |
|---------------|----------------|----------------|
| Naive Python | 2.3519 | 1× (baseline) |
| NumPy (BLAS) | 0.0453 | **52× faster** |
| Luna Language | 0.0169 | **139× faster** |

### 50×50 Matrix

| Implementation | Time (seconds) | Relative Speed |
|---------------|----------------|----------------|
| Naive Python | 0.0237 | 1× (baseline) |
| NumPy (BLAS) | 0.000156 | **152× faster** |
| Luna Language | 0.000245 | **97× faster** |

### Key Notes

1. **Luna vs NumPy (200×200)**: Luna is **2.7× faster** than optimized NumPy
   - Luna: 16.9ms
   - NumPy: 45.3ms

2. **Luna vs NumPy (50×50)**: NumPy is **1.6× faster** than Luna
   - Luna: 0.245ms
   - NumPy: 0.156ms
   - Note: For smaller matrices, NumPy's BLAS has lower overhead

3. **Optimization Impact**: 
   - Moving from naive Python loops to optimized libraries provides **50-152× speedup**
   - Luna's C/SIMD implementation excels on larger matrices
   - NumPy's advantage increases for smaller matrices due to lower call overhead

4. **All implementations verified correctly**: 
   - 200×200: Result[0][0] = 400.0 (200 × 1.0 × 2.0)
   - 50×50: Result[0][0] = 300.0 (50 × 2.0 × 3.0)



---

## Additional Benchmarks

### Vector Multiplication (SIMD Performance)

#### 1. Naive Python Implementation

```python
import time

size = 1_000_000
v1 = []
v2 = []

for i in range(size):
    v1.append(10.5)
    v2.append(2.5)

print("Testing multiplication path...")

start = time.perf_counter()
res = [v1[i] * v2[i] for i in range(size)]
end = time.perf_counter()

print("Time for 1M element multiplication:", end - start, "seconds")
print("Result[0]:", res[0])
```

**Result**: `0.2168 seconds` for 1,000,000 elements

**Performance**: ~4.6 million operations/second

---

#### 2. Optimized Python (NumPy SIMD)

```python
import numpy as np
import time

size = 1_000_000

# Contiguous float64 arrays
v1 = np.full(size, 10.5, dtype=np.float64)
v2 = np.full(size, 2.5, dtype=np.float64)

print("Testing Zero-Copy SIMD path...")

start = time.perf_counter()
res = v1 * v2   # Vectorized SIMD operation
end = time.perf_counter()

print("Time for 1M element SIMD multiplication:", end - start, "seconds")
print("Result[0]:", res[0])
```

**Result**: `0.00261 seconds` for 1,000,000 elements

**Performance**: ~383 million operations/second

---

#### 3. Luna Language SIMD Implementation

```javascript
# dense_path_test.lu
let size = 1000000
let v1 = []
let v2 = []

for (let i = 0; i < size; i++) {
    append(v1, 10.5)
    append(v2, 2.5)
}

print("Testing Zero-Copy SIMD path...")

let start = clock() 
let res = v1 * v2
let end = clock()

print("Time for 1M element SIMD multiplication:", end - start, "seconds")
print("Result[0]:", res[0])
```

**Result**: `0.0641 seconds` for 1,000,000 elements

**Performance**: ~15.6 million operations/second

---

#### Vector Multiplication Comparison

| Implementation | Time (seconds) | Ops/Second | Relative Speed |
|---------------|----------------|------------|----------------|
| Naive Python | 0.2168 | 4.6M | 1× (baseline) |
| Luna SIMD | 0.0641 | 15.6M | **3.4× faster** |
| NumPy SIMD | 0.00261 | 383M | **83× faster** |

---

## Variable Resolution / Scope Access Benchmark

#### 1. Python Native Variable Lookup

```python
import time

iterations = 100
counter = 0

print("Starting environment lookup stress test...")

start = time.perf_counter()

for i in range(iterations):
    for j in range(iterations):
        for k in range(iterations):
            counter = counter + 1

end = time.perf_counter()

print("Total iterations:", counter)
print("Environment lookup time:", end - start, "seconds")
```

**Result**: `0.0728 seconds` for 1,000,000 iterations

**Performance**: ~13.7 million lookups/second

---

#### 2. Python Dictionary (Hash Table) Lookup

```python
import time

iterations = 100
env = {
    "counter": 0
}

print("Starting hash-table environment lookup stress test...")

start = time.perf_counter()

for i in range(iterations):
    for j in range(iterations):
        for k in range(iterations):
            env["counter"] = env["counter"] + 1

end = time.perf_counter()

print("Total iterations:", env["counter"])
print("Environment lookup time:", end - start, "seconds")
```

**Result**: `0.1103 seconds` for 1,000,000 iterations

**Performance**: ~9.1 million lookups/second

---

#### 3. Luna Hash Table Implementation

```javascript
# hash_test.lu
let iterations = 100
let counter = 0

print("Starting environment lookup stress test...")

let start = clock()

# This triple-loop performs millions of variable lookups
for (let i = 0; i < iterations; i++) {
    for (let j = 0; j < iterations; j++) {
        for (let k = 0; k < iterations; k++) {
            # Accessing 'counter', 'i', 'j', and 'k' now uses the Hash Table
            counter = counter + 1
        }
    }
}

let end = clock()

print("Total iterations:", counter)
print("Environment lookup time:", end - start, "seconds")
```

**Result**: `0.6869 seconds` for 1,000,000 iterations

**Performance**: ~1.46 million lookups/second

---

#### Environment Lookup Comparison

| Implementation | Time (seconds) | Lookups/Second | Relative Speed |
|---------------|----------------|----------------|----------------|
| Python Native Variables | 0.0728 | 13.7M | **9.4× faster** |
| Python Dict (Hash) | 0.1103 | 9.1M | **6.2× faster** |
| Luna Hash Table | 0.6869 | 1.46M | 1× (baseline) |

**Analysis:**
- Python's native variable lookup is highly optimized (13.7M ops/sec)
- Python dict operations are 51% slower than native variables but still fast
- Luna's hash table implementation is 9.4× slower than Python native lookups
- This represents an optimization opportunity for Luna's variable environment management

---

## Complete Performance Summary

| Benchmark | Implementation | Operations | Time (s) | Ops/Second | Speedup |
|-----------|---------------|------------|----------|------------|---------|
| **Matrix Mult (200×200)** | Naive Python | 8M | 2.3519 | 3.4M | 1× |
| | NumPy BLAS | 8M | 0.0453 | 176.6M | 52× |
| | **Luna C/SIMD** | 8M | 0.0169 | **473.4M** | **139×** |
| **Matrix Mult (50×50)** | Naive Python | 125K | 0.0237 | 5.3M | 1× |
| | **NumPy BLAS** | 125K | 0.000156 | **801M** | **152×** |
| | Luna C/SIMD | 125K | 0.000245 | 510M | 97× |
| **Vector Mult (1M)** | Naive Python | 1M | 0.2168 | 4.6M | 1× |
| | Luna SIMD | 1M | 0.0641 | 15.6M | 3.4× |
| | **NumPy SIMD** | 1M | 0.00261 | **383M** | **83×** |
| **Env Lookups (1M)** | Luna Hash Table | 1M | 0.6869 | 1.46M | 1× |
| | Python Dict | 1M | 0.1103 | 9.1M | 6.2× |
| | **Python Native** | 1M | 0.0728 | **13.7M** | **9.4×** |

### Architecture Highlights

**Luna Language Performance Characteristics:**

1. **Matrix Operations - Size Matters**: 
   - **200×200**: Luna is **2.7× faster** than NumPy (473.4M vs 176.6M ops/sec)
   - **50×50**: NumPy is **1.6× faster** than Luna (801M vs 510M ops/sec)
   - **Insight**: Luna's optimizations shine on larger matrices; NumPy has lower overhead on smaller ones

2. **Vector Operations (Optimization Opportunity)**:
   - 3.4× faster than naive Python
   - NumPy is 24.5× faster than Luna (383M vs 15.6M ops/sec)
   - Suggests room for SIMD optimization in element-wise operations

3. **Variable Lookups (Optimization Opportunity)**:
   - Luna hash table: 1.46M ops/second
   - Python native variables: 13.7M ops/second (9.4× faster)
   - Python dict: 9.1M ops/second (6.2× faster)
   - Significant room for improvement in environment management

4. **Overall Assessment**:
   - Luna excels at complex, large-scale operations (large matrix multiplication)
   - NumPy dominates simple vectorized operations and smaller matrices
   - Python's VM optimizations excel at variable lookups
   - Performance characteristics change with problem size


# Note:
- Benchmarks focus on specific kernels and do not represent overall language performance.
- Matrix multiplication in Luna uses a dedicated C/SIMD backend.
- Performance may vary depending on BLAS backend, thread configuration, and CPU scaling.

## Test Environment

- CPU: AMD Ryzen 7 7435HS (8 cores / 16 threads)
- Base / Boost Frequency: 4.5 GHz
- RAM: 24GB DDR5
- OS: Linux Mint (Cinnamon Edition)
- Python Version: Python executed in online environment (exact version unknown)
- NumPy Version: NumPy version unknown (online environment) 
- Site: [Programiz Online Python Compiler](https://www.programiz.com/python-programming/online-compiler/)
- Compiler: gcc
- Compiler Flags: Default
- Threading: No explicit multithreading implemented in Luna

