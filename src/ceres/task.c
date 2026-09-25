// Tasks and channels. See ceres/task.h.
//
// A task is a slot: a state, a saved context (asm/task.casm) and a stack. main is slot 0, with the stack the
// machine gave it. The scheduler is round robin over the slots after the current one; a task that is waiting is
// woken by whatever it waits for (a task finishing, a channel changing) and then looks again.
//
// The stack limit follows the running task: main's is the heap's top (which malloc reports through
// __heap_top_hook while tasks exist), a task's is the bottom of its own stack. Only a push is checked against it,
// so across a switch it is the lower of the two, and each side puts its own back once it runs.
#include "ceres/task.h"
#include "ceres/timer.h"
#include "ceres/sys.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"

struct ctx { unsigned int w[14]; };

void __task_switch(struct ctx* save, struct ctx* load);
unsigned int __task_entry_address(void);
void __task_finish(void) __attribute__((__noreturn__));

extern unsigned int __heap_main_sp;                  // malloc.c
extern void (*__heap_top_hook)(unsigned int top);

#define T_FREE      0
#define T_READY     1
#define T_SLEEPING  2
#define T_WAITING   3
#define T_DONE      4

struct task
{
    int state;
    int generation;                  // the id is generation * TASK_MAX + slot
    struct ctx ctx;
    unsigned char* stack;            // 0 for main
    unsigned int stack_size;
    unsigned long long wake_ns;
    const void* waiting_on;          // a struct task or a struct chan
    int deadlocked;                  // woken because nothing else ever could
    task_fn fn;
    void* arg;
};

static struct task tasks[TASK_MAX];
static int current = 0;
static int started = 0;
static int alive = 1;                // main
static unsigned int main_limit;      // main's stack limit: the heap's top
static unsigned char* to_free = 0;   // a finished task's stack, freed once we are off it

static unsigned int limit_of(int i)
{
    return i == 0 ? main_limit : (unsigned int)tasks[i].stack;
}

static void set_limit(unsigned int limit)
{
    if (limit != 0 && limit != 0xFFFFFFFFu)
        sys_set_stack_limit(limit);
}

static void heap_moved(unsigned int top)
{
    main_limit = top;
    if (current == 0)
        set_limit(top);
}

static void start(void)
{
    if (started)
        return;
    started = 1;
    tasks[0].state = T_READY;
    main_limit = sys_stack_limit();
    __heap_top_hook = heap_moved;
}

static int id_of(int slot)
{
    return tasks[slot].generation * TASK_MAX + slot;
}

// The slot of a live id, or -1.
static int slot_of(int id)
{
    if (id < 0)
        return -1;
    int slot = id % TASK_MAX;
    if (tasks[slot].generation != id / TASK_MAX || tasks[slot].state == T_FREE)
        return -1;
    return slot;
}

static void release_finished(void)
{
    if (to_free != 0)
    {
        free(to_free);
        to_free = 0;
    }
}

static void switch_to(int next)
{
    int from = current;
    unsigned int a = sys_stack_limit(), b = limit_of(next);   // what holds now (a finished task has no stack left)
    set_limit(a < b ? a : b);                        // both stacks are in bounds while the switch runs
    if (from == 0)
        __heap_main_sp = sys_sp();                   // malloc keeps the heap below main's stack, not this one
    current = next;
    if (next == 0)
        __heap_main_sp = 0;
    __task_switch(&tasks[from].ctx, &tasks[next].ctx);
    // Resumed: `current` is this task again.
    set_limit(limit_of(current));
    release_finished();
}

// Moves on to the next task that can run - the current one again if it is ready and no other is - waking the
// sleepers that are due, and halting the machine until the first one is when nothing else can run. Returns 1
// once the current task runs again, 0 when nothing can ever run (the caller's wait would be for ever).
static int schedule(void)
{
    for (;;)
    {
        unsigned long long now = timer_nanos64();
        unsigned long long earliest = ~0ull;
        int next = -1;
        for (int k = 1; k <= TASK_MAX; k++)
        {
            int i = (current + k) % TASK_MAX;
            struct task* t = &tasks[i];
            if (t->state == T_SLEEPING)
            {
                if (t->wake_ns <= now)
                    t->state = T_READY;
                else if (t->wake_ns < earliest)
                    earliest = t->wake_ns;
            }
            if (t->state == T_READY && next < 0)
                next = i;
        }
        if (next >= 0)
        {
            if (next != current)
                switch_to(next);
            return 1;
        }
        if (earliest != ~0ull)
        {
            timer_halt_until_ns(earliest);           // everyone sleeps: so does the machine
            continue;
        }
        if (tasks[current].state != T_DONE)
            return 0;                                // the caller waits for something no one will do
        // A finished task has to leave, and everyone else waits: one of them learns it never will be served.
        for (int k = 1; k <= TASK_MAX; k++)
        {
            int i = (current + k) % TASK_MAX;
            if (tasks[i].state == T_WAITING)
            {
                tasks[i].deadlocked = 1;
                tasks[i].state = T_READY;
                break;
            }
        }
    }
}

// Wakes every task waiting on `object`.
static void wake(const void* object)
{
    for (int i = 0; i < TASK_MAX; i++)
        if (tasks[i].state == T_WAITING && tasks[i].waiting_on == object)
            tasks[i].state = T_READY;
}

// Waits until something wakes the current task on `object`: 0, or -1 with EDEADLK when nothing ever will.
static int wait_on(const void* object)
{
    struct task* t = &tasks[current];
    t->state = T_WAITING;
    t->waiting_on = object;
    t->deadlocked = 0;
    int ran = schedule();
    t->waiting_on = 0;
    if (!ran || t->deadlocked)
    {
        t->state = T_READY;
        t->deadlocked = 0;
        errno = EDEADLK;
        return -1;
    }
    return 0;
}

// Where a new task's first switch lands (through asm/task.casm's __task_entry): its own stack limit, then fn.
static void task_start(void* slot_as_pointer)
{
    int slot = (int)(unsigned int)slot_as_pointer;
    set_limit(limit_of(slot));
    release_finished();
    tasks[slot].fn(tasks[slot].arg);
}

void __task_finish(void)
{
    struct task* t = &tasks[current];
    t->state = T_DONE;
    alive--;
    to_free = t->stack;                               // freed by whoever runs next, off this stack
    t->stack = 0;
    wake(t);
    schedule();                                       // never comes back: nothing makes a DONE task ready
    for (;;)
        __builtin_halt();
}

int task_spawn(task_fn fn, void* arg, unsigned int stack_size)
{
    start();
    if (fn == 0)
    {
        errno = EINVAL;
        return -1;
    }
    int slot = -1;
    for (int i = 1; i < TASK_MAX && slot < 0; i++)
        if (tasks[i].state == T_FREE || tasks[i].state == T_DONE)
            slot = i;                                 // a finished one's id stops being valid now
    if (slot < 0)
    {
        errno = EAGAIN;
        return -1;
    }
    if (stack_size == 0)
        stack_size = TASK_STACK_DEFAULT;
    stack_size = (stack_size + 7u) & ~7u;
    if (stack_size < 512u)
        stack_size = 512u;
    unsigned char* stack = (unsigned char*)malloc(stack_size);
    if (stack == 0)
    {
        errno = ENOMEM;
        return -1;
    }
    struct task* t = &tasks[slot];
    int generation = t->generation + 1;
    memset(t, 0, sizeof *t);
    t->generation = generation;
    t->stack = stack;
    t->stack_size = stack_size;
    t->fn = fn;
    t->arg = arg;
    // The first switch "returns" to __task_entry, which calls r8(r9) = task_start(slot), then r10.
    unsigned int* top = (unsigned int*)(stack + stack_size);
    top[-2] = __task_entry_address();                // the return address the switch pops; 8-aligned below it
    t->ctx.w[0] = (unsigned int)task_start;
    t->ctx.w[1] = (unsigned int)slot;
    t->ctx.w[2] = (unsigned int)__task_finish;
    t->ctx.w[4] = 0;                                 // fp: no frame above
    t->ctx.w[5] = (unsigned int)&top[-2];
    t->state = T_READY;
    alive++;
    return id_of(slot);
}

void task_yield(void)
{
    start();
    schedule();                                       // the current task stays ready: it runs again in turn
}

void task_sleep_ns(unsigned long long ns)
{
    start();
    struct task* t = &tasks[current];
    t->wake_ns = timer_nanos64() + ns;
    t->state = T_SLEEPING;
    schedule();
}

void task_sleep_ms(unsigned int ms)
{
    task_sleep_ns((unsigned long long)ms * 1000000ull);
}

int task_join(int id)
{
    start();
    int slot = slot_of(id);
    if (slot < 0 || slot == current)
    {
        errno = slot == current && slot >= 0 ? EDEADLK : ESRCH;
        return -1;
    }
    while (tasks[slot].state != T_DONE && tasks[slot].generation * TASK_MAX + slot == id)
        if (wait_on(&tasks[slot]) != 0)
            return -1;
    return 0;
}

void task_exit(void)
{
    if (current == 0)
        exit(0);                                      // main ending is the program ending
    __task_finish();
}

int task_self(void)
{
    return started ? id_of(current) : 0;
}

int task_count(void)
{
    return alive;
}

int task_alive(int id)
{
    if (id == 0)
        return 1;
    int slot = slot_of(id);
    return slot >= 0 && tasks[slot].state != T_DONE;
}

// ---- channels ----

int chan_init(struct chan* c, unsigned int elem_size, unsigned int capacity)
{
    if (c == 0 || elem_size == 0 || capacity == 0 || capacity > 0x7FFFFFFFu / elem_size)
    {
        errno = EINVAL;
        return -1;
    }
    memset(c, 0, sizeof *c);
    c->buf = (unsigned char*)malloc(elem_size * capacity);
    if (c->buf == 0)
    {
        errno = ENOMEM;
        return -1;
    }
    c->elem_size = elem_size;
    c->capacity = capacity;
    return 0;
}

void chan_free(struct chan* c)
{
    if (c == 0)
        return;
    free(c->buf);
    c->buf = 0;
    c->closed = 1;
    wake(c);
}

int chan_try_send(struct chan* c, const void* msg)
{
    if (c->closed)
    {
        errno = EPIPE;
        return -1;
    }
    if (c->count == c->capacity)
    {
        errno = EAGAIN;
        return -1;
    }
    unsigned int at = (c->head + c->count) % c->capacity;
    memcpy(c->buf + at * c->elem_size, msg, c->elem_size);
    c->count++;
    wake(c);                                          // a receiver may be waiting
    return 0;
}

int chan_try_recv(struct chan* c, void* msg)
{
    if (c->count == 0)
    {
        errno = c->closed ? EPIPE : EAGAIN;
        return -1;
    }
    memcpy(msg, c->buf + c->head * c->elem_size, c->elem_size);
    c->head = (c->head + 1) % c->capacity;
    c->count--;
    wake(c);                                          // a sender may be waiting
    return 0;
}

int chan_send(struct chan* c, const void* msg)
{
    start();
    for (;;)
    {
        if (chan_try_send(c, msg) == 0)
            return 0;
        if (errno != EAGAIN || wait_on(c) != 0)
            return -1;
    }
}

int chan_recv(struct chan* c, void* msg)
{
    start();
    for (;;)
    {
        if (chan_try_recv(c, msg) == 0)
            return 0;
        if (errno != EAGAIN || wait_on(c) != 0)
            return -1;
    }
}

void chan_close(struct chan* c)
{
    c->closed = 1;
    wake(c);                                          // everyone waiting learns it
}

unsigned int chan_len(const struct chan* c)
{
    return c->count;
}
