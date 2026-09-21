// With no handler, raise() ends the program with 128 plus the signal number: SIGTERM is 15, so 143
// (tests/expected/test_signal_default.status). Nothing after it runs, and the atexit handlers do not either.
#include "stdio.h"
#include "stdlib.h"
#include "signal.h"

static void bye(void) { puts("NOT REACHED: atexit ran"); }

int main(void)
{
    atexit(bye);
    puts("raising SIGTERM");
    raise(SIGTERM);
    puts("NOT REACHED");
    return 0;
}
