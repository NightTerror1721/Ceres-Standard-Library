# `<ceres/ds/slist.h>`

A singly linked list whose node lives INSIDE the object it links: no allocation, one pointer per node instead of list.h's two. Good for a stack or a free list, where nothing is ever removed from the middle - removing a node other than the head is O(n) here, because there is no `prev` to walk back from; that is exactly the operation to reach for list.h over this one for. Header only.

```c
struct job { int id; struct slist_node link; };
struct slist free_jobs;   slist_init(&free_jobs);
slist_push_front(&free_jobs, &job->link);
struct slist_node* n = slist_pop_front(&free_jobs);
struct job* j = slist_entry(n, struct job, link);
```

```c
struct slist_node { struct slist_node* next; };
struct slist      { struct slist_node* head; unsigned int count; };

static inline void slist_init(struct slist* l) { l->head = NULL; l->count = 0; }

// A node that has never been on a list, or was just popped off one.
static inline void slist_node_init(struct slist_node* n) { n->next = NULL; }

static inline int slist_empty(const struct slist* l) { return l->head == NULL; }
static inline unsigned int slist_count(const struct slist* l) { return l->count; }
static inline struct slist_node* slist_first(const struct slist* l) { return l->head; }

static inline void slist_push_front(struct slist* l, struct slist_node* n)
{
    n->next = l->head;
    l->head = n;
    l->count++;
}

// Unlinks and returns the head, or NULL when the list is empty.
static inline struct slist_node* slist_pop_front(struct slist* l)
{
    struct slist_node* n = l->head;
    if (n != NULL)
    {
        l->head = n->next;
        n->next = NULL;
        l->count--;
    }
    return n;
}

// Unlinks `at->next` (which must be on this list) - the only way to remove a node that is not the
// head, since there is no `prev` to find it from the node itself. O(1) once `at` is known, but
// finding `at` in the first place is an O(n) walk, same as the removed node's own cost would be.
static inline struct slist_node* slist_remove_after(struct slist* l, struct slist_node* at)
{
    struct slist_node* n = at->next;
    if (n != NULL)
    {
        at->next = n->next;
        n->next = NULL;
        l->count--;
    }
    return n;
}

// The object a node is embedded in.
#define slist_entry(node, type, member) ((type*)((char*)(node) - offsetof(type, member)))

// `it` is a struct slist_node*. Do not remove the current node inside the loop: use SLIST_FOR_EACH_SAFE.
#define SLIST_FOR_EACH(it, l) for ((it) = (l)->head; (it) != NULL; (it) = (it)->next)

// Like SLIST_FOR_EACH, with `nx` (another struct slist_node*) remembering the next node before the
// body runs, so the body may pop or slist_remove_after the current node.
static inline int slist_save_next(struct slist_node** nx, struct slist_node* it) { *nx = it->next; return 1; }
#define SLIST_FOR_EACH_SAFE(it, nx, l) \
    for ((it) = (l)->head; (it) != NULL && slist_save_next(&(nx), (it)); (it) = (nx))
```
