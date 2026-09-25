# `<ceres/ds/list.h>`

A doubly linked list whose node lives INSIDE the object it links: no allocation, and an object can sit on several lists at once by carrying several nodes. The list is circular around a sentinel, so no operation has an "empty" special case. Header only.

```c
struct job { int id; struct list_node link; };
struct list queue;   list_init(&queue);
list_push_back(&queue, &job->link);
struct list_node* it;
LIST_FOR_EACH(it, &queue) { struct job* j = list_entry(it, struct job, link); ... }
```

```c
struct list_node { struct list_node* prev; struct list_node* next; };
struct list      { struct list_node head; unsigned int count; };

static inline void list_init(struct list* l)
{
    l->head.prev = &l->head;
    l->head.next = &l->head;
    l->count = 0;
}

// A node that has never been on a list: points at itself, so list_remove on it does nothing.
static inline void list_node_init(struct list_node* n) { n->prev = n; n->next = n; }

static inline int list_empty(const struct list* l) { return l->head.next == &l->head; }
static inline unsigned int list_count(const struct list* l) { return l->count; }

static inline void list_insert_after(struct list* l, struct list_node* at, struct list_node* n)
{
    n->prev = at;
    n->next = at->next;
    at->next->prev = n;
    at->next = n;
    l->count++;
}

static inline void list_push_front(struct list* l, struct list_node* n) { list_insert_after(l, &l->head, n); }
static inline void list_push_back(struct list* l, struct list_node* n)  { list_insert_after(l, l->head.prev, n); }

// Unlinks a node that is on this list. The node points at itself afterwards, so removing it twice by
// mistake does no harm to the list.
static inline void list_remove(struct list* l, struct list_node* n)
{
    if (n->next == n)                       // not linked
        return;
    n->prev->next = n->next;
    n->next->prev = n->prev;
    n->prev = n;
    n->next = n;
    l->count--;
}

// The first and last node, or NULL when the list is empty.
static inline struct list_node* list_first(const struct list* l) { return list_empty(l) ? NULL : l->head.next; }
static inline struct list_node* list_last(const struct list* l)  { return list_empty(l) ? NULL : l->head.prev; }

// Unlinks and returns the first (or last) node, or NULL when there is none.
static inline struct list_node* list_pop_front(struct list* l)
{
    struct list_node* n = list_first(l);
    if (n != NULL)
        list_remove(l, n);
    return n;
}

static inline struct list_node* list_pop_back(struct list* l)
{
    struct list_node* n = list_last(l);
    if (n != NULL)
        list_remove(l, n);
    return n;
}

// The object a node is embedded in.
#define list_entry(node, type, member) ((type*)((char*)(node) - offsetof(type, member)))

// `it` is a struct list_node*. Do not remove the current node inside the loop: use LIST_FOR_EACH_SAFE.
#define LIST_FOR_EACH(it, l) for ((it) = (l)->head.next; (it) != &(l)->head; (it) = (it)->next)

// Like LIST_FOR_EACH, with `nx` (another struct list_node*) remembering the next node before the body
// runs, so the body may remove `it`. (The C subset has no comma operator, hence the helper.)
static inline int list_save_next(struct list_node** nx, struct list_node* it) { *nx = it->next; return 1; }
#define LIST_FOR_EACH_SAFE(it, nx, l) \
    for ((it) = (l)->head.next; (it) != &(l)->head && list_save_next(&(nx), (it)); (it) = (nx))
```
