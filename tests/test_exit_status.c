// Returning n from main is exit(n): the run ends with status n (tests/expected/test_exit_status.status), and the
// atexit handlers run first, last registered first. That needs the compiler to end main with a call to exit,
// which it does for a unit that declares one - <stdio.h> and <stdlib.h> both do.
#include "stdio.h"
#include "stdlib.h"

static void first(void)  { puts("first registered, runs last"); }
static void second(void) { puts("second registered, runs first"); }

int main(void)
{
    atexit(first);
    atexit(second);
    puts("main returns 7");
    return 7;
}
