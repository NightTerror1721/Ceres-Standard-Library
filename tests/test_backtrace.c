// USE: fault
// Backtraces and symbolized faults (ceres/backtrace.h, CeresASM de96a5f): built with ceresc --symtab
// (tests/expected/test_backtrace.flags), so the program carries its function names. Addresses differ from one
// build to another; the names and the order of the calls do not.
#include "stdio.h"
#include "string.h"
#include "ceres/sys.h"
#include "ceres/backtrace.h"

static void print_callers(unsigned int* pcs, int n)
{
    for (int i = 0; i < n; i++)
    {
        unsigned int offset = 0;
        const char* name = backtrace_symbol(pcs[i] - 4u, &offset);
        putstr("  ");
        putstr(name != 0 ? name : "?");
        putstr("\n");
        if (name != 0 && strcmp(name, "main") == 0)
            break;                                       // what called main is the start-up code
    }
}

__attribute__((noinline)) void innermost(void)                   // not in its own backtrace: the callers are
{
    unsigned int pcs[8];
    int n = backtrace(pcs, 8);
    putstr("backtrace:\n");
    print_callers(pcs, n);
    putstr(pcs[0] - 4u >= (unsigned int)innermost ? "the printers:\n" : "?\n");
    backtrace_print_address((unsigned int)innermost);    // a function's own address: its name alone
    putstr("\n");
}

__attribute__((noinline)) void middle(void) { innermost(); putstr(""); }
__attribute__((noinline)) void outer(void) { middle(); putstr(""); }

static void hook(int irq, unsigned int pc, unsigned int flags)
{
    unsigned int offset = 0;
    const char* name = backtrace_symbol(pc, &offset);
    putstr("fault ");
    putint(irq);
    putstr(" in ");
    putstr(name != 0 ? name : "?");
    putstr(": ");
    unsigned int access = sys_fault_access();
    putstr((access & 0xFFu) == FAULT_WRITE ? "store " : "load ");
    putint((int)(access >> 8));
    putstr(" bytes at ");
    puthex(sys_fault_address());
    putstr("\ncalled from:\n");
    static unsigned int pcs[8];                          // a handler runs on the 4 KiB system stack: keep it small
    print_callers(pcs, backtrace_from(sys_fault_frame(), pcs, 8));
}

__attribute__((noinline)) void poke(volatile unsigned short* p) { *p = 1; putstr(""); }
__attribute__((noinline)) void reach(void) { poke((volatile unsigned short*)0x10001); putstr(""); }

int main(void)
{
    putstr(sys_fault_frame() == 0 && backtrace_has_symbols() ? "no fault yet, and names\n" : "?\n");
    outer();
    unsigned int offset = 1;
    putstr(backtrace_symbol(0, &offset) == 0 ? "no name below the code\n" : "a name below the code\n");
    sys_install_fault_handlers(hook);
    reach();
    putstr("NOT REACHED\n");
    return 0;
}
