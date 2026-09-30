#pragma once
// The binary-heap sift, shared by ceres/ds/pqueue.h (elements inline in a vector) and ceres/ds/iheap.h
// (handles into an items array): both are the same algorithm over a different "element at heap slot i".
// A caller supplies two macros: LESS(i, j) < 0 means heap slot i leaves first (its comparator), and
// SWAP(i, j) exchanges two slots. `LESS` and `SWAP` are last in both signatures, so only SIFT_DOWN carries
// the extra `n`. The locals are prefixed `ceres_heap_` so they cannot shadow a caller's own names. NOT
// installed in include/.

#define HEAP_SIFT_UP(i, LESS, SWAP) \
    do { \
        unsigned int ceres_heap_i = (i); \
        while (ceres_heap_i > 0) \
        { \
            unsigned int ceres_heap_p = (ceres_heap_i - 1u) / 2u; \
            if (LESS(ceres_heap_i, ceres_heap_p) >= 0) \
                break; \
            SWAP(ceres_heap_i, ceres_heap_p); \
            ceres_heap_i = ceres_heap_p; \
        } \
    } while (0)

#define HEAP_SIFT_DOWN(i, n, LESS, SWAP) \
    do { \
        unsigned int ceres_heap_i = (i), ceres_heap_n = (n); \
        for (;;) \
        { \
            unsigned int ceres_heap_l = 2u * ceres_heap_i + 1u; \
            unsigned int ceres_heap_r = ceres_heap_l + 1u; \
            unsigned int ceres_heap_best = ceres_heap_i; \
            if (ceres_heap_l < ceres_heap_n && LESS(ceres_heap_l, ceres_heap_best) < 0) \
                ceres_heap_best = ceres_heap_l; \
            if (ceres_heap_r < ceres_heap_n && LESS(ceres_heap_r, ceres_heap_best) < 0) \
                ceres_heap_best = ceres_heap_r; \
            if (ceres_heap_best == ceres_heap_i) \
                break; \
            SWAP(ceres_heap_i, ceres_heap_best); \
            ceres_heap_i = ceres_heap_best; \
        } \
    } while (0)
