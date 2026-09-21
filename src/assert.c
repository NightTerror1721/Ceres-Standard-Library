#include "assert.h"
#include "stdio.h"
#include "ceres.h"

// Called by assert() when its expression is 0. It reports with putstr/putint rather than printf:
// a failing assert may be a symptom of a broken heap or format engine, and the report must not
// depend on either.
void __assert_fail(const char* expr, const char* file, int line)
{
    putstr(file);
    putchar(':');
    putint(line);
    putstr(": assertion '");
    putstr(expr);
    putstr("' failed\n");
    sys_exit_status(134);                                // abnormal termination, like abort()
}
