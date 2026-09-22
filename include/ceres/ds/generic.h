#pragma once

#include "../hash.h"
#include "../../string.h"

// _Generic sugar over ten of the eighteen collections (hset, gmap, multimap, lru, rbtree, skiplist,
// omap, flatmap/flatset, iheap) that all take a hash/eq or a cmp function pointer at init time:
// DS_DEFAULT_HASH(T)/DS_DEFAULT_EQ(T)/DS_DEFAULT_CMP(T) pick the right one for a handful of common
// key types, so a call site does not have to name hash_int/eq_int/cmp_int by hand.
//
//   struct hset s;  hset_init(&s, sizeof(int), 0, DS_DEFAULT_HASH(int), DS_DEFAULT_EQ(int));
//   struct omap m;  omap_init(&m, sizeof(int), sizeof(int), DS_DEFAULT_CMP(int));
//
// Not rbtree.h itself: rb_init's cmp compares two struct rb_node*, unwrapped through rb_entry by a
// comparator the caller writes for their own type - there is no data pointer of a fixed shape for
// this header's macros to hand it. Every OTHER structure that takes a cmp (skiplist, flatmap/
// flatset, iheap) uses the same plain (const void*, const void*) shape as hset/gmap's eq and
// DS_DEFAULT_CMP already targets, so it applies to all of them.
//
// Every association below is a bare function NAME, never a call: _Generic still has to type-check
// every association even when it is not the one selected, so a call inside one - which would need
// to compile against whatever type the controlling expression turns out to have - is exactly the
// mistake to avoid. Naming a function only asks it to decay to a pointer, which needs nothing from
// the argument at all; the one real call happens outside the selection, already on the right
// function. See docs on _Generic in Ceres-C for the full reasoning.
//
// DS_DEFAULT_HASH has a `default:` (ds_hash_bytes, hashing sizeof-many bytes - the length IS
// available, since hash's own signature carries it) for any type not listed below. DS_DEFAULT_EQ
// and DS_DEFAULT_CMP do NOT: eq and cmp take two bare pointers with no length of their own to fall
// back on (unlike hash), so a struct type with no case here is a compile error, on purpose, rather
// than a memcmp/comparison guessing at a size it was never given. Write a one-line eq/cmp for that
// type instead - exactly what the call site would have had to write anyway without this header.
//
// Comparing a float with == (ds_eq_float, and so the same float used as a DS_DEFAULT_HASH(float)
// key) treats +0.0 and -0.0 as equal while hashing their differing bit patterns differently - a
// real, if obscure, way for the hash-table invariant "equal keys hash equally" to break. Treat the
// float defaults as convenience for the ordinary case, not a guarantee for every bit pattern.

static inline unsigned int ds_hash_int(const void* p, unsigned int len)   { (void)len; return hash_fnv1a(p, sizeof(int)); }
static inline unsigned int ds_hash_uint(const void* p, unsigned int len)  { (void)len; return hash_fnv1a(p, sizeof(unsigned int)); }
static inline unsigned int ds_hash_float(const void* p, unsigned int len) { (void)len; return hash_fnv1a(p, sizeof(float)); }
static inline unsigned int ds_hash_cstr(const void* p, unsigned int len)  { (void)len; return hash_str(*(const char* const*)p); }
static inline unsigned int ds_hash_bytes(const void* p, unsigned int len) { return hash_fnv1a(p, len); }

static inline int ds_eq_int(const void* a, const void* b)   { return *(const int*)a == *(const int*)b; }
static inline int ds_eq_uint(const void* a, const void* b)  { return *(const unsigned int*)a == *(const unsigned int*)b; }
static inline int ds_eq_float(const void* a, const void* b) { return *(const float*)a == *(const float*)b; }
static inline int ds_eq_cstr(const void* a, const void* b)  { return strcmp(*(const char* const*)a, *(const char* const*)b) == 0; }

static inline int ds_cmp_int(const void* a, const void* b)
{
    int x = *(const int*)a, y = *(const int*)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}
static inline int ds_cmp_uint(const void* a, const void* b)
{
    unsigned int x = *(const unsigned int*)a, y = *(const unsigned int*)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}
static inline int ds_cmp_float(const void* a, const void* b)
{
    float x = *(const float*)a, y = *(const float*)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}
static inline int ds_cmp_cstr(const void* a, const void* b) { return strcmp(*(const char* const*)a, *(const char* const*)b); }

#define DS_DEFAULT_HASH(T) _Generic((T){0}, \
    int: ds_hash_int, unsigned int: ds_hash_uint, float: ds_hash_float, \
    char*: ds_hash_cstr, const char*: ds_hash_cstr, default: ds_hash_bytes)

#define DS_DEFAULT_EQ(T) _Generic((T){0}, \
    int: ds_eq_int, unsigned int: ds_eq_uint, float: ds_eq_float, \
    char*: ds_eq_cstr, const char*: ds_eq_cstr)

#define DS_DEFAULT_CMP(T) _Generic((T){0}, \
    int: ds_cmp_int, unsigned int: ds_cmp_uint, float: ds_cmp_float, \
    char*: ds_cmp_cstr, const char*: ds_cmp_cstr)
