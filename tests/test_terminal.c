// Regression: every character must reach stdout as exactly ONE byte. term_write_char used to store
// a 32-bit word into the terminal's byte-wide output register, so putchar('A') put out 'A', 0, 0, 0.
// The expected file is compared byte for byte, so a stray NUL fails this test.
#include "stdio.h"
#include "ceres/sys.h"

int main(void)
{
    putchar('A');
    putchar('B');
    putchar('C');
    putstr("DE");
    puts("F");                 // "F" and a newline
    printf("%c%d\n", 'G', 7);
    puts("shutting down");
    sys_exit();                // nothing after this may run
    puts("NOT REACHED");
    return 0;
}
