#pragma once

#include "../stddef.h"

// A stable sort: elements that compare equal keep the order they had, which qsort does not promise.
// Sort a table by one column, then by another with this, and rows equal in the second stay ordered by
// the first. A merge sort: O(n log n) comparisons for every input, and a sorted or nearly sorted array
// costs about n. It borrows a buffer of n * size bytes from malloc for the time of the call.
//
// Returns 0, or -1 (with the array untouched) when the buffer cannot be had.
int sort_stable(void* base, size_t n, size_t size, int (*cmp)(const void*, const void*));
