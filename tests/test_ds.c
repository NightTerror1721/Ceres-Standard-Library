// The data structures: intrusive list, vector, byte ring, string hash map, priority queue, bit set and
// string builder. Each is checked against what a plain array or a hand count says, and the heap is
// checked to be exactly as it was once everything has been freed.
#include "ceres/test.h"
#include "ceres/heap.h"
#include "ceres/rand.h"
#include "ceres/ds/list.h"
#include "ceres/ds/vector.h"
#include "ceres/ds/ringbuf.h"
#include "ceres/ds/hashmap.h"
#include "ceres/ds/bitset.h"
#include "ceres/ds/pqueue.h"
#include "ceres/ds/strbuf.h"

// ---- list ----

struct job { int id; struct list_node link; };

static void lists(void)
{
    struct list l;
    struct job a, b, c, d;
    a.id = 1; b.id = 2; c.id = 3; d.id = 4;
    list_node_init(&a.link); list_node_init(&b.link); list_node_init(&c.link); list_node_init(&d.link);

    TEST_SECTION("list: empty");
    list_init(&l);
    CHECK(list_empty(&l));
    CHECK_EQ((int)list_count(&l), 0);
    CHECK(list_first(&l) == NULL);
    CHECK(list_last(&l) == NULL);
    CHECK(list_pop_front(&l) == NULL);
    CHECK(list_pop_back(&l) == NULL);

    TEST_SECTION("list: order");
    list_push_back(&l, &b.link);                        // b
    list_push_back(&l, &c.link);                        // b c
    list_push_front(&l, &a.link);                       // a b c
    list_push_back(&l, &d.link);                        // a b c d
    CHECK_EQ((int)list_count(&l), 4);
    CHECK(!list_empty(&l));
    struct list_node* it;
    int order[4], n = 0;
    LIST_FOR_EACH(it, &l)
        order[n++] = list_entry(it, struct job, link)->id;
    CHECK_EQ(n, 4);
    CHECK(order[0] == 1 && order[1] == 2 && order[2] == 3 && order[3] == 4);
    CHECK_EQ(list_entry(list_first(&l), struct job, link)->id, 1);
    CHECK_EQ(list_entry(list_last(&l), struct job, link)->id, 4);

    TEST_SECTION("list: remove");
    list_remove(&l, &b.link);                           // a c d
    CHECK_EQ((int)list_count(&l), 3);
    list_remove(&l, &b.link);                           // already out: nothing happens
    CHECK_EQ((int)list_count(&l), 3);
    n = 0;
    LIST_FOR_EACH(it, &l)
        order[n++] = list_entry(it, struct job, link)->id;
    CHECK(n == 3 && order[0] == 1 && order[1] == 3 && order[2] == 4);
    list_insert_after(&l, &a.link, &b.link);            // back between a and c
    CHECK_EQ(list_entry(a.link.next, struct job, link)->id, 2);

    TEST_SECTION("list: pop");
    CHECK_EQ(list_entry(list_pop_front(&l), struct job, link)->id, 1);
    CHECK_EQ(list_entry(list_pop_back(&l), struct job, link)->id, 4);
    CHECK_EQ((int)list_count(&l), 2);
    CHECK(a.link.next == &a.link);                      // a popped node is a clean, unlinked node again

    TEST_SECTION("list: removing while walking");
    list_init(&l);
    struct job many[8];
    for (int i = 0; i < 8; i++)
    {
        many[i].id = i;
        list_push_back(&l, &many[i].link);
    }
    struct list_node* nx;
    LIST_FOR_EACH_SAFE(it, nx, &l)
        if (list_entry(it, struct job, link)->id % 2 == 0)
            list_remove(&l, it);
    CHECK_EQ((int)list_count(&l), 4);
    n = 0;
    LIST_FOR_EACH(it, &l)
        order[n++] = list_entry(it, struct job, link)->id;
    CHECK(n == 4 && order[0] == 1 && order[1] == 3);
    LIST_FOR_EACH_SAFE(it, nx, &l)                      // and all of them
        list_remove(&l, it);
    CHECK(list_empty(&l));
}

// ---- vector ----

struct point { int x; int y; char tag; };

static int cmp_int(const void* a, const void* b)
{
    int x = *(const int*)a, y = *(const int*)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

static int cmp_point_y(const void* a, const void* b)
{
    return ((const struct point*)a)->y - ((const struct point*)b)->y;
}

static void vectors(void)
{
    struct vector v;

    TEST_SECTION("vector: push and access");
    vector_init(&v, sizeof(int));
    CHECK_EQ((int)vector_len(&v), 0);
    CHECK(vector_at(&v, 0) == NULL);
    CHECK(vector_last(&v) == NULL);
    for (int i = 0; i < 100; i++)
    {
        int* slot = (int*)vector_push(&v);
        CHECK(slot != NULL);
        CHECK_EQ(*slot, 0);                             // a new element starts zeroed
        *slot = i * 3;
    }
    CHECK_EQ((int)v.len, 100);
    CHECK(v.cap >= 100);
    CHECK_EQ(VECTOR_GET(&v, int, 0), 0);
    CHECK_EQ(VECTOR_GET(&v, int, 57), 171);
    CHECK_EQ(*(int*)vector_last(&v), 297);
    CHECK(vector_at(&v, 100) == NULL);                  // one past the end
    CHECK(vector_at(&v, 0xFFFFFFFFu) == NULL);
    int intact = 1;
    for (unsigned int i = 0; i < v.len; i++)            // growing moved the storage without losing anything
        if (VECTOR_GET(&v, int, i) != (int)i * 3) intact = 0;
    CHECK(intact);

    TEST_SECTION("vector: copy in, pop, clear");
    int seven = 7;
    CHECK_EQ(vector_push_copy(&v, &seven), 0);
    CHECK_EQ(*(int*)vector_last(&v), 7);
    vector_pop(&v);
    CHECK_EQ((int)v.len, 100);
    vector_clear(&v);
    CHECK_EQ((int)v.len, 0);
    CHECK(v.cap >= 100);                                // the storage stays
    vector_pop(&v);                                     // popping an empty vector does nothing
    CHECK_EQ((int)v.len, 0);

    TEST_SECTION("vector: insert and remove");
    for (int i = 0; i < 5; i++)
        vector_push_copy(&v, &i);                       // 0 1 2 3 4
    int ten = 10;
    CHECK_EQ(vector_insert(&v, 0, &ten), 0);            // 10 0 1 2 3 4
    CHECK_EQ(vector_insert(&v, 3, &ten), 0);            // 10 0 1 10 2 3 4
    CHECK_EQ(vector_insert(&v, v.len, &ten), 0);        // at the end: an append
    CHECK_EQ(vector_insert(&v, v.len + 1, &ten), -1);   // past the end: refused
    CHECK_EQ((int)v.len, 8);
    int want1[8] = { 10, 0, 1, 10, 2, 3, 4, 10 };
    int same = 1;
    for (int i = 0; i < 8; i++) if (VECTOR_GET(&v, int, i) != want1[i]) same = 0;
    CHECK(same);
    vector_remove(&v, 0);                               // 0 1 10 2 3 4 10
    vector_remove(&v, 2);                               // 0 1 2 3 4 10
    vector_remove(&v, v.len - 1);                       // 0 1 2 3 4
    vector_remove(&v, 99);                              // nothing
    CHECK_EQ((int)v.len, 5);
    for (int i = 0; i < 5; i++) CHECK_EQ(VECTOR_GET(&v, int, i), i);

    TEST_SECTION("vector: sort");
    vector_clear(&v);
    struct rng r;
    rng_seed(&r, 99);
    for (int i = 0; i < 200; i++)
    {
        int x = rng_range(&r, -50, 50);
        vector_push_copy(&v, &x);
    }
    vector_sort(&v, cmp_int);
    int sorted = 1;
    for (unsigned int i = 1; i < v.len; i++)
        if (VECTOR_GET(&v, int, i - 1) > VECTOR_GET(&v, int, i)) sorted = 0;
    CHECK(sorted);
    vector_clear(&v);
    vector_sort(&v, cmp_int);                           // empty: nothing to do
    int one = 5;
    vector_push_copy(&v, &one);
    vector_sort(&v, cmp_int);
    CHECK_EQ(VECTOR_GET(&v, int, 0), 5);

    TEST_SECTION("vector: reserve");
    unsigned int before = v.cap;
    CHECK_EQ(vector_reserve(&v, 10), 0);                // already enough
    CHECK_EQ((int)v.cap, (int)before);
    CHECK_EQ(vector_reserve(&v, 1000), 0);
    CHECK(v.cap >= 1000);
    CHECK_EQ(VECTOR_GET(&v, int, 0), 5);                // the contents came along
    CHECK_EQ(vector_reserve(&v, 0x7FFFFFFFu), -1);      // more than there is: refused, and the vector still works
    CHECK_EQ(vector_reserve(&v, 0xFFFFFFFFu), -1);      // and a byte count that overflows
    CHECK_EQ(VECTOR_GET(&v, int, 0), 5);
    vector_free(&v);
    CHECK(v.data == NULL);
    CHECK_EQ((int)v.len, 0);
    vector_free(&v);                                    // twice is fine

    TEST_SECTION("vector: structs");
    struct vector pts;
    vector_init(&pts, sizeof(struct point));
    for (int i = 0; i < 20; i++)
    {
        struct point* p = (struct point*)vector_push(&pts);
        p->x = i;
        p->y = (i * 7) % 20;
        p->tag = (char)('a' + i);
    }
    vector_sort(&pts, cmp_point_y);
    int ordered = 1, whole = 1;
    for (unsigned int i = 0; i < pts.len; i++)
    {
        struct point* p = (struct point*)vector_at(&pts, i);
        if (i > 0 && ((struct point*)vector_at(&pts, i - 1))->y > p->y) ordered = 0;
        if (p->tag != 'a' + p->x || p->y != (p->x * 7) % 20) whole = 0;    // each record moved as a whole
    }
    CHECK(ordered);
    CHECK(whole);
    vector_free(&pts);
}

// ---- ring buffer ----

static void rings(void)
{
    unsigned char store[8];
    struct ringbuf q;

    TEST_SECTION("ring: fill and drain");
    ring_init(&q, store, 8);
    CHECK_EQ((int)ring_count(&q), 0);
    CHECK_EQ((int)ring_space(&q), 8);
    CHECK_EQ(ring_get(&q), -1);
    CHECK_EQ(ring_peek(&q), -1);
    for (int i = 0; i < 8; i++)
        CHECK_EQ(ring_put(&q, (unsigned char)(10 + i)), 0);
    CHECK_EQ((int)ring_count(&q), 8);                   // every byte of the buffer is usable
    CHECK_EQ((int)ring_space(&q), 0);
    CHECK_EQ(ring_put(&q, 99), -1);                     // full: refused, and nothing is lost
    CHECK_EQ(ring_peek(&q), 10);
    for (int i = 0; i < 8; i++)
        CHECK_EQ(ring_get(&q), 10 + i);
    CHECK_EQ(ring_get(&q), -1);

    TEST_SECTION("ring: wrapping");
    int ok = 1;
    for (int round = 0; round < 50; round++)            // the indices run far past the buffer size
    {
        for (int i = 0; i < 5; i++) ring_put(&q, (unsigned char)(round + i));
        for (int i = 0; i < 5; i++) if (ring_get(&q) != ((round + i) & 255)) ok = 0;
    }
    CHECK(ok);
    CHECK_EQ((int)ring_count(&q), 0);

    TEST_SECTION("ring: bytes 128 and up come back unsigned");
    ring_put(&q, 200);
    ring_put(&q, 255);
    CHECK_EQ(ring_get(&q), 200);
    CHECK_EQ(ring_get(&q), 255);

    TEST_SECTION("ring: blocks and clear");
    CHECK_EQ((int)ring_write(&q, "abcdefghij", 10), 8);  // only 8 fit
    CHECK_EQ((int)ring_write(&q, "x", 1), 0);
    char out[16];
    CHECK_EQ((int)ring_read(&q, out, 3), 3);
    CHECK(out[0] == 'a' && out[1] == 'b' && out[2] == 'c');
    CHECK_EQ((int)ring_count(&q), 5);
    CHECK_EQ((int)ring_write(&q, "XYZ", 3), 3);         // wraps around the end of the store
    CHECK_EQ((int)ring_read(&q, out, 16), 8);           // asked for more than there is
    CHECK(memcmp(out, "defghXYZ", 8) == 0);
    CHECK_EQ((int)ring_read(&q, out, 4), 0);
    ring_put(&q, 1);
    ring_put(&q, 2);
    ring_clear(&q);
    CHECK_EQ((int)ring_count(&q), 0);
    CHECK_EQ(ring_get(&q), -1);

    TEST_SECTION("ring: a size that is not a power of two rounds down");
    unsigned char odd[20];
    struct ringbuf o;
    ring_init(&o, odd, 20);                             // 16
    CHECK_EQ((int)ring_space(&o), 16);
    ring_init(&o, odd, 1);
    CHECK_EQ((int)ring_space(&o), 1);
    CHECK_EQ(ring_put(&o, 5), 0);
    CHECK_EQ(ring_put(&o, 6), -1);
    CHECK_EQ(ring_get(&o), 5);
}

// ---- hash map ----

static int visits;
static int visit_sum;
static void visit(const char* key, void* value, void* ctx)
{
    visits++;
    visit_sum += (int)value;
    if (ctx != NULL)
        (*(int*)ctx)++;
}

static void maps(void)
{
    struct hashmap m;
    char key[16];

    TEST_SECTION("map: basics");
    CHECK_EQ(hashmap_init(&m, 0), 0);
    CHECK_EQ((int)m.cap, 8);
    CHECK_EQ((int)hashmap_len(&m), 0);
    CHECK(hashmap_get(&m, "nothing") == NULL);
    CHECK_EQ(hashmap_has(&m, "nothing"), 0);
    CHECK_EQ(hashmap_remove(&m, "nothing"), 0);
    CHECK_EQ(hashmap_set(&m, "one", (void*)1), 0);
    CHECK_EQ(hashmap_set(&m, "two", (void*)2), 0);
    CHECK_EQ(hashmap_set(&m, "", (void*)3), 0);          // the empty string is a key like any other
    CHECK_EQ((int)hashmap_get(&m, "one"), 1);
    CHECK_EQ((int)hashmap_get(&m, "two"), 2);
    CHECK_EQ((int)hashmap_get(&m, ""), 3);
    CHECK_EQ((int)hashmap_len(&m), 3);
    CHECK(hashmap_get(&m, "three") == NULL);
    CHECK(hashmap_get(&m, "on") == NULL);                // a prefix is not the key
    CHECK(hashmap_get(&m, "ones") == NULL);

    TEST_SECTION("map: replace, NULL values");
    CHECK_EQ(hashmap_set(&m, "one", (void*)11), 0);
    CHECK_EQ((int)hashmap_get(&m, "one"), 11);
    CHECK_EQ((int)hashmap_len(&m), 3);                  // replaced, not added
    CHECK_EQ(hashmap_set(&m, "null", NULL), 0);
    CHECK(hashmap_get(&m, "null") == NULL);
    CHECK_EQ(hashmap_has(&m, "null"), 1);               // present, though its value is NULL
    CHECK_EQ(hashmap_has(&m, "nul"), 0);

    TEST_SECTION("map: the key is copied");
    strcpy(key, "mutable");
    hashmap_set(&m, key, (void*)5);
    strcpy(key, "changed");
    CHECK_EQ((int)hashmap_get(&m, "mutable"), 5);
    CHECK(hashmap_get(&m, "changed") == NULL);

    TEST_SECTION("map: remove");
    CHECK_EQ(hashmap_remove(&m, "two"), 1);
    CHECK_EQ(hashmap_remove(&m, "two"), 0);
    CHECK(hashmap_get(&m, "two") == NULL);
    CHECK_EQ(hashmap_has(&m, "two"), 0);
    CHECK_EQ((int)hashmap_get(&m, "one"), 11);           // the others are still found
    CHECK_EQ(hashmap_set(&m, "two", (void*)22), 0);      // and the key can come back
    CHECK_EQ((int)hashmap_get(&m, "two"), 22);
    hashmap_free(&m);
    CHECK(m.slots == NULL);
    CHECK_EQ((int)hashmap_len(&m), 0);
    hashmap_free(&m);                                   // twice is fine

    TEST_SECTION("map: growing");
    hashmap_init(&m, 8);
    for (int i = 0; i < 1000; i++)
    {
        sprintf(key, "key%d", i);
        CHECK_EQ(hashmap_set(&m, key, (void*)(i + 1)), 0);
    }
    CHECK_EQ((int)hashmap_len(&m), 1000);
    CHECK(m.cap >= 1024);                               // never more than 70% full
    CHECK((m.len * 10) <= m.cap * 7);
    int found = 1;
    for (int i = 0; i < 1000; i++)
    {
        sprintf(key, "key%d", i);
        if ((int)hashmap_get(&m, key) != i + 1) found = 0;
    }
    CHECK(found);
    CHECK(hashmap_get(&m, "key1000") == NULL);

    TEST_SECTION("map: each");
    visits = 0;
    visit_sum = 0;
    int counted = 0;
    hashmap_each(&m, visit, &counted);
    CHECK_EQ(visits, 1000);
    CHECK_EQ(counted, 1000);
    CHECK_EQ(visit_sum, 500500);                        // 1 + 2 + ... + 1000: each entry exactly once

    TEST_SECTION("map: remove many, then reuse");
    int removed_ok = 1;
    for (int i = 0; i < 1000; i += 2)
    {
        sprintf(key, "key%d", i);
        if (hashmap_remove(&m, key) != 1) removed_ok = 0;
    }
    CHECK(removed_ok);
    CHECK_EQ((int)hashmap_len(&m), 500);
    int rest = 1;
    for (int i = 0; i < 1000; i++)
    {
        sprintf(key, "key%d", i);
        int expect = (i % 2 == 0) ? 0 : i + 1;
        if ((int)hashmap_get(&m, key) != expect) rest = 0;
    }
    CHECK(rest);
    for (int round = 0; round < 40; round++)            // churn: add and remove in turn, so tombstones pile up
    {
        for (int i = 0; i < 100; i++)
        {
            sprintf(key, "tmp%d_%d", round, i);
            hashmap_set(&m, key, (void*)1);
        }
        for (int i = 0; i < 100; i++)
        {
            sprintf(key, "tmp%d_%d", round, i);
            hashmap_remove(&m, key);
        }
    }
    CHECK_EQ((int)hashmap_len(&m), 500);
    CHECK(m.used * 10 <= m.cap * 8);                    // tombstones did not fill the table
    CHECK_EQ((int)hashmap_get(&m, "key999"), 1000);

    TEST_SECTION("map: clear");
    hashmap_clear(&m);
    CHECK_EQ((int)hashmap_len(&m), 0);
    CHECK(hashmap_get(&m, "key999") == NULL);
    CHECK_EQ(hashmap_set(&m, "after", (void*)9), 0);
    CHECK_EQ((int)hashmap_get(&m, "after"), 9);
    visits = 0;
    hashmap_each(&m, visit, NULL);
    CHECK_EQ(visits, 1);
    hashmap_free(&m);

    TEST_SECTION("map: keys that collide");
    hashmap_init(&m, 8);
    // "Aa" and "B@" hash alike under djb2 (65*33+97 == 66*33+64), and so do longer strings built from them.
    CHECK_EQ(hashmap_set(&m, "Aa", (void*)1), 0);
    CHECK_EQ(hashmap_set(&m, "B@", (void*)2), 0);
    CHECK_EQ(hashmap_set(&m, "AaAa", (void*)3), 0);
    CHECK_EQ(hashmap_set(&m, "B@B@", (void*)4), 0);
    CHECK_EQ(hashmap_set(&m, "AaB@", (void*)5), 0);
    CHECK_EQ(hashmap_set(&m, "B@Aa", (void*)6), 0);
    CHECK_EQ((int)hashmap_get(&m, "Aa"), 1);
    CHECK_EQ((int)hashmap_get(&m, "B@"), 2);
    CHECK_EQ((int)hashmap_get(&m, "AaAa"), 3);
    CHECK_EQ((int)hashmap_get(&m, "B@B@"), 4);
    CHECK_EQ((int)hashmap_get(&m, "AaB@"), 5);
    CHECK_EQ((int)hashmap_get(&m, "B@Aa"), 6);
    CHECK_EQ(hashmap_remove(&m, "B@"), 1);              // removing from the middle of a probe chain
    CHECK_EQ((int)hashmap_get(&m, "Aa"), 1);
    CHECK_EQ((int)hashmap_get(&m, "AaAa"), 3);
    CHECK(hashmap_get(&m, "B@") == NULL);
    hashmap_free(&m);
}

// ---- priority queue ----

struct task { int due; int id; };

static int cmp_task(const void* a, const void* b)
{
    return ((const struct task*)a)->due - ((const struct task*)b)->due;
}

static int cmp_max(const void* a, const void* b)
{
    return cmp_int(b, a);
}

static void queues(void)
{
    struct pqueue q;

    TEST_SECTION("pqueue: order");
    pq_init(&q, sizeof(int), cmp_int);
    CHECK_EQ((int)pq_len(&q), 0);
    CHECK(pq_peek(&q) == NULL);
    int out = -1;
    CHECK_EQ(pq_pop(&q, &out), -1);
    CHECK_EQ(out, -1);                                  // an empty queue leaves *out alone
    int in[10] = { 5, 3, 8, 1, 9, 2, 7, 3, 6, 4 };
    for (int i = 0; i < 10; i++)
        CHECK_EQ(pq_push(&q, &in[i]), 0);
    CHECK_EQ((int)pq_len(&q), 10);
    CHECK_EQ(*(const int*)pq_peek(&q), 1);
    int got[10];
    for (int i = 0; i < 10; i++)
        CHECK_EQ(pq_pop(&q, &got[i]), 0);
    int asc[10] = { 1, 2, 3, 3, 4, 5, 6, 7, 8, 9 };      // with the duplicate
    int match = 1;
    for (int i = 0; i < 10; i++) if (got[i] != asc[i]) match = 0;
    CHECK(match);
    CHECK_EQ((int)pq_len(&q), 0);
    CHECK_EQ(pq_pop(&q, &out), -1);

    TEST_SECTION("pqueue: a max queue, and popping without taking the value");
    pq_free(&q);
    pq_init(&q, sizeof(int), cmp_max);
    for (int i = 0; i < 10; i++) pq_push(&q, &in[i]);
    CHECK_EQ(*(const int*)pq_peek(&q), 9);
    CHECK_EQ(pq_pop(&q, NULL), 0);                      // discard the largest
    CHECK_EQ(*(const int*)pq_peek(&q), 8);
    pq_clear(&q);
    CHECK_EQ((int)pq_len(&q), 0);
    pq_free(&q);

    TEST_SECTION("pqueue: records and a long random run");
    struct pqueue t;
    pq_init(&t, sizeof(struct task), cmp_task);
    struct rng r;
    rng_seed(&r, 31);
    for (int i = 0; i < 300; i++)
    {
        struct task k;
        k.due = rng_range(&r, 0, 1000);
        k.id = i;
        pq_push(&t, &k);
    }
    int prev = -1, in_order = 1, count = 0;
    struct task k;
    while (pq_pop(&t, &k) == 0)
    {
        if (k.due < prev) in_order = 0;
        prev = k.due;
        count++;
    }
    CHECK(in_order);
    CHECK_EQ(count, 300);
    for (int i = 0; i < 50; i++)                        // interleaved pushes and pops keep it ordered
    {
        k.due = 100 - i; k.id = i;
        pq_push(&t, &k);
        if (i % 3 == 2) pq_pop(&t, &k);
    }
    prev = -1;
    in_order = 1;
    while (pq_pop(&t, &k) == 0)
    {
        if (k.due < prev) in_order = 0;
        prev = k.due;
    }
    CHECK(in_order);
    pq_free(&t);
}

// ---- bit set ----

static void bitsets(void)
{
    unsigned int words[4];
    struct bitset s;

    TEST_SECTION("bitset: single bits");
    CHECK_EQ((int)bitset_words(0), 0);
    CHECK_EQ((int)bitset_words(1), 1);
    CHECK_EQ((int)bitset_words(32), 1);
    CHECK_EQ((int)bitset_words(33), 2);
    bitset_init(&s, words, 100);
    CHECK_EQ((int)bitset_count(&s), 0);
    CHECK_EQ(bitset_find_first_set(&s), -1);
    CHECK_EQ(bitset_find_first_clear(&s), 0);
    bitset_set(&s, 0);
    bitset_set(&s, 31);
    bitset_set(&s, 32);
    bitset_set(&s, 99);
    CHECK_EQ(bitset_get(&s, 0), 1);
    CHECK_EQ(bitset_get(&s, 1), 0);
    CHECK_EQ(bitset_get(&s, 31), 1);
    CHECK_EQ(bitset_get(&s, 32), 1);
    CHECK_EQ(bitset_get(&s, 33), 0);
    CHECK_EQ(bitset_get(&s, 99), 1);
    CHECK_EQ((int)bitset_count(&s), 4);
    CHECK_EQ(bitset_find_first_set(&s), 0);
    bitset_clear(&s, 0);
    CHECK_EQ(bitset_find_first_set(&s), 31);
    bitset_set(&s, 31);                                 // setting twice changes nothing
    CHECK_EQ((int)bitset_count(&s), 3);
    bitset_toggle(&s, 5);
    CHECK_EQ(bitset_get(&s, 5), 1);
    bitset_toggle(&s, 5);
    CHECK_EQ(bitset_get(&s, 5), 0);
    bitset_clear_all(&s);
    CHECK_EQ((int)bitset_count(&s), 0);

    TEST_SECTION("bitset: finding a free bit");
    bitset_init(&s, words, 70);
    for (int i = 0; i < 70; i++)
    {
        CHECK_EQ(bitset_find_first_clear(&s), i);       // the allocation pattern: take the lowest free one
        bitset_set(&s, (unsigned int)i);
    }
    CHECK_EQ(bitset_find_first_clear(&s), -1);          // full - the unused bits of the last word do not count
    CHECK_EQ((int)bitset_count(&s), 70);
    bitset_clear(&s, 40);
    CHECK_EQ(bitset_find_first_clear(&s), 40);

    TEST_SECTION("bitset: set all, and sizes at a word boundary");
    bitset_init(&s, words, 70);
    bitset_set_all(&s);
    CHECK_EQ((int)bitset_count(&s), 70);                // not 96: the bits past the end stay clear
    CHECK_EQ(bitset_find_first_clear(&s), -1);
    bitset_init(&s, words, 64);
    bitset_set_all(&s);
    CHECK_EQ((int)bitset_count(&s), 64);
    CHECK_EQ(bitset_find_first_clear(&s), -1);
    bitset_init(&s, words, 128);
    bitset_set_all(&s);
    CHECK_EQ((int)bitset_count(&s), 128);
    bitset_clear(&s, 127);
    CHECK_EQ(bitset_find_first_clear(&s), 127);
    bitset_init(&s, words, 0);                          // empty: nothing to find
    CHECK_EQ(bitset_find_first_clear(&s), -1);
    CHECK_EQ(bitset_find_first_set(&s), -1);
    CHECK_EQ((int)bitset_count(&s), 0);
}

// ---- string builder ----

static void builders(void)
{
    struct strbuf sb;

    TEST_SECTION("strbuf: appending");
    sb_init(&sb);
    CHECK_STR(sb_cstr(&sb), "");                        // an empty builder is still a string
    CHECK_EQ((int)sb_len(&sb), 0);
    CHECK_EQ(sb_append(&sb, "hello"), 0);
    CHECK_EQ(sb_append_char(&sb, ','), 0);
    CHECK_EQ(sb_append_char(&sb, ' '), 0);
    CHECK_EQ(sb_append(&sb, "world"), 0);
    CHECK_STR(sb_cstr(&sb), "hello, world");
    CHECK_EQ((int)sb_len(&sb), 12);
    CHECK_EQ(sb_append(&sb, ""), 0);
    CHECK_STR(sb_cstr(&sb), "hello, world");
    CHECK_EQ(sb_append_n(&sb, "!!!!!", 2), 0);          // the first two only
    CHECK_STR(sb_cstr(&sb), "hello, world!!");
    CHECK_EQ(sb_append_n(&sb, "ab", 10), 0);            // n larger than the text: stops at the NUL
    CHECK_STR(sb_cstr(&sb), "hello, world!!ab");
    CHECK_EQ((int)sb_len(&sb), 16);

    TEST_SECTION("strbuf: printf-style");
    sb_clear(&sb);
    CHECK_EQ((int)sb_len(&sb), 0);
    CHECK_STR(sb_cstr(&sb), "");
    CHECK_EQ(sb_appendf(&sb, "%d items", 3), 0);
    CHECK_EQ(sb_appendf(&sb, ", %s=%5.2f", "pi", 3.14159f), 0);
    CHECK_STR(sb_cstr(&sb), "3 items, pi= 3.14");
    CHECK_EQ(sb_appendf(&sb, ""), 0);                   // nothing
    CHECK_STR(sb_cstr(&sb), "3 items, pi= 3.14");
    char big[300];
    for (int i = 0; i < 299; i++) big[i] = (char)('a' + i % 26);
    big[299] = '\0';
    CHECK_EQ(sb_appendf(&sb, "[%s]", big), 0);          // far longer than the builder had room for
    CHECK_EQ((int)sb_len(&sb), 17 + 301);
    CHECK_EQ(sb_cstr(&sb)[17], '[');
    CHECK_EQ(sb_cstr(&sb)[17 + 300], ']');
    CHECK_EQ(memcmp(sb_cstr(&sb) + 18, big, 299), 0);

    TEST_SECTION("strbuf: growing a long way");
    sb_clear(&sb);
    for (int i = 0; i < 5000; i++)
        sb_append_char(&sb, (char)('0' + i % 10));
    CHECK_EQ((int)sb_len(&sb), 5000);
    CHECK_EQ((int)strlen(sb_cstr(&sb)), 5000);
    CHECK_EQ(sb_cstr(&sb)[4999], '9');

    TEST_SECTION("strbuf: taking the memory");
    sb_clear(&sb);
    sb_append(&sb, "keep me");
    char* mine = sb_take(&sb);
    CHECK_STR(mine, "keep me");
    CHECK_EQ((int)sb_len(&sb), 0);                      // the builder is empty and reusable
    CHECK_STR(sb_cstr(&sb), "");
    sb_append(&sb, "again");
    CHECK_STR(sb_cstr(&sb), "again");
    CHECK_STR(mine, "keep me");                         // and did not disturb what was taken
    free(mine);
    sb_free(&sb);
    char* nothing = sb_take(&sb);                       // taking from a builder that never held anything
    CHECK(nothing != NULL);
    CHECK_STR(nothing, "");
    free(nothing);
    sb_free(&sb);                                       // twice is fine
}

int main(void)
{
    unsigned int baseline = heap_used();
    lists();
    vectors();
    rings();
    maps();
    queues();
    bitsets();
    builders();

    TEST_SECTION("nothing leaked");
    CHECK_EQ((int)heap_used(), (int)baseline);          // every allocation above was given back
    CHECK_EQ(heap_check(), 0);                          // and the block list is still sound
    return test_summary();
}
