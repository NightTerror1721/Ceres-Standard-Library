# `<ceres/atomic.h>`

Read-modify-write that an interrupt cannot cut in half. There is one core, so the only thing that can interleave with the code is an interrupt handler: masking user interrupts around the access is enough. (irq_save/irq_restore are inline, in irq.h; this needs nothing from the optional irq module.)

```c
typedef volatile int atomic_int;

static inline int  atomic_load(atomic_int* p)         { return *p; }
static inline void atomic_store(atomic_int* p, int v) { *p = v; }

static inline int atomic_add(atomic_int* p, int v)             // returns the value it had BEFORE
{
    unsigned int s = irq_save();
    int old = *p;
    *p = old + v;
    irq_restore(s);
    return old;
}

static inline int atomic_swap(atomic_int* p, int v)            // returns the value it had before
{
    unsigned int s = irq_save();
    int old = *p;
    *p = v;
    irq_restore(s);
    return old;
}

static inline int atomic_cas(atomic_int* p, int expect, int desired)   // 1 if it changed
{
    unsigned int s = irq_save();
    int ok = (*p == expect);
    if (ok)
        *p = desired;
    irq_restore(s);
    return ok;
}

// A critical section around a block, without the comma operator the C subset does not have:
//     CRITICAL_BEGIN(); ...; CRITICAL_END();      (once per block: it declares a variable)
#define CRITICAL_BEGIN()  unsigned int __crit_state = irq_save()
#define CRITICAL_END()    irq_restore(__crit_state)
```
