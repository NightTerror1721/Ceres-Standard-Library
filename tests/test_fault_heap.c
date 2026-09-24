// USE: fault
// The stack stops at the top of the heap: malloc raises the machine's stack limit as the heap grows, so a
// runaway recursion is a StackOverflow before it reaches an allocation, and the allocation is intact when the
// fault is reported.
#include "stdio.h"
#include "stdlib.h"
#include "ceres/sys.h"
#include "ceres/config.h"

#define BLOCK 4096u
#define HEADERS 64u   // room for the two blocks' 8-byte headers and their 8-byte rounding, with some to spare

static unsigned char* block;

static void hook(int irq, unsigned int pc, unsigned int flags)
{
    putstr("hook: interrupt ");
    putint(irq);
    putstr("\n");
    unsigned int damaged = 0;
    for (unsigned int i = 0; i < BLOCK; i++)
        damaged += block[i] != (unsigned char)(i * 7u);
    putstr(damaged == 0 ? "the heap is intact\n" : "the heap was overwritten\n");
}

static int recurse(int n)
{
    volatile char padding[256];
    padding[0] = (char)n;
    return recurse(n + 1) + padding[0];              // not a tail call: every level keeps its frame
}

int main(void)
{
    sys_install_fault_handlers(hook);
    unsigned int before = sys_stack_limit();

    // The heap takes most of the ground it may - all but the reserve malloc keeps for the stack - and the
    // block watched is its top, the first thing a stack coming down would rewrite.
    unsigned int room = sys_stack_free();
    if (room > CERES_HEAP_STACK_RESERVE + 2u * BLOCK + HEADERS)
        malloc(room - CERES_HEAP_STACK_RESERVE - 2u * BLOCK - HEADERS);
    block = malloc(BLOCK);
    if (block == 0)
    {
        putstr("no room for the watched block\n");
        return 1;
    }
    for (unsigned int i = 0; i < BLOCK; i++)
        block[i] = (unsigned char)(i * 7u);

    unsigned int after = sys_stack_limit();
    putstr(after > before && after >= (unsigned int)block + BLOCK ? "the limit rose over the heap\n" : "the limit did not move\n");
    sys_set_stack_limit(0);                          // below the image: held at the image's end, not lowered
    putstr(sys_stack_limit() == before ? "it cannot go below the image\n" : "it went below the image\n");
    sys_set_stack_limit(after);
    putstr("recursing\n");
    int v = recurse(0);
    putstr("NOT REACHED\n");
    return v;
}
