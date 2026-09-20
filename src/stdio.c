#include "stdio.h"
#include "ceres/terminal.h"
#include "math.h"   // trunc, for putfloat

int putchar(int c)
{
    term_write_char(c);
    return c;
}

int getchar(void)
{
    return term_read_char(TERM_READ_NON_BLOCKING);   // -1 when nothing is buffered
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
    // Digits are emitted most-significant-first with no buffer: a local char[]
    // buffer trips a Ceres-C regalloc miscompile at -O2, so we do not use one.
    if (v == 0)
    {
        putchar('0');
        return 1;
    }
    unsigned int p = 1;
    while (p <= v / 10)   // p * 10 <= v, so p * 10 never overflows
        p *= 10;
    int n = 0;
    while (p > 0)
    {
        putchar((char)('0' + ((v / p) % 10)));
        p /= 10;
        n++;
    }
    return n;
}

int putint(int v)
{
    if (v < 0)
    {
        putchar('-');
        return 1 + putuint((unsigned int)(-(v + 1)) + 1u);
    }
    return putuint((unsigned int)v);
}

int puthex(unsigned int v)
{
    putstr("0x");
    int shift = 28;
    while (shift > 0 && ((v >> shift) & 0xF) == 0)
        shift -= 4;
    int n = 0;
    for (; shift >= 0; shift -= 4)
    {
        int nib = (int)((v >> shift) & 0xF);
        putchar((char)(nib < 10 ? '0' + nib : 'a' + (nib - 10)));
        n++;
    }
    return n + 2;
}

int putbin(unsigned int v, int bits)
{
    if (bits > 32) bits = 32;
    if (bits < 1)  bits = 1;
    for (int i = bits - 1; i >= 0; i--)
        putchar((v & (1u << i)) ? '1' : '0');
    return bits;
}

int putfloat(float f, int decimals)
{
    int n = 0;
    if (f < 0.0f)
    {
        putchar('-');
        n = 1;
        f = -f;
    }
    if (decimals < 0) decimals = 0;
    if (decimals > 9) decimals = 9;

    unsigned int scale = 1;
    for (int i = 0; i < decimals; i++)
        scale *= 10;

    float integral = trunc(f);
    float frac = f - integral;
    unsigned int fd = (unsigned int)(frac * (float)scale + 0.5f);   // rounded fraction
    if (fd >= scale)
    {
        fd = 0;              // carried into the integer part (e.g. 2.999 -> 3.000)
        integral += 1.0f;
    }

    n += putuint((unsigned int)integral);

    if (decimals > 0)
    {
        putchar('.');
        n++;
        unsigned int sc = scale / 10;
        while (sc > 0)
        {
            putchar((char)('0' + ((fd / sc) % 10)));
            sc /= 10;
            n++;
        }
    }
    return n;
}

int vprintf(const char* fmt, __builtin_va_list ap)
{
    int count = 0;
    for (int i = 0; fmt[i] != 0; i++)
    {
        char c = fmt[i];
        if (c != '%')
        {
            putchar(c);
            count++;
            continue;
        }

        i++;
        c = fmt[i];
        if (c == 0)
            break;

        if (c == '%') { putchar('%'); count++; }
        else if (c == 'c') { putchar(__builtin_va_arg(ap, int)); count++; }
        else if (c == 's') { count += putstr(__builtin_va_arg(ap, const char*)); }
        else if (c == 'd' || c == 'i') { count += putint(__builtin_va_arg(ap, int)); }
        else if (c == 'u') { count += putuint(__builtin_va_arg(ap, unsigned int)); }
        else if (c == 'x' || c == 'X' || c == 'p') { count += puthex(__builtin_va_arg(ap, unsigned int)); }
        else if (c == 'b') { count += putbin(__builtin_va_arg(ap, unsigned int), 32); }
        else if (c == 'f') { count += putfloat(__builtin_va_arg(ap, float), 6); }
        else { putchar('%'); putchar(c); count += 2; }
    }
    return count;
}

int printf(const char* fmt, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    __builtin_va_end(ap);
    return r;
}
