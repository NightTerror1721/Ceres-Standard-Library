#pragma once

#include "../stddef.h"
#include "config.h"

// Tasks: coroutines with a scheduler, cooperative. A task is a function running on a stack of its own (from
// malloc); it runs until it yields, sleeps, waits for another task or a channel, or returns - and then the next
// task that can run does, in turn. Nothing preempts a task: between two of those calls it has the machine to
// itself, so it needs no locks. The program's main() is a task too, the first one (id 0).
//
//   static void blink(void* arg) { for (;;) { led_toggle(); task_sleep_ms(500); } }
//   task_spawn(blink, 0, 0);
//   for (;;) { game_frame(); task_yield(); }
//
// When every task is asleep the machine halts until the first one is due (the alarm, ceres/timer.h): it does not
// spin. Each task's stack is guarded: while it runs, the machine's stack limit (CeresASM fcf7d4c) is the bottom of
// its stack, so running out is a StackOverflow fault instead of rewritten memory. A task that waits for something
// nothing can ever give (every other task waiting too) gets -1 and EDEADLK instead of waiting for ever.
//
// Limits: TASK_MAX tasks at once, main included (ceres/config.h); a stack of TASK_STACK_DEFAULT bytes when 0 is
// asked for. An interrupt handler must not call any of these.

typedef void (*task_fn)(void* arg);

int  task_spawn(task_fn fn, void* arg, unsigned int stack_size);   // its id (> 0), or -1 (EAGAIN: no slot, ENOMEM)
void task_yield(void);                     // let every other task that can run have a turn
void task_sleep_ms(unsigned int ms);       // yield for at least ms milliseconds of real time
void task_sleep_ns(unsigned long long ns);
int  task_join(int id);                    // wait until task `id` has returned: 0, or -1 (ESRCH, EDEADLK)
void task_exit(void) __attribute__((__noreturn__));   // end this task (not main) as if its function returned
int  task_self(void);                      // this task's id; 0 for main
int  task_count(void);                     // tasks alive, main included
int  task_alive(int id);

// Channels: a queue of fixed-size messages between tasks. send waits while it is full and recv while it is empty,
// letting the others run; a closed channel refuses new messages, and recv drains what is left and then fails.
struct chan
{
    unsigned char* buf;
    unsigned int elem_size, capacity, count, head;
    int closed;
};

int  chan_init(struct chan* c, unsigned int elem_size, unsigned int capacity);   // 0, or -1 (ENOMEM, EINVAL)
void chan_free(struct chan* c);
int  chan_send(struct chan* c, const void* msg);       // 0; -1 when closed (EPIPE) or nothing can ever take it (EDEADLK)
int  chan_recv(struct chan* c, void* msg);             // 0; -1 when closed and empty (EPIPE), or EDEADLK
int  chan_try_send(struct chan* c, const void* msg);   // 0, or -1 (EAGAIN when full, EPIPE when closed)
int  chan_try_recv(struct chan* c, void* msg);         // 0, or -1 (EAGAIN when empty, EPIPE when closed and empty)
void chan_close(struct chan* c);
unsigned int chan_len(const struct chan* c);
