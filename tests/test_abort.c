// abort() says so and stops the machine WITHOUT running the atexit handlers (exit() does).
#include "stdlib.h"
#include "stdio.h"

static void handler(void) { putstr("atexit ran: WRONG after abort()\n"); }

int main(void)
{
    atexit(handler);
    putstr("before\n");
    abort();
    putstr("NOT REACHED\n");
    return 0;
}
