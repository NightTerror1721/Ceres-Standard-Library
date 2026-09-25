// USE: fault
// A task that recurses without end runs into the bottom of its own stack: the stack limit follows the running
// task, so it is a StackOverflow, not a stack that runs over the heap below it.
#include "stdio.h"
#include "ceres/sys.h"
#include "ceres/task.h"

static void hook(int irq, unsigned int pc, unsigned int flags)
{
    putstr("hook: interrupt ");
    putint(irq);
    putstr("\n");
}

static int depth;

static int recurse(int n)
{
    volatile char padding[128];
    padding[0] = (char)n;
    depth = n;
    return recurse(n + 1) + padding[0];              // not a tail call: every level keeps its frame
}

static void runaway(void* arg)
{
    putstr("the task recurses\n");
    recurse(0);
    putstr("NOT REACHED\n");
}

int main(void)
{
    sys_install_fault_handlers(hook);
    int id = task_spawn(runaway, 0, 2048);
    task_join(id);
    putstr("NOT REACHED either\n");
    return 0;
}
