// A SIGABRT handler runs when abort() is called; when it returns abort() goes on and stops the program with
// status 134 (tests/expected/test_signal_abort.status). raise(SIGABRT) with a handler only calls the handler.
#include "stdio.h"
#include "stdlib.h"
#include "signal.h"

static void on_abort(int sig)
{
    printf("handler: signal %d\n", sig);
}

int main(void)
{
    signal(SIGABRT, on_abort);
    raise(SIGABRT);                    // the handler, and then the program goes on
    puts("after raise");
    abort();                           // the handler again, and then the program stops
    puts("NOT REACHED");
    return 0;
}
