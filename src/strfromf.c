// strfromf (C23) and ftoa_shortest, kept apart from the format engine's callers: a program that uses neither does
// not carry the shortest-digits search.
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "errno.h"
#include "fconv_priv.h"

// C23: format is "%" [ "." precision ] one of a A e E f F g G, and nothing else - no flags, no width, no length.
int strfromf(char* restrict s, size_t n, const char* restrict format, float fp)
{
    const char* f = format;
    if (f == 0 || *f++ != '%')
    {
        errno = EINVAL;
        return -1;
    }
    if (*f == '.')
    {
        f++;
        while (*f >= '0' && *f <= '9')
            f++;
    }
    char c = *f++;
    if (c == 0 || *f != 0 || (c != 'a' && c != 'A' && c != 'e' && c != 'E' && c != 'f' && c != 'F' && c != 'g' && c != 'G'))
    {
        errno = EINVAL;
        return -1;
    }
    return snprintf(s, n, format, fp);
}

int ftoa_shortest(char* buf, size_t size, float x)
{
    char text[40];
    int n = 0;
    union { float f; unsigned int u; } b;
    b.f = x;
    if (b.u >> 31)
        text[n++] = '-';
    if (((b.u >> 23) & 255u) == 255u)
    {
        const char* word = (b.u & 0x7FFFFFu) ? "nan" : "inf";
        if ((b.u & 0x7FFFFFu) && n)
            n = 0;                                  // a NaN has no sign to show
        for (int i = 0; i < 3; i++) text[n++] = word[i];
    }
    else if ((b.u & 0x7FFFFFFFu) == 0)
    {
        text[n++] = '0';
    }
    else
    {
        char dg[FCONV_MAX_DIGITS];
        int X;
        b.u &= 0x7FFFFFFFu;
        int nd = __fconv_shortest(b.f, dg, &X);
        if (X >= -4 && X < 9)
        {
            // Plain: 0.001, 12.5, 100000000.
            if (X < 0)
            {
                text[n++] = '0';
                text[n++] = '.';
                for (int i = 0; i < -X - 1; i++) text[n++] = '0';
                for (int i = 0; i < nd; i++) text[n++] = dg[i];
            }
            else
            {
                for (int i = 0; i <= X; i++) text[n++] = i < nd ? dg[i] : '0';
                if (nd > X + 1)
                {
                    text[n++] = '.';
                    for (int i = X + 1; i < nd; i++) text[n++] = dg[i];
                }
            }
        }
        else
        {
            text[n++] = dg[0];
            if (nd > 1)
            {
                text[n++] = '.';
                for (int i = 1; i < nd; i++) text[n++] = dg[i];
            }
            text[n++] = 'e';
            text[n++] = X < 0 ? '-' : '+';
            int ex = X < 0 ? -X : X;
            text[n++] = (char)('0' + ex / 10);         // two digits at least, as printf writes them
            text[n++] = (char)('0' + ex % 10);
        }
    }
    text[n] = 0;
    if (buf != 0 && size > 0)
    {
        size_t copy = (size_t)n < size - 1 ? (size_t)n : size - 1;
        memcpy(buf, text, copy);
        buf[copy] = 0;
    }
    return n;
}
