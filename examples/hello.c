// The smallest useful program: a string, a number, a formatted line.
// Build and run from the repository root:
//   ceresc examples/hello.c src/*.c src/ceres/*.c asm/*.casm -I include --run
#include "stdio.h"
#include "stdlib.h"

int main(void)
{
    puts("hello, ceres");

    int answer = 6 * 7;
    printf("the answer is %d (0x%X, binary %b)\n", answer, answer, answer);
    printf("pi is about %.4f, and 1/3 is %.6f\n", 3.14159265f, 1.0f / 3.0f);

    char text[32];
    printf("as a string: [%s]\n", itoa(answer, text, 16));
    return 0;
}
