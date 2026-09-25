// USE: mmu
// Guard pages under task stacks (mmu_guard_task_stacks): a store that runs off the bottom of a task's stack - an
// index gone negative, no push and no frame for the stack limit to see - hits the unmapped page under the stack
// instead of whatever the heap keeps below it. (A frame too large for the stack is the stack limit's: `enter`
// checks it.) The handler reports it and ends the program (tests/expected/test_mmu_guard.status).
#include "stdio.h"
#include "ceres/mmu.h"
#include "ceres/sys.h"
#include "ceres/task.h"
#include "stdlib.h"

static struct mmu_space space;

static int on_fault(unsigned int va, unsigned int pc, unsigned int access)
{
    putstr(access == FAULT_WRITE ? "a store hit the guard page\n" : "some other fault\n");
    sys_exit_status(3);
    return 0;
}

static void returns(void* arg)
{
    volatile char buffer[64];
    buffer[0] = 1;
    putstr(buffer[0] == 1 ? "a guarded task returns\n" : "NOT RIGHT\n");
}

static void underflow(void* arg)
{
    volatile char buffer[16];
    putstr("the task starts\n");
    volatile char* p = buffer;
    int index = -5000;                                // past the 4 KiB stack, into the page below it
    p[index] = 1;
    putstr("NOT REACHED\n");
}

int main(void)
{
    mmu_space_init(&space);
    mmu_identity(&space, MMU_WRITE | MMU_EXEC);
    mmu_on_fault(on_fault);
    mmu_guard_task_stacks(&space);
    mmu_activate(&space);
    task_join(task_spawn(returns, 0, 4096));          // its guard page is mapped again before the stack is freed
    volatile char* p = malloc(8192);                  // ...so the heap can hand that memory out and use it
    for (int i = 0; p != 0 && i < 8192; i++)
        p[i] = 0;
    free((void*)p);
    int id = task_spawn(underflow, 0, 4096);
    task_join(id);
    putstr("NOT REACHED either\n");
    return 0;
}
