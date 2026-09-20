// The terminal's input ring holds 63 bytes. The host holds piped input back while the ring is full, so
// a program that is busy for a while before it reads still receives all of a long input. The 400
// bytes in test_stdin_long.stdin are far more than fit, and the spin below is what would have made
// the tail get dropped.
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
    printf("dropped by the ring: %d\n", term_dropped());
    return 0;
}
