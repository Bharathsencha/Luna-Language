// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2025 Bharath

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list_lib.h"
#include "value.h"
#include "math_lib.h" // For math_internal_next()
#include "luna_error.h"

// Threshold for switching from Merge Sort to Insertion Sort
#define SORT_THRESHOLD 16

// Helper: Check if a < b for Luna Values
static int value_less_than(Value a, Value b) {
    if (a.type == VAL_INT && b.type == VAL_INT) return a.i < b.i;
    if (a.type == VAL_FLOAT && b.type == VAL_FLOAT) return a.f < b.f;
    if (a.type == VAL_INT && b.type == VAL_FLOAT) return (double)a.i < b.f;
    if (a.type == VAL_FLOAT && b.type == VAL_INT) return a.f < (double)b.i;
    if (a.type == VAL_STRING && b.type == VAL_STRING) return strcmp(a.s, b.s) < 0;
    return 0; 
}

// Insertion Sort (Used for small sub-lists)
static void insertion_sort(Value *items, int left, int right) {
    for (int i = left + 1; i <= right; i++) {
        Value key = items[i];
        int j = i - 1;
        while (j >= left && value_less_than(key, items[j])) {
            items[j + 1] = items[j];
            j--;
        }
        items[j + 1] = key;
    }
}

// Merge
static void merge(Value *items, int l, int m, int r) {
    int n1 = m - l + 1;
    int n2 = r - m;

    Value *L = malloc(n1 * sizeof(Value));
    Value *R = malloc(n2 * sizeof(Value));

    for (int i = 0; i < n1; i++) L[i] = items[l + i];
    for (int j = 0; j < n2; j++) R[j] = items[m + 1 + j];

    int i = 0, j = 0, k = l;
    while (i < n1 && j < n2) {
        if (value_less_than(L[i], R[j]) || (!value_less_than(R[j], L[i]))) {
            items[k++] = L[i++];
        } else {
            items[k++] = R[j++];
        }
    }

    while (i < n1) items[k++] = L[i++];
    while (j < n2) items[k++] = R[j++];

    free(L);
    free(R);
}

// Recursive Hybrid Sort Logic
static void hybrid_sort(Value *items, int l, int r) {
    if (l < r) {
        // Switch to Insertion Sort for small segments
        if (r - l < SORT_THRESHOLD) {
            insertion_sort(items, l, r);
            return;
        }

        int m = l + (r - l) / 2;
        hybrid_sort(items, l, m);
        hybrid_sort(items, m + 1, r);
        merge(items, l, m, r);
    }
}

Value lib_list_sort(int argc, Value *argv) {
    if (argc != 1 || argv[0].type != VAL_LIST) {
        error_report(ERR_ARGUMENT, 0, 0, "sort() expects 1 list", "Usage: sort(myList)");
        return value_null();
    }

    Value list = argv[0];
    if (list.list.count > 1) {
        hybrid_sort(list.list.items, 0, list.list.count - 1);
    }
    return value_null();
}

// Fisher-Yates Shuffle Implementation
//It should work now I suppose
Value lib_list_shuffle(int argc, Value *argv) {
    if (argc != 1 || argv[0].type != VAL_LIST) {
        error_report(ERR_ARGUMENT, 0, 0, "shuffle() expects 1 list", "Usage: shuffle(myList)");
        return value_null();
    }

    Value list = argv[0];
    int n = list.list.count;
    if (n <= 1) return value_null();

    for (int i = n - 1; i > 0; i--) {
        // Pick random index using xoroshiro128++ engine
        int j = (int)(math_internal_next() % (i + 1));
        
        // Swap
        Value temp = list.list.items[i];
        list.list.items[i] = list.list.items[j];
        list.list.items[j] = temp;
    }
    
    return value_null();
}