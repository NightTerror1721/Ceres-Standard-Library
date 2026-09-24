// The end of a program: exit, _Exit, abort and atexit. Kept apart from the rest of stdlib.c because a program
// whose main returns ends here (ceresc turns `return n;` into `exit(n)` for a unit that declares exit), and
// a program that links the library by archive should carry these few functions and not qsort and strtod.
//
// The exit status is the low eight bits of the argument. `ceres run` exits with it; a machine that does not
// carry the status (an older CeresASM) still stops, with status 0.
#include "stdlib.h"
#include "stdio.h"
#include "ceres.h"
#include "ceres/config.h"

#define ATEXIT_SLOTS CERES_ATEXIT_SLOTS
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

// What abort() runs before it stops the program: signal(SIGABRT, handler) points it at the handler. It is a
// pointer, not a call, so a program that never uses signals does not link src/signal.c.
void (*__abort_hook)(void) = 0;
void __abort_now(void) __attribute__((__noreturn__));

// Runs the SIGABRT handler, if there is one, and stops the program with status 134 (128 + SIGABRT, what a shell
// reports for an aborted program). assert() ends here too, without abort()'s message.
void __abort_now(void)
{
    if (__abort_hook != 0)
        __abort_hook();
    sys_exit_status(134);
}

void abort(void)
{
    putstr("abort\n");
    __abort_now();
}
