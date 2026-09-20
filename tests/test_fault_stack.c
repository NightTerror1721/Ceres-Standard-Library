// USE: fault
// Runaway recursion runs the stack into its limit: StackOverflow. The handler runs on the separate system
// stack, so it can still report.
#include "stdio.h"
#include "ceres/sys.h"

static void hook(int irq, unsigned int pc, unsigned int flags)
{
    putstr("hook: interrupt ");
    putint(irq);
    putstr("\n");
}

static int depth;

static int recurse(int n)
{
    volatile char padding[256];
    padding[0] = (char)n;
    depth = n;
    return recurse(n + 1) + padding[0];              // not a tail call: every level keeps its frame
}

int main(void)
{
    putstr("installing\n");
    sys_install_fault_handlers(hook);
    putstr("recursing\n");
    int v = recurse(0);
    putstr("NOT REACHED\n");
    return v;
}
