# `<ceres/arena.h>`

A linear allocator: allocation is a pointer bump, and everything is released at once (or back to a mark). Suited to memory with one lifetime - a frame, a level, a parse.

```c
struct arena
{
    char* base;
    unsigned int size;
    unsigned int used;
    int owned;                 // 1 when the memory came from malloc and arena_destroy must free it
};

void  arena_init(struct arena* a, void* buf, unsigned int size);    // over memory the caller owns
int   arena_init_heap(struct arena* a, unsigned int size);          // from malloc; 0 ok, -1 when there is no memory
void* arena_alloc(struct arena* a, unsigned int n);                 // 8-byte aligned; NULL when it does not fit
void* arena_alloc_zero(struct arena* a, unsigned int n);
char* arena_strdup(struct arena* a, const char* s);
unsigned int arena_mark(const struct arena* a);
void  arena_release(struct arena* a, unsigned int mark);            // back to a mark taken earlier
void  arena_reset(struct arena* a);                                 // release everything
unsigned int arena_left(const struct arena* a);                     // bytes still available
void  arena_destroy(struct arena* a);                               // frees the memory when it came from malloc
```
