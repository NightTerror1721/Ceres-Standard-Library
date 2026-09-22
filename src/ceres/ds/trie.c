// A trie over pool.h nodes, children kept as a sorted slist.h. See ceres/ds/trie.h.
#include "ceres/ds/trie.h"

void trie_init(struct trie* t, void* node_storage, unsigned int max_nodes)
{
    pool_init(&t->nodes, node_storage, sizeof(struct trie_node), max_nodes);
    t->root.ch = 0;
    t->root.value = NULL;
    t->root.is_word = 0;
    slist_node_init(&t->root.siblings);
    slist_init(&t->root.children);
}

void trie_clear(struct trie* t)
{
    pool_clear(&t->nodes);
    slist_init(&t->root.children);
}

static struct trie_node* find_child(struct trie_node* parent, char ch)
{
    struct slist_node* it;
    SLIST_FOR_EACH(it, &parent->children)
    {
        struct trie_node* c = slist_entry(it, struct trie_node, siblings);
        if (c->ch == ch)
            return c;
        if (c->ch > ch)
            break;                      // children are kept sorted: nothing further can match
    }
    return NULL;
}

// Splices `child` into `parent->children` keeping the list sorted by ch. Done by hand rather than
// through slist.h's push_front/remove_after alone, the same way list.h's own list_insert_after
// works directly with a node's fields - an intrusive list's fields are public exactly so its owner
// can do this.
static void insert_child_sorted(struct trie_node* parent, struct trie_node* child)
{
    struct slist_node* prev = NULL;
    struct slist_node* it = parent->children.head;
    while (it != NULL && slist_entry(it, struct trie_node, siblings)->ch < child->ch)
    {
        prev = it;
        it = it->next;
    }
    child->siblings.next = it;
    if (prev == NULL)
        parent->children.head = &child->siblings;
    else
        prev->next = &child->siblings;
    parent->children.count++;
}

int trie_insert(struct trie* t, const char* key, void* value)
{
    struct trie_node* node = &t->root;
    for (const char* p = key; *p; p++)
    {
        struct trie_node* child = find_child(node, *p);
        if (child == NULL)
        {
            child = (struct trie_node*)pool_alloc(&t->nodes);
            if (child == NULL)
                return -1;
            child->ch = *p;
            child->value = NULL;
            child->is_word = 0;
            slist_node_init(&child->siblings);
            slist_init(&child->children);
            insert_child_sorted(node, child);
        }
        node = child;
    }
    node->is_word = 1;
    node->value = value;
    return 0;
}

void* trie_find(const struct trie* t, const char* key)
{
    struct trie_node* node = (struct trie_node*)&t->root;
    for (const char* p = key; *p; p++)
    {
        node = find_child(node, *p);
        if (node == NULL)
            return NULL;
    }
    return node->is_word ? node->value : NULL;
}

int trie_has_prefix(const struct trie* t, const char* prefix)
{
    struct trie_node* node = (struct trie_node*)&t->root;
    for (const char* p = prefix; *p; p++)
    {
        node = find_child(node, *p);
        if (node == NULL)
            return 0;
    }
    return 1;
}

#define TRIE_WALK_BUF 256

struct trie_walk_ctx
{
    void (*fn)(const char* word, void* value, void* ctx);
    void* user_ctx;
    char buf[TRIE_WALK_BUF];
};

static void walk(struct trie_node* node, unsigned int depth, struct trie_walk_ctx* wc)
{
    if (node->is_word)
    {
        wc->buf[depth] = '\0';
        wc->fn(wc->buf, node->value, wc->user_ctx);
    }
    struct slist_node* it;
    SLIST_FOR_EACH(it, &node->children)
    {
        struct trie_node* c = slist_entry(it, struct trie_node, siblings);
        if (depth + 1u < TRIE_WALK_BUF)
        {
            wc->buf[depth] = c->ch;
            walk(c, depth + 1u, wc);
        }
        // else: this branch's words are longer than TRIE_WALK_BUF-1 characters - skipped, not
        // overflowed; see the header comment
    }
}

void trie_each_prefix(const struct trie* t, const char* prefix, void (*fn)(const char*, void*, void*), void* ctx)
{
    struct trie_node* node = (struct trie_node*)&t->root;
    struct trie_walk_ctx wc;
    wc.fn = fn;
    wc.user_ctx = ctx;
    unsigned int len = 0;
    for (const char* p = prefix; *p; p++)
    {
        node = find_child(node, *p);
        if (node == NULL)
            return;                     // nothing has this prefix
        if (len + 1u < TRIE_WALK_BUF)
            wc.buf[len] = *p;
        len++;
    }
    walk(node, len, &wc);
}
