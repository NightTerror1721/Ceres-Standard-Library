#include "ceres/line.h"
#include "ceres/terminal.h"
#include "stdio.h"
#include "stdlib.h"
#include "ctype.h"
#include "string.h"

// One byte that line_read looked at and did not use (after a '\r' that was not followed by '\n').
static int pushed_back = -1;

int line_available(void)
{
    return term_bytes_available() + (pushed_back >= 0 ? 1 : 0);
}

void line_discard(void)
{
    pushed_back = -1;
    while (term_read_ready())
        term_read_char(TERM_READ_NON_BLOCKING);
}

static int next_byte(void)
{
    if (pushed_back >= 0)
    {
        int c = pushed_back;
        pushed_back = -1;
        return c;
    }
    return term_read_char(TERM_READ_UNTIL_STATUS);           // waits for the next byte
}

int line_read(char* buf, int max)
{
    if (buf == 0 || max < 1)
        return -1;
    int n = 0;
    for (;;)
    {
        int c = next_byte();
        if (c == '\r')
        {
            if (term_read_ready())
            {
                // "\r\n" is ONE line ending; a "\r" followed by anything else ends the line by itself
                int after = term_read_char(TERM_READ_NON_BLOCKING);
                if (after != '\n')
                    pushed_back = after;
            }
            break;
        }
        if (c == '\n')
            break;
        if (n < max - 1)
        {
            buf[n] = (char)c;
            n++;
        }
    }
    buf[n] = 0;
    return n;
}

int line_prompt(const char* prompt, char* buf, int max)
{
    putstr(prompt);
    return line_read(buf, max);
}

static int only_blanks(const char* s)
{
    while (*s != 0)
    {
        if (!isspace((unsigned char)*s))
            return 0;
        s++;
    }
    return 1;
}

int read_int(const char* prompt, int* out)
{
    char line[64];
    line_prompt(prompt, line, sizeof(line));
    char* end;
    int v = strtol(line, &end, 10);
    if (end == line || !only_blanks(end))
        return -1;
    *out = v;
    return 0;
}

int read_float(const char* prompt, float* out)
{
    char line[64];
    line_prompt(prompt, line, sizeof(line));
    char* end;
    float v = strtof(line, &end);
    if (end == line || !only_blanks(end))
        return -1;
    *out = v;
    return 0;
}

int read_yes_no(const char* prompt, int default_yes)
{
    char line[16];
    for (;;)
    {
        putstr(prompt);
        putstr(default_yes ? " [Y/n] " : " [y/N] ");
        line_read(line, sizeof(line));
        // trim and lower-case in place
        int start = 0;
        while (line[start] != 0 && isspace((unsigned char)line[start]))
            start++;
        int end = (int)strlen(line);
        while (end > start && isspace((unsigned char)line[end - 1]))
            end--;
        line[end] = 0;
        char* word = line + start;
        for (int i = 0; word[i] != 0; i++)
            word[i] = (char)tolower((unsigned char)word[i]);

        if (word[0] == 0)
            return default_yes != 0;
        if (strcmp(word, "y") == 0 || strcmp(word, "yes") == 0)
            return 1;
        if (strcmp(word, "n") == 0 || strcmp(word, "no") == 0)
            return 0;
        puts("please answer y or n");
    }
}

int read_choice(const char* prompt, const char* const* options, int n)
{
    for (;;)
    {
        puts(prompt);
        for (int i = 0; i < n; i++)
            printf("  %d) %s\n", i + 1, options[i]);
        int choice;
        if (read_int("> ", &choice) == 0 && choice >= 1 && choice <= n)
            return choice;
        printf("enter a number from 1 to %d\n", n);
    }
}
