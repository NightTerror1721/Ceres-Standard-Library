#include "assert.h"
#include "stdio.h"
#include "ceres.h"
#include "ceres/terminal.h"
#include "string.h"

void __abort_now(void) __attribute__((__noreturn__));        // exit.c: the SIGABRT handler, if any, then status 134

static void err(const char* s)
{
    term_write_error(s, (int)strlen(s));
}

// Called by assert() when its expression is 0. It reports on the error stream, as C asks, with nothing
// but block writes rather than printf: a failing assert may be a symptom of a broken heap or format engine,
// and the report must not depend on either.
void __assert_fail(const char* expr, const char* file, int line, const char* func)
{
    char digits[12];
    int n = 0;
    unsigned int v = line < 0 ? 0u : (unsigned int)line;
    do
    {
        digits[10 - n] = (char)('0' + v % 10u);
        n++;
        v /= 10u;
    } while (v != 0 && n < 10);
    digits[11] = 0;
    err(file);
    err(":");
    err(digits + 11 - n);
    err(": ");
    err(func);
    err(": assertion '");
    err(expr);
    err("' failed\n");
    __abort_now();                                       // abnormal termination, like abort()
}
