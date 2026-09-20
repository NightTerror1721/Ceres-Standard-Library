#pragma once

#include "../../stddef.h"

// A growable array of fixed-size elements. There are no templates, so the element size is a run-time
// value and the elements are handled by pointer; VECTOR_GET gives the typed access back.
//
//   struct vector v;  vector_init(&v, sizeof(int));
//   int* slot = (int*)vector_push(&v);  *slot = 7;
//   int first = VECTOR_GET(&v, int, 0);
//   vector_free(&v);
//
// Growing may move the storage: a pointer from vector_at/vector_push is good until the next push, insert
// or reserve.

struct vector
{
    void* data;
    unsigned int len;
    unsigned int cap;
    unsigned int elem;         // bytes per element
};

void  vector_init(struct vector* v, unsigned int elem_size);
void  vector_free(struct vector* v);                            // releases the storage; the vector is empty and usable again
int   vector_reserve(struct vector* v, unsigned int cap);       // room for at least `cap` elements; 0 ok, -1 when out of memory
void* vector_push(struct vector* v);                            // appends a zeroed element and returns it; NULL when out of memory
int   vector_push_copy(struct vector* v, const void* item);     // appends a copy of *item; 0 ok, -1 when out of memory
void* vector_at(const struct vector* v, unsigned int i);        // NULL when i >= len
void* vector_last(const struct vector* v);                      // NULL when empty
void  vector_pop(struct vector* v);                             // drops the last element; nothing when empty
int   vector_insert(struct vector* v, unsigned int i, const void* item);   // before index i (i == len appends); 0 ok, -1 bad index or no memory
void  vector_remove(struct vector* v, unsigned int i);          // shifts the rest down; nothing for a bad index
void  vector_clear(struct vector* v);                           // len = 0, keeps the storage
void  vector_sort(struct vector* v, int (*cmp)(const void*, const void*));   // qsort: not stable

static inline unsigned int vector_len(const struct vector* v) { return v->len; }

// Typed access. i must be < len: it does not check, unlike vector_at.
#define VECTOR_GET(v, T, i) (*((T*)vector_at((v), (i))))
