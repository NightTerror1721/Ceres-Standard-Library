// A line calculator: "12 + 3.5", "2 ^ 10", "9 sqrt", "10 % 3". Ends on "quit".
// Shows line input, sscanf and the math functions. Every operator takes two numbers except `sqrt`.
#include "stdio.h"
#include "string.h"
#include "math.h"
#include "ceres/line.h"

int main(void)
{
    char line[64];
    puts("calc: number op number  (ops: + - * / % ^), number sqrt, or quit");
    for (;;)
    {
        line_prompt("> ", line, sizeof(line));
        if (strcmp(line, "quit") == 0)
            break;

        float a, b;
        char op[8];
        int n = sscanf(line, "%f %7s %f", &a, op, &b);
        if (n == 2 && strcmp(op, "sqrt") == 0)
        {
            if (a < 0.0f)
                puts("sqrt of a negative number");
            else
                printf("= %g\n", sqrt(a));
            continue;
        }
        if (n != 3 || op[1] != 0)
        {
            puts("?");
            continue;
        }
        switch (op[0])
        {
        case '+': printf("= %g\n", a + b); break;
        case '-': printf("= %g\n", a - b); break;
        case '*': printf("= %g\n", a * b); break;
        case '/':
            if (b == 0.0f) puts("division by zero");
            else printf("= %g\n", a / b);
            break;
        case '%':
            if (b == 0.0f) puts("division by zero");
            else printf("= %g\n", fmod(a, b));
            break;
        case '^': printf("= %g\n", pow(a, b)); break;
        default:  puts("?"); break;
        }
    }
    puts("bye");
    return 0;
}
