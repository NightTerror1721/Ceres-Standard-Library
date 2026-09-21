// The end of a program: exit, _Exit, abort and atexit. Kept apart from the rest of stdlib.c because a program
// whose main returns ends here (ceresc turns `return n;` into `exit(n)` for a unit that declares exit), and
// a program that links the library by archive should carry these few functions and not qsort and strtod.
//
// The exit status is the low eight bits of the argument. `ceres run` exits with it; a machine that does not
// carry the status (an older CeresASM) still stops, with status 0.
#include "stdlib.h"
#include "stdio.h"
#include "ceres.h"

#define ATEXIT_SLOTS 32
static void (*atexit_table[ATEXIT_SLOTS])(void);
static int atexit_count = 0;

int atexit(void (*fn)(void))
{
    if (fn == 0 || atexit_count >= ATEXIT_SLOTS)
        return -1;
    atexit_table[atexit_count] = fn;
    atexit_count++;
    return 0;
}

void _Exit(int status)
{
    sys_exit_status(status);
}

void exit(int status)
{
    while (atexit_count > 0)
    {
        atexit_count--;                    // pop first: a handler that calls exit() must not run twice
        atexit_table[atexit_count]();
    }
    sys_exit_status(status);
}

void abort(void)
{
    putstr("abort\n");
    sys_exit_status(134);                  // 128 + SIGABRT, what a shell reports for an aborted program
}
