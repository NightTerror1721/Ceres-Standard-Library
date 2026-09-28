// A long input, typed with `ceres run --type`, reaches a program that is busy for a while before it reads: the
// 400 bytes of test_stdin_long.stdin wait on the terminal and all arrive, none dropped. It has no line end, so it
// comes as the input ends.
#include "stdio.h"
#include "ceres/terminal.h"

int main(void)
{
    volatile int spin;
    for (spin = 0; spin < 200000; spin++) {}

    unsigned int sum = 0;
    int first = 0, last = 0, n;
    for (n = 0; n < 400; n++)
    {
        int c = getchar();
        if (n == 0) first = c;
        last = c;
        sum += (unsigned int)c * (unsigned int)(n + 1);
    }
    printf("read %d bytes: first '%c', last '%c', weighted sum %u\n", n, first, last, sum);
    return 0;
}
