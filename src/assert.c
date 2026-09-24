#include "assert.h"
#include "stdio.h"
#include "ceres.h"

void __abort_now(void) __attribute__((noreturn));        // exit.c: the SIGABRT handler, if any, then status 134

// Called by assert() when its expression is 0. It reports with putstr/putint rather than printf:
// a failing assert may be a symptom of a broken heap or format engine, and the report must not
// depend on either.
void __assert_fail(const char* expr, const char* file, int line, const char* func)
{
    putstr(file);
    putchar(':');
    putint(line);
    putstr(": ");
    putstr(func);
    putstr(": assertion '");
    putstr(expr);
    putstr("' failed\n");
    __abort_now();                                       // abnormal termination, like abort()
}
