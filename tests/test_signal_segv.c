// USE: fault
// SIGSEGV: a store into the vector table is a memory fault, and a handler that leaves through exit() ends the
// program with its own status (tests/expected/test_signal_segv.status).
#include "stdio.h"
#include "stdlib.h"
#include "ceres/sys.h"
#include "signal.h"

static void on_segv(int sig)
{
    putstr("caught signal ");                            // no printf: this runs on the 4 KiB system stack
    putint(sig);
    putstr("\n");
    exit(7);
}

int main(void)
{
    sys_install_fault_handlers(0);
    signal(SIGSEGV, on_segv);
    puts("storing into the vector table");
    *(volatile unsigned int*)0x10 = 1;                   // a MemoryFault
    puts("NOT REACHED");
    return 0;
}
