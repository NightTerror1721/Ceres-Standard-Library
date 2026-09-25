# `<ceres/ds/dsu.h>`

A disjoint-set (union-find) over the integers 0 .. n-1, on memory the caller provides - the textbook structure, no nodes and no allocation of its own, in the same spirit as bitset.h. Answers "are these two already in the same group?" in effectively constant time: flood-filling a connected region of tiles, Kruskal's minimum spanning tree, grouping entities that have touched.

```c
unsigned int parent[N];  unsigned char rank[N];
struct dsu d;  dsu_init(&d, parent, rank, N);
dsu_union(&d, 2, 5);
if (dsu_connected(&d, 2, 5)) { ... }
```

```c
struct dsu
{
    unsigned int* parent;       // needs n * sizeof(unsigned int) bytes
    unsigned char* rank;        // needs n bytes
    unsigned int n;
};

static inline void dsu_init(struct dsu* d, unsigned int* parent_storage, unsigned char* rank_storage, unsigned int n)
{
    d->parent = parent_storage;
    d->rank = rank_storage;
    d->n = n;
    for (unsigned int i = 0; i < n; i++)
    {
        d->parent[i] = i;
        d->rank[i] = 0;
    }
}

// The root of x's group, with path compression: every node visited on the way up is re-pointed
// straight at the root, so the next find() through any of them is O(1).
static inline unsigned int dsu_find(struct dsu* d, unsigned int x)
{
    while (d->parent[x] != x)
    {
        d->parent[x] = d->parent[d->parent[x]];   // path halving: one hop closer, done in passing
        x = d->parent[x];
    }
    return x;
}

static inline int dsu_connected(struct dsu* d, unsigned int a, unsigned int b) { return dsu_find(d, a) == dsu_find(d, b); }

// Merges a's and b's groups. Union by rank: the shallower tree hangs off the deeper one, which is
// what keeps find() short without needing full path compression to do all the work by itself.
static inline void dsu_union(struct dsu* d, unsigned int a, unsigned int b)
{
    unsigned int ra = dsu_find(d, a);
    unsigned int rb = dsu_find(d, b);
    if (ra == rb)
        return;
    if (d->rank[ra] < d->rank[rb])
    {
        unsigned int t = ra; ra = rb; rb = t;
    }
    d->parent[rb] = ra;
    if (d->rank[ra] == d->rank[rb])
        d->rank[ra]++;
}
```
