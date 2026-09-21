// _Exit stops at once with its status, and does not run the atexit handlers.
#include "stdio.h"
#include "stdlib.h"

static void bye(void) { puts("NOT REACHED: handler ran"); }

int main(void)
{
    atexit(bye);
    puts("calling _Exit(9)");
    _Exit(9);
    puts("NOT REACHED");
    return 0;
}
