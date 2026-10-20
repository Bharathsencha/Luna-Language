// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Bharath
//
// Memory management for code emitted by lunac: a conservative, non-moving
// mark-and-sweep collector.
//
// Every runtime object is allocated here. Small objects come from 64 KB pages
// split into size classes; large ones get pages of their own. A collection marks
// everything reachable from the C stack, the registers, the program's static
// data and any registered root ranges, treating every aligned word as a possible
// pointer (interior pointers count), then sweeps the rest. Objects that hold no
// pointers (strings, doubles, bytes) are allocated "atomic" and never scanned.
//
// Environment:
//   LUNA_RT_GC_STRESS=N  collect every N allocations (testing)
//   LUNA_RT_GC_STATS=1   print collection counts at exit

#define _GNU_SOURCE
#include "luna_rt.h"

#include <pthread.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SHIFT 16
#define PAGE_SIZE ((size_t)1 << PAGE_SHIFT)
#define MAX_SMALL 8192
#define MIN_THRESHOLD ((size_t)16 << 20)

typedef struct Page Page;
struct Page {
    char *base;
    size_t obj_size;
    uint32_t nobjs;
    uint32_t bump;       // objects handed out at least once
    uint32_t free_head;  // 1-based index of the first recycled object, 0 if none
    uint32_t nfree;      // recycled objects available
    uint32_t used_count;
    uint8_t atomic;
    uint8_t large;
    uint8_t dead;
    uint8_t in_list;
    int cls;
    size_t span;         // bytes owned by a large page
    uint8_t *used;
    uint8_t *mark;
    Page *next;
};

typedef struct {
    uintptr_t key;
    Page *page;
} Slot;

static const uint32_t class_sizes[] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512,
    640, 768, 1024, 1280, 1536, 2048, 3072, 4096, 6144, 8192
};
#define NCLASSES ((int)(sizeof(class_sizes) / sizeof(class_sizes[0])))

static uint8_t class_of[MAX_SMALL / 16 + 1];
static Page *class_list[2][32];     // pages with room, per [atomic][class]
static Page **all_pages = NULL;
static size_t all_count = 0, all_cap = 0;
static Page **pool = NULL;
static size_t pool_count = 0, pool_cap = 0;

static Slot *table = NULL;
static size_t table_cap = 0, table_used = 0;
static uintptr_t heap_lo = (uintptr_t)-1, heap_hi = 0;

static size_t bytes_since_gc = 0;
static size_t threshold = MIN_THRESHOLD;
static size_t live_bytes = 0;
static int gc_ready = 0;
static int gc_busy = 0;
static uint64_t stress_every = 0, alloc_count = 0, collections = 0;
static int want_stats = 0;
static char *stack_base = NULL;

typedef struct { char *p; size_t n; } MarkItem;
static MarkItem *mstack = NULL;
static size_t mtop = 0, mcap = 0;

typedef struct { char *lo, *hi; } Range;
static Range *roots = NULL;
static size_t root_count = 0, root_cap = 0;

static void die_oom(void) {
    fprintf(stderr, "Runtime Error: out of memory\n");
    exit(1);
}

// ---- page table -------------------------------------------------------------

static size_t slot_hash(uintptr_t k, size_t cap) {
    k ^= k >> 17;
    k *= 0x9E3779B97F4A7C15ULL;
    return (size_t)(k >> 20) & (cap - 1);
}

static void table_insert(uintptr_t key, Page *pg);

static void table_grow(void) {
    Slot *old = table;
    size_t old_cap = table_cap;
    table_cap = table_cap ? table_cap * 2 : 1024;
    table = calloc(table_cap, sizeof(Slot));
    if (!table) die_oom();
    table_used = 0;
    for (size_t i = 0; i < old_cap; i++) {
        if (old[i].page) table_insert(old[i].key, old[i].page);
    }
    free(old);
}

static void table_insert(uintptr_t key, Page *pg) {
    if ((table_used + 1) * 2 > table_cap) table_grow();
    size_t i = slot_hash(key, table_cap);
    while (table[i].page && table[i].key != key) i = (i + 1) & (table_cap - 1);
    if (!table[i].page) table_used++;
    table[i].key = key;
    table[i].page = pg;
}

static Page *table_find(uintptr_t key) {
    if (!table_cap) return NULL;
    size_t i = slot_hash(key, table_cap);
    while (table[i].page) {
        if (table[i].key == key) return table[i].page;
        i = (i + 1) & (table_cap - 1);
    }
    return NULL;
}

// ---- pages ------------------------------------------------------------------

static void note_heap(char *lo, size_t n) {
    if ((uintptr_t)lo < heap_lo) heap_lo = (uintptr_t)lo;
    if ((uintptr_t)lo + n > heap_hi) heap_hi = (uintptr_t)lo + n;
}

static void track_page(Page *pg) {
    if (all_count == all_cap) {
        all_cap = all_cap ? all_cap * 2 : 256;
        all_pages = realloc(all_pages, sizeof(Page *) * all_cap);
        if (!all_pages) die_oom();
    }
    all_pages[all_count++] = pg;
}

static void init_small_page(Page *pg, int cls, int atomic) {
    pg->cls = cls;
    pg->obj_size = class_sizes[cls];
    pg->nobjs = (uint32_t)(PAGE_SIZE / pg->obj_size);
    pg->bump = 0;
    pg->free_head = 0;
    pg->nfree = 0;
    pg->used_count = 0;
    pg->atomic = (uint8_t)atomic;
    pg->large = 0;
    pg->dead = 0;
    pg->in_list = 0;
    free(pg->used);
    free(pg->mark);
    pg->used = calloc(pg->nobjs, 1);
    pg->mark = calloc(pg->nobjs, 1);
    if (!pg->used || !pg->mark) die_oom();
}

static Page *new_small_page(int cls, int atomic) {
    Page *pg = pool_count ? pool[--pool_count] : NULL;
    if (pg) {
        pg->next = NULL;
    } else {
        pg = calloc(1, sizeof(Page));
        char *mem = aligned_alloc(PAGE_SIZE, PAGE_SIZE);
        if (!pg || !mem) die_oom();
        pg->base = mem;
        note_heap(mem, PAGE_SIZE);
        table_insert((uintptr_t)mem >> PAGE_SHIFT, pg);
        track_page(pg);
    }
    init_small_page(pg, cls, atomic);
    return pg;
}

static void collect(void);

static void *alloc_large(size_t n, int atomic) {
    size_t span = (n + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    char *mem = aligned_alloc(PAGE_SIZE, span);
    Page *pg = calloc(1, sizeof(Page));
    if (!mem || !pg) die_oom();
    memset(mem, 0, span);
    pg->base = mem;
    pg->obj_size = n;
    pg->nobjs = 1;
    pg->bump = 1;
    pg->used_count = 1;
    pg->atomic = (uint8_t)atomic;
    pg->large = 1;
    pg->span = span;
    pg->used = calloc(1, 1);
    pg->mark = calloc(1, 1);
    pg->used[0] = 1;
    note_heap(mem, span);
    for (size_t off = 0; off < span; off += PAGE_SIZE) {
        table_insert(((uintptr_t)mem + off) >> PAGE_SHIFT, pg);
    }
    track_page(pg);
    bytes_since_gc += span;
    return mem;
}

static void *gc_alloc(size_t n, int atomic) {
    if (n == 0) n = 1;
    n += 1;  // keeps a pointer just past the end inside the object
    alloc_count++;
    if (gc_ready && !gc_busy) {
        if (bytes_since_gc > threshold || (stress_every && alloc_count % stress_every == 0)) collect();
    }
    if (n > MAX_SMALL) return alloc_large(n, atomic);

    int cls = class_of[(n + 15) / 16];
    Page *pg = class_list[atomic][cls];
    while (pg && pg->free_head == 0 && pg->bump >= pg->nobjs) {
        pg->in_list = 0;
        pg = pg->next;
        class_list[atomic][cls] = pg;
    }
    if (!pg) {
        pg = new_small_page(cls, atomic);
        pg->next = class_list[atomic][cls];
        pg->in_list = 1;
        class_list[atomic][cls] = pg;
    }
    uint32_t idx;
    if (pg->free_head) {
        idx = pg->free_head - 1;
        uint32_t next;
        memcpy(&next, pg->base + (size_t)idx * pg->obj_size, sizeof(next));
        pg->free_head = next;
        pg->nfree--;
    } else {
        idx = pg->bump++;
    }
    pg->used[idx] = 1;
    pg->used_count++;
    char *obj = pg->base + (size_t)idx * pg->obj_size;
    memset(obj, 0, pg->obj_size);
    bytes_since_gc += pg->obj_size;
    return obj;
}

void *lv_alloc(size_t n) { return gc_alloc(n, 0); }
void *lv_alloc_atomic(size_t n) { return gc_alloc(n, 1); }

static Page *page_of(const void *p, uint32_t *idx_out) {
    uintptr_t w = (uintptr_t)p;
    if (w < heap_lo || w >= heap_hi) return NULL;
    Page *pg = table_find(w >> PAGE_SHIFT);
    if (!pg || pg->dead) return NULL;
    uint32_t idx = 0;
    if (pg->large) {
        if (w >= (uintptr_t)pg->base + pg->obj_size) return NULL;
    } else {
        idx = (uint32_t)((w - (uintptr_t)pg->base) / pg->obj_size);
        if (idx >= pg->bump) return NULL;
    }
    if (!pg->used[idx]) return NULL;
    *idx_out = idx;
    return pg;
}

void *lv_realloc(void *p, size_t n) {
    if (!p) return gc_alloc(n, 0);
    uint32_t idx;
    Page *pg = page_of(p, &idx);
    if (!pg) die_oom();
    size_t old = pg->large ? pg->obj_size : pg->obj_size;
    void *q = gc_alloc(n, pg->atomic);
    memcpy(q, p, old < n ? old : n);
    return q;
}

// ---- marking ----------------------------------------------------------------

static void push(char *p, size_t n) {
    if (mtop == mcap) {
        mcap = mcap ? mcap * 2 : 4096;
        mstack = realloc(mstack, sizeof(MarkItem) * mcap);
        if (!mstack) die_oom();
    }
    mstack[mtop].p = p;
    mstack[mtop].n = n;
    mtop++;
}

static void mark_word(uintptr_t w) {
    uint32_t idx;
    Page *pg = page_of((void *)w, &idx);
    if (!pg || pg->mark[idx]) return;
    pg->mark[idx] = 1;
    if (!pg->atomic) {
        char *obj = pg->large ? pg->base : pg->base + (size_t)idx * pg->obj_size;
        push(obj, pg->obj_size);
    }
}

static void scan_range(char *lo, char *hi) {
    uintptr_t a = ((uintptr_t)lo + 7) & ~(uintptr_t)7;
    for (uintptr_t *p = (uintptr_t *)a; (char *)(p + 1) <= hi; p++) mark_word(*p);
}

static void drain(void) {
    while (mtop) {
        MarkItem it = mstack[--mtop];
        scan_range(it.p, it.p + it.n);
    }
}

void lv_gc_root_add(void *lo, void *hi) {
    if (root_count == root_cap) {
        root_cap = root_cap ? root_cap * 2 : 64;
        roots = realloc(roots, sizeof(Range) * root_cap);
        if (!roots) die_oom();
    }
    roots[root_count].lo = lo;
    roots[root_count].hi = hi;
    root_count++;
}

void lv_gc_root_remove(void *lo) {
    for (size_t i = 0; i < root_count; i++) {
        if (roots[i].lo == lo) {
            roots[i] = roots[--root_count];
            return;
        }
    }
}

int lv_gc_is_marked(const void *p) {
    uint32_t idx;
    Page *pg = page_of(p, &idx);
    return pg ? pg->mark[idx] : 1;
}

extern char __data_start[];
extern char _end[];

static __attribute__((noinline)) void scan_stack_and_registers(void) {
    jmp_buf regs;
    __builtin_unwind_init();  // spill callee-saved registers into this frame
    setjmp(regs);
    volatile char probe = 0;
    char *sp = (char *)&probe;
    if ((char *)&regs < sp) sp = (char *)&regs;
    if (sp < stack_base) scan_range(sp, stack_base);
    drain();
}

// ---- sweeping ---------------------------------------------------------------

static void rebuild_lists(void) {
    memset(class_list, 0, sizeof(class_list));
    for (size_t i = 0; i < all_count; i++) {
        Page *pg = all_pages[i];
        if (pg->in_list == 2) continue;
        pg->in_list = 0;
        pg->next = NULL;
    }
}

static void pool_push(Page *pg) {
    if (pool_count == pool_cap) {
        pool_cap = pool_cap ? pool_cap * 2 : 64;
        pool = realloc(pool, sizeof(Page *) * pool_cap);
        if (!pool) die_oom();
    }
    pool[pool_count++] = pg;
}

static void sweep(void) {
    live_bytes = 0;
    rebuild_lists();
    size_t keep = 0;
    for (size_t i = 0; i < all_count; i++) {
        Page *pg = all_pages[i];
        if (pg->dead) continue;
        if (pg->in_list == 2) {
            all_pages[keep++] = pg;
            continue;
        }
        if (pg->large) {
            if (!pg->mark[0]) {
                pg->dead = 1;
                free(pg->base);
                free(pg->used);
                free(pg->mark);
                pg->used = pg->mark = NULL;
                pg->base = NULL;
                continue;
            }
            pg->mark[0] = 0;
            live_bytes += pg->span;
            all_pages[keep++] = pg;
            continue;
        }
        for (uint32_t j = 0; j < pg->bump; j++) {
            if (!pg->used[j]) continue;
            if (pg->mark[j]) {
                pg->mark[j] = 0;
                continue;
            }
            pg->used[j] = 0;
            pg->used_count--;
            memcpy(pg->base + (size_t)j * pg->obj_size, &pg->free_head, sizeof(pg->free_head));
            pg->free_head = j + 1;
            pg->nfree++;
        }
        live_bytes += (size_t)pg->used_count * pg->obj_size;
        all_pages[keep++] = pg;
        if (pg->used_count == 0) {
            // Empty: recycle the whole page for any size class.
            pool_push(pg);
            pg->bump = 0;
            pg->free_head = 0;
            pg->nfree = 0;
            pg->in_list = 2;
            continue;
        }
        if (pg->free_head || pg->bump < pg->nobjs) {
            pg->next = class_list[pg->atomic][pg->cls];
            class_list[pg->atomic][pg->cls] = pg;
            pg->in_list = 1;
        }
    }
    all_count = keep;
}

void lv_data_gc_hook(void);

static void collect(void) {
    gc_busy = 1;
    collections++;
    mtop = 0;
    scan_range(__data_start, _end);
    for (size_t i = 0; i < root_count; i++) scan_range(roots[i].lo, roots[i].hi);
    drain();
    scan_stack_and_registers();
    lv_data_gc_hook();
    sweep();
    bytes_since_gc = 0;
    threshold = live_bytes > MIN_THRESHOLD ? live_bytes : MIN_THRESHOLD;
    gc_busy = 0;
}

void lv_gc_collect(void) {
    if (gc_ready && !gc_busy) collect();
}

static void gc_report(void) {
    fprintf(stderr, "LUNA_RT_GC_STATS,%llu collections,%zu live bytes,%zu pages\n",
            (unsigned long long)collections, live_bytes, all_count);
}

// Called once from the thread that runs the program.
void lv_gc_start(void) {
    pthread_attr_t attr;
    void *addr = NULL;
    size_t size = 0;
    if (pthread_getattr_np(pthread_self(), &attr) == 0) {
        pthread_attr_getstack(&attr, &addr, &size);
        pthread_attr_destroy(&attr);
        stack_base = (char *)addr + size;
    } else {
        stack_base = (char *)__builtin_frame_address(0) + 4096;
    }
    const char *s = getenv("LUNA_RT_GC_STRESS");
    if (s && *s) stress_every = (uint64_t)strtoull(s, NULL, 10);
    if (getenv("LUNA_RT_GC_STATS")) {
        want_stats = 1;
        atexit(gc_report);
    }
    gc_ready = 1;
}

// Size-class lookup table, filled before the first allocation.
__attribute__((constructor)) static void gc_setup(void) {
    int c = 0;
    for (size_t i = 0; i <= MAX_SMALL / 16; i++) {
        while (class_sizes[c] < i * 16) c++;
        class_of[i] = (uint8_t)c;
    }
}
