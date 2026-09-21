// The terminal reports the end of its input, so a program can read until there is no more: getchar() returns
// EOF, feof(stdin) turns true, and scanf, fgets and line_read all see the end instead of waiting forever.
// The input (test_stdin_eof.stdin) has no newline at the end of its last line.
#include "stdio.h"
#include "ceres/terminal.h"
#include "ceres/line.h"

int main(void)
{
    printf("end before reading: %d\n", term_eof());

    int c, chars = 0, lines = 0;
    while ((c = getchar()) != EOF)
    {
        chars++;
        if (c == '\n')
            lines++;
    }
    printf("chars=%d lines=%d feof=%d term_eof=%d\n", chars, lines, feof(stdin), term_eof());
    printf("read again: %d\n", getchar());

    int v = 0;
    printf("scanf: %d\n", scanf("%d", &v));
    char buf[16];
    printf("fgets: %s\n", fgets(buf, sizeof(buf), stdin) ? "a line" : "NULL");
    printf("line_read: %d\n", line_read(buf, sizeof(buf)));
    printf("term_read: %d\n", term_read(buf, sizeof(buf), TERM_READ_UNTIL_STATUS));

    clearerr(stdin);
    printf("after clearerr: feof=%d, getchar=%d, feof=%d\n", feof(stdin), getchar(), feof(stdin));
    return 0;
}
