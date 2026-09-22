#pragma once

#include "vector.h"
#include "../../string.h"

// A LIFO stack of fixed-size elements - built directly on ceres/ds/vector.h (push/pop/last are
// already exactly a stack's operations), named separately so that a reader does not have to check
// that nothing ever calls vector_insert on it to know the order is only ever push/pop.
//
//   struct stack s;  stack_init(&s, sizeof(int));
//   int v = 3;  stack_push(&s, &v);
//   int out;  stack_pop(&s, &out);
//   stack_free(&s);

struct stack { struct vector items; };

static inline void stack_init(struct stack* s, unsigned int elem_size) { vector_init(&s->items, elem_size); }
static inline void stack_free(struct stack* s) { vector_free(&s->items); }
static inline void stack_clear(struct stack* s) { vector_clear(&s->items); }
static inline unsigned int stack_len(const struct stack* s) { return s->items.len; }
static inline int stack_empty(const struct stack* s) { return s->items.len == 0; }

static inline int stack_push(struct stack* s, const void* item) { return vector_push_copy(&s->items, item); }   // 0 ok, -1 out of memory
static inline const void* stack_top(const struct stack* s) { return vector_last(&s->items); }                   // NULL when empty

// Copies the top element to *out (which may be NULL) and removes it. 0 ok, -1 when empty.
static inline int stack_pop(struct stack* s, void* out)
{
    const void* top = vector_last(&s->items);
    if (top == NULL)
        return -1;
    if (out != NULL)
        memcpy(out, top, s->items.elem);
    vector_pop(&s->items);
    return 0;
}
