// A red-black tree (CLRS's algorithm), using real NULL for "no child"/"no parent" rather than a
// shared sentinel: it costs a handful of explicit NULL guards in the fixup code (annotated below,
// each one exactly where CLRS's own proof already says the corresponding sentinel access is safe -
// only here it is a real pointer that might truly be absent), and in exchange a node's left/right/
// parent fields mean exactly what they say to anyone walking the tree from outside this file, this
// library's own tests included.
#include "ceres/ds/rbtree.h"

static int is_red(const struct rb_node* n) { return n != NULL && n->red; }

void rb_init(struct rbtree* t, int (*cmp)(const struct rb_node*, const struct rb_node*))
{
    t->root = NULL;
    t->count = 0;
    t->cmp = cmp;
}

static void rotate_left(struct rbtree* t, struct rb_node* x)
{
    struct rb_node* y = x->right;                 // must have a right child: only ever called when it does
    x->right = y->left;
    if (y->left != NULL)
        y->left->parent = x;
    y->parent = x->parent;
    if (x->parent == NULL)
        t->root = y;
    else if (x == x->parent->left)
        x->parent->left = y;
    else
        x->parent->right = y;
    y->left = x;
    x->parent = y;
}

static void rotate_right(struct rbtree* t, struct rb_node* x)
{
    struct rb_node* y = x->left;
    x->left = y->right;
    if (y->right != NULL)
        y->right->parent = x;
    y->parent = x->parent;
    if (x->parent == NULL)
        t->root = y;
    else if (x == x->parent->right)
        x->parent->right = y;
    else
        x->parent->left = y;
    y->right = x;
    x->parent = y;
}

static void insert_fixup(struct rbtree* t, struct rb_node* z)
{
    while (z->parent != NULL && z->parent->red)      // root is always black, so z->parent red implies a grandparent exists below
    {
        if (z->parent == z->parent->parent->left)
        {
            struct rb_node* y = z->parent->parent->right;   // z's uncle - may be NULL
            if (is_red(y))
            {
                z->parent->red = 0;
                y->red = 0;
                z->parent->parent->red = 1;
                z = z->parent->parent;
            }
            else
            {
                if (z == z->parent->right)
                {
                    z = z->parent;
                    rotate_left(t, z);
                }
                z->parent->red = 0;
                z->parent->parent->red = 1;
                rotate_right(t, z->parent->parent);
            }
        }
        else                                          // mirror image of the block above
        {
            struct rb_node* y = z->parent->parent->left;
            if (is_red(y))
            {
                z->parent->red = 0;
                y->red = 0;
                z->parent->parent->red = 1;
                z = z->parent->parent;
            }
            else
            {
                if (z == z->parent->left)
                {
                    z = z->parent;
                    rotate_right(t, z);
                }
                z->parent->red = 0;
                z->parent->parent->red = 1;
                rotate_left(t, z->parent->parent);
            }
        }
    }
    t->root->red = 0;
}

void rb_insert(struct rbtree* t, struct rb_node* z)
{
    struct rb_node* y = NULL;
    struct rb_node* x = t->root;
    while (x != NULL)
    {
        y = x;
        x = t->cmp(z, x) < 0 ? x->left : x->right;
    }
    z->parent = y;
    if (y == NULL)
        t->root = z;
    else if (t->cmp(z, y) < 0)
        y->left = z;
    else
        y->right = z;
    z->left = NULL;
    z->right = NULL;
    z->red = 1;
    insert_fixup(t, z);
    t->count++;
}

// Replaces the subtree rooted at u with the subtree rooted at v, in u's parent's eyes. v may be NULL.
static void transplant(struct rbtree* t, struct rb_node* u, struct rb_node* v)
{
    if (u->parent == NULL)
        t->root = v;
    else if (u == u->parent->left)
        u->parent->left = v;
    else
        u->parent->right = v;
    if (v != NULL)
        v->parent = u->parent;
}

static struct rb_node* tree_minimum(struct rb_node* x)
{
    while (x->left != NULL)
        x = x->left;
    return x;
}

// x (possibly NULL - a "double black" position) sits under x_parent, passed explicitly because a
// NULL x carries no parent pointer of its own to read back.
static void delete_fixup(struct rbtree* t, struct rb_node* x, struct rb_node* x_parent)
{
    while (x != t->root && !is_red(x))
    {
        if (x == x_parent->left)
        {
            struct rb_node* w = x_parent->right;     // x's sibling - never NULL: removing a node could
                                                        // not have left the tree short of one here, by the
                                                        // invariant this loop maintains (CLRS's own proof)
            if (is_red(w))
            {
                w->red = 0;
                x_parent->red = 1;
                rotate_left(t, x_parent);
                w = x_parent->right;
            }
            if (!is_red(w->left) && !is_red(w->right))
            {
                w->red = 1;
                x = x_parent;
                x_parent = x->parent;
            }
            else
            {
                if (!is_red(w->right))
                {
                    if (w->left != NULL)
                        w->left->red = 0;
                    w->red = 1;
                    rotate_right(t, w);
                    w = x_parent->right;
                }
                w->red = x_parent->red;
                x_parent->red = 0;
                if (w->right != NULL)
                    w->right->red = 0;
                rotate_left(t, x_parent);
                x = t->root;
            }
        }
        else                                          // mirror image
        {
            struct rb_node* w = x_parent->left;
            if (is_red(w))
            {
                w->red = 0;
                x_parent->red = 1;
                rotate_right(t, x_parent);
                w = x_parent->left;
            }
            if (!is_red(w->right) && !is_red(w->left))
            {
                w->red = 1;
                x = x_parent;
                x_parent = x->parent;
            }
            else
            {
                if (!is_red(w->left))
                {
                    if (w->right != NULL)
                        w->right->red = 0;
                    w->red = 1;
                    rotate_left(t, w);
                    w = x_parent->left;
                }
                w->red = x_parent->red;
                x_parent->red = 0;
                if (w->left != NULL)
                    w->left->red = 0;
                rotate_right(t, x_parent);
                x = t->root;
            }
        }
    }
    if (x != NULL)
        x->red = 0;
}

void rb_remove(struct rbtree* t, struct rb_node* z)
{
    struct rb_node* y = z;
    int y_was_red = y->red;
    struct rb_node* x;
    struct rb_node* x_parent;

    if (z->left == NULL)
    {
        x = z->right;
        x_parent = z->parent;
        transplant(t, z, z->right);
    }
    else if (z->right == NULL)
    {
        x = z->left;
        x_parent = z->parent;
        transplant(t, z, z->left);
    }
    else
    {
        y = tree_minimum(z->right);       // z's in-order successor takes z's place
        y_was_red = y->red;
        x = y->right;
        if (y->parent == z)
        {
            x_parent = y;                  // x's logical parent, whether or not x is NULL
        }
        else
        {
            x_parent = y->parent;
            transplant(t, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }
        transplant(t, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->red = z->red;
    }

    if (!y_was_red)
        delete_fixup(t, x, x_parent);
    t->count--;
    // z's own parent/left/right/red are left as they were - nothing reads a removed node's rbtree
    // fields again, and what happens to the object itself now belongs to the caller.
}

struct rb_node* rb_find(const struct rbtree* t, const struct rb_node* key)
{
    struct rb_node* x = t->root;
    while (x != NULL)
    {
        int c = t->cmp(key, x);
        if (c == 0)
            return x;
        x = c < 0 ? x->left : x->right;
    }
    return NULL;
}

struct rb_node* rb_first(const struct rbtree* t)
{
    return t->root == NULL ? NULL : tree_minimum(t->root);
}

struct rb_node* rb_last(const struct rbtree* t)
{
    struct rb_node* x = t->root;
    if (x == NULL)
        return NULL;
    while (x->right != NULL)
        x = x->right;
    return x;
}

struct rb_node* rb_next(const struct rb_node* n)
{
    struct rb_node* x = (struct rb_node*)n;
    if (x->right != NULL)
        return tree_minimum(x->right);
    struct rb_node* y = x->parent;
    while (y != NULL && x == y->right)
    {
        x = y;
        y = y->parent;
    }
    return y;
}

struct rb_node* rb_prev(const struct rb_node* n)
{
    struct rb_node* x = (struct rb_node*)n;
    if (x->left != NULL)
    {
        x = x->left;
        while (x->right != NULL)
            x = x->right;
        return x;
    }
    struct rb_node* y = x->parent;
    while (y != NULL && x == y->left)
    {
        x = y;
        y = y->parent;
    }
    return y;
}
