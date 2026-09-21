// The line reader at the end of its input: a last line with no newline is still a line, then -1; the prompts
// that used to loop for ever on an empty stream give up (read_choice) or take the default (read_yes_no).
#include "stdio.h"
#include "ceres/line.h"
#include "ceres/terminal.h"

int main(void)
{
    char line[32];
    int n;
    int count = 0;
    while ((n = line_read(line, sizeof(line))) >= 0)
    {
        count++;
        printf("line %d (%d): \"%s\"\n", count, n, line);
    }
    printf("ended after %d lines, term_eof=%d, buffer \"%s\"\n", count, term_eof(), line);

    int number = 99;
    printf("read_int: %d (number stays %d)\n", read_int("number? ", &number), number);
    printf("read_yes_no default yes: %d\n", read_yes_no("sure?", 1));
    printf("read_yes_no default no: %d\n", read_yes_no("sure?", 0));
    const char* options[] = { "alpha", "beta" };
    printf("read_choice: %d\n", read_choice("pick", options, 2));
    return 0;
}
