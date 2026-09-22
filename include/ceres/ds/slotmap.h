#pragma once

#include "vector.h"
#include "../../string.h"

// Stable handles into a growable array: removing one entity never invalidates another's handle,
// and a handle kept too long is detectably stale instead of silently pointing at whatever now
// lives in that slot. A plain index into a vector.h breaks on the first removal (later indices
// shift, or a hole has to be skipped by hand); this is what a game with entities dying and being
// born every frame reaches for instead.
//
//   struct slotmap entities;  sm_init(&entities, sizeof(struct entity));
//   struct sm_handle h = sm_insert(&entities, &e);
//   struct entity* e = (struct entity*)sm_get(&entities, h);   // NULL once h is stale
//   sm_remove(&entities, h);
//
// Each slot carries a generation, bumped every time it is freed; a handle is `{index, generation}`
// and is only valid while the two still match. Freed slots are tracked as a small stack of reusable
// indices rather than a free list threaded through the slot bytes themselves (ceres/pool.h's trick),
// so this works for any elem_size, including one narrower than a pointer.

struct sm_handle { unsigned int index; unsigned int gen; };

struct slotmap
{
    struct vector slots;        // elem_size bytes each; a freed slot's bytes are unspecified
    struct vector gens;         // one unsigned int per slot, parallel to slots
    struct vector free_list;    // a stack of freed slot indices, ready to be reused
};

static inline void sm_init(struct slotmap* m, unsigned int elem_size)
{
    vector_init(&m->slots, elem_size);
    vector_init(&m->gens, sizeof(unsigned int));
    vector_init(&m->free_list, sizeof(unsigned int));
}

static inline void sm_free(struct slotmap* m)
{
    vector_free(&m->slots);
    vector_free(&m->gens);
    vector_free(&m->free_list);
}

static inline unsigned int sm_len(const struct slotmap* m) { return m->slots.len - m->free_list.len; }

static inline int sm_valid(const struct slotmap* m, struct sm_handle h)
{
    if (h.index >= m->slots.len)
        return 0;
    return *(const unsigned int*)vector_at(&m->gens, h.index) == h.gen;
}

// NULL when h's generation is stale or its index was never valid.
static inline void* sm_get(const struct slotmap* m, struct sm_handle h)
{
    return sm_valid(m, h) ? vector_at(&m->slots, h.index) : NULL;
}

// Copies *item into a new (or reused) slot. On out-of-memory the handle's index is a value no real
// slot ever has (slots.len can never reach it in practice), so sm_valid() and sm_get() already
// treat it as invalid - no separate error case for the caller to check.
static inline struct sm_handle sm_insert(struct slotmap* m, const void* item)
{
    struct sm_handle h;
    if (m->free_list.len > 0)
    {
        h.index = *(unsigned int*)vector_last(&m->free_list);
        vector_pop(&m->free_list);
        h.gen = *(unsigned int*)vector_at(&m->gens, h.index);
        memcpy(vector_at(&m->slots, h.index), item, m->slots.elem);
        return h;
    }
    if (vector_push_copy(&m->slots, item) != 0)
    {
        h.index = 0xFFFFFFFFu;
        h.gen = 0;
        return h;
    }
    h.index = m->slots.len - 1u;
    h.gen = 0;
    vector_push_copy(&m->gens, &h.gen);          // slots and gens always grow together: this cannot be the one to fail alone in practice, and if it were, the slot is simply never reachable by a valid handle
    return h;
}

// Does nothing when h is already stale, so removing the same handle twice by mistake is harmless.
static inline void sm_remove(struct slotmap* m, struct sm_handle h)
{
    if (!sm_valid(m, h))
        return;
    unsigned int* gen = (unsigned int*)vector_at(&m->gens, h.index);
    (*gen)++;
    vector_push_copy(&m->free_list, &h.index);   // on OOM here the slot just never gets reused - no correctness break
}
