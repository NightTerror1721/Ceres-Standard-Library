// quick_exit runs the at_quick_exit handlers, last registered first, and does NOT run the atexit ones.
#include "stdio.h"
#include "stdlib.h"

static void ordinary(void) { puts("NOT REACHED: atexit handler ran"); }
static void first(void) { puts("quick first"); }
static void second(void) { puts("quick second"); }

int main(void)
{
    atexit(ordinary);
    at_quick_exit(first);
    at_quick_exit(second);
    puts("calling quick_exit(7)");
    quick_exit(7);
    puts("NOT REACHED");
    return 0;
}
