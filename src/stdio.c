// Console I/O: characters, strings and the small number printers. printf and friends are in format.c.
#include "stdio.h"
#include "ceres/terminal.h"

int putchar(int c)
{
    term_write_char(c);
    return c;
}

int putstr(const char* s)
{
    int n = 0;
    while (s[n] != 0)
        n++;
    term_write(s, n);   // one block transfer, not a byte loop
    return n;
}

int puts(const char* s)
{
    int n = putstr(s);
    putchar('\n');
    return n + 1;
}

int putuint(unsigned int v)
{
    char digits[10];                         // 4294967295 has ten
    int n = 0;
    do
    {
        digits[n] = (char)('0' + v % 10u);
        n++;
        v /= 10u;
    } while (v != 0);
    char text[10];
    for (int i = 0; i < n; i++)
        text[i] = digits[n - 1 - i];
    term_write(text, n);
    return n;
}

int putint(int v)
{
    if (v < 0)
    {
        putchar('-');
        return 1 + putuint(0u - (unsigned int)v);   // 0u - x is |x| even for INT_MIN
    }
    return putuint((unsigned int)v);
}

int puthex(unsigned int v)
{
    char text[10];
    text[0] = '0';
    text[1] = 'x';
    int shift = 28;
    while (shift > 0 && ((v >> shift) & 0xF) == 0)
        shift -= 4;
    int n = 2;
    for (; shift >= 0; shift -= 4)
    {
        int nib = (int)((v >> shift) & 0xF);
        text[n] = (char)(nib < 10 ? '0' + nib : 'a' + (nib - 10));
        n++;
    }
    term_write(text, n);
    return n;
}

int putbin(unsigned int v, int bits)
{
    if (bits > 32) bits = 32;
    if (bits < 1)  bits = 1;
    char text[32];
    for (int i = 0; i < bits; i++)
        text[i] = (v & (1u << (bits - 1 - i))) ? '1' : '0';
    term_write(text, bits);
    return bits;
}
