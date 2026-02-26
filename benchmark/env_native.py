import time

size = 1000000
global_var = 42

def test_lookups():
    start = time.perf_counter()
    local_var = 10
    sum_val = 0
    
    for i in range(size):
        sum_val += local_var + global_var
        
    end = time.perf_counter()
    print(f"Python Env Lookup Time: {end - start:.4f} seconds")

test_lookups()
