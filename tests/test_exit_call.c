// exit() from deep in a call chain: the handlers run once, then the run ends with the low eight bits of the
// status (300 & 255 = 44). Nothing after the call may run.
#include "stdio.h"
#include "stdlib.h"

static void bye(void) { puts("handler ran"); }

static void deep(int n)
{
    if (n == 0)
    {
        puts("calling exit(300)");
        exit(300);
        puts("NOT REACHED");
    }
    deep(n - 1);
}

int main(void)
{
    atexit(bye);
    deep(3);
    puts("NOT REACHED");
    return 0;
}
