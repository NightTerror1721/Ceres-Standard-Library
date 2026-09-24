// scanf and friends: ONE scanner that reads through a get-a-character and give-it-back pair, so the same
// code serves a string (sscanf), a stream (fscanf) and the terminal (scanf).
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "ctype.h"
#include "stdint.h"
#include "stdarg.h"
#include "file_priv.h"

struct scan
{
    int (*get)(void*);
    void (*unget)(void*, int);
    void* ctx;
    int count;                 // characters consumed so far (what %n reports)
    int hit_eof;               // a read found the end of the input
};

static int next_char(struct scan* s)
{
    int c = s->get(s->ctx);
    if (c < 0)
    {
        s->hit_eof = 1;
        return -1;
    }
    s->count++;
    return c;
}

static void give_back(struct scan* s, int c)
{
    if (c >= 0)
    {
        s->unget(s->ctx, c);
        s->count--;
    }
}

static void skip_space(struct scan* s)
{
    for (;;)
    {
        int c = next_char(s);
        if (c < 0)
            return;
        if (!isspace(c))
        {
            give_back(s, c);
            return;
        }
    }
}

#define FIELD_MAX 64

static int is_digit_in_base(int c, int base)
{
    int d;
    if (c >= '0' && c <= '9')      d = c - '0';
    else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
    else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
    else return 0;
    return d < base;
}

// Reads an integer field of at most `width` characters into buf. Returns its length, 0 when there is no number.
static int read_integer(struct scan* s, int width, int base, char* buf)
{
    int n = 0;
    skip_space(s);
    int c = next_char(s);
    if ((c == '-' || c == '+') && width > 1)
    {
        buf[n++] = (char)c;
        width--;
        c = next_char(s);
    }
    int digits = 0;
    // "0x" opens a hexadecimal number in base 16 and in base 0 (which also reads a leading 0 as octal)
    if (c == '0' && (base == 0 || base == 16) && width > 1)
    {
        buf[n++] = '0';
        width--;
        digits = 1;
        c = next_char(s);
        if ((c == 'x' || c == 'X') && width > 1)
        {
            buf[n++] = (char)c;
            width--;
            base = 16;
            digits = 0;                                  // the prefix alone is not a number
            c = next_char(s);
        }
        else if (base == 0)
        {
            base = 8;
        }
    }
    if (base == 0)
        base = 10;
    while (c >= 0 && width > 0 && n < FIELD_MAX - 1 && is_digit_in_base(c, base))
    {
        buf[n++] = (char)c;
        width--;
        digits++;
        c = next_char(s);
    }
    give_back(s, c);
    buf[n] = 0;
    return digits > 0 ? n : 0;
}

// A floating-point field: sign, digits, '.', digits, an exponent, or inf/infinity/nan.
static int read_float_field(struct scan* s, int width, char* buf)
{
    int n = 0;
    skip_space(s);
    int c = next_char(s);
    if ((c == '-' || c == '+') && width > 0)
    {
        buf[n++] = (char)c;
        width--;
        c = next_char(s);
    }
    int digits = 0;
    if (c >= 0 && (isalpha(c)))                          // inf, infinity, nan
    {
        while (c >= 0 && width > 0 && n < FIELD_MAX - 1 && isalpha(c))
        {
            buf[n++] = (char)c;
            width--;
            c = next_char(s);
        }
        give_back(s, c);
        buf[n] = 0;
        return n;
    }
    while (c >= 0 && width > 0 && n < FIELD_MAX - 1 && isdigit(c))
    {
        buf[n++] = (char)c;
        width--;
        digits++;
        c = next_char(s);
    }
    if (c == '.' && width > 0 && n < FIELD_MAX - 1)
    {
        buf[n++] = '.';
        width--;
        c = next_char(s);
        while (c >= 0 && width > 0 && n < FIELD_MAX - 1 && isdigit(c))
        {
            buf[n++] = (char)c;
            width--;
            digits++;
            c = next_char(s);
        }
    }
    if (digits > 0 && (c == 'e' || c == 'E') && width > 1 && n < FIELD_MAX - 3)
    {
        // the exponent counts only if a digit follows; the characters are read ahead, so they are kept
        buf[n++] = (char)c;
        width--;
        c = next_char(s);
        if ((c == '-' || c == '+') && width > 1)
        {
            buf[n++] = (char)c;
            width--;
            c = next_char(s);
        }
        while (c >= 0 && width > 0 && n < FIELD_MAX - 1 && isdigit(c))
        {
            buf[n++] = (char)c;
            width--;
            c = next_char(s);
        }
    }
    give_back(s, c);
    buf[n] = 0;
    return digits > 0 ? n : 0;
}

// The set of a %[...] conversion, as a table of 256 flags. Returns the format index just past the ']'.
static int parse_scanset(const char* fmt, int i, unsigned char* set)
{
    int negate = 0;
    if (fmt[i] == '^')
    {
        negate = 1;
        i++;
    }
    for (int k = 0; k < 256; k++)
        set[k] = 0;
    int first = 1;
    while (fmt[i] != 0 && (fmt[i] != ']' || first))
    {
        unsigned char lo = (unsigned char)fmt[i];
        if (fmt[i + 1] == '-' && fmt[i + 2] != 0 && fmt[i + 2] != ']')
        {
            unsigned char hi = (unsigned char)fmt[i + 2];
            for (int k = lo; k <= hi; k++)
                set[k] = 1;
            i += 3;
        }
        else
        {
            set[lo] = 1;
            i++;
        }
        first = 0;
    }
    if (negate)
        for (int k = 0; k < 256; k++)
            set[k] = (unsigned char)!set[k];
    return fmt[i] == ']' ? i + 1 : i;
}

static int scan_core(struct scan* s, const char* fmt, va_list ap)
{
    int assigned = 0;
    for (int i = 0; fmt[i] != 0;)
    {
        char f = fmt[i];
        if (isspace((unsigned char)f))
        {
            skip_space(s);
            i++;
            continue;
        }
        if (f != '%')
        {
            int c = next_char(s);
            if (c != (unsigned char)f)
            {
                give_back(s, c);
                return (assigned == 0 && s->hit_eof && c < 0) ? -1 : assigned;
            }
            i++;
            continue;
        }
        i++;
        if (fmt[i] == '%')
        {
            skip_space(s);
            int c = next_char(s);
            if (c != '%')
            {
                give_back(s, c);
                return (assigned == 0 && c < 0) ? -1 : assigned;
            }
            i++;
            continue;
        }

        int suppress = 0;
        if (fmt[i] == '*')
        {
            suppress = 1;
            i++;
        }
        int width = 0;
        while (fmt[i] >= '0' && fmt[i] <= '9')
        {
            width = width * 10 + (fmt[i] - '0');
            i++;
        }
        int narrow = 0;                                  // 'H' = hh, 'h' = h, 'W' = ll (64-bit), 0 = 32 bits
        for (;;)
        {
            char m = fmt[i];
            if (m == 'h') narrow = (narrow == 'h') ? 'H' : 'h';
            else if (m == 'l') narrow = (narrow == 'l') ? 'W' : 'l';
            else if (m == 'j' || m == 'q') narrow = 'W';       // intmax_t is long long
            else if (m == 'z' || m == 't' || m == 'L') { }
            else break;
            i++;
        }
        char conv = fmt[i];
        if (conv == 0)
            return assigned;
        i++;
        char field[FIELD_MAX];
        int failed = 0;

        if (conv == 'n')
        {
            if (!suppress)
                *va_arg(ap, int*) = s->count;
            continue;
        }
        if (conv == 'c')
        {
            int n = width > 0 ? width : 1;
            char* out = suppress ? 0 : va_arg(ap, char*);
            int got = 0;
            while (got < n)
            {
                int c = next_char(s);
                if (c < 0)
                    break;
                if (out != 0)
                    out[got] = (char)c;
                got++;
            }
            if (got < n)
                return (assigned == 0 && got == 0) ? -1 : assigned;   // a partial %c is a failure
            if (!suppress)
                assigned++;
            continue;
        }
        if (conv == 's' || conv == '[')
        {
            unsigned char set[256];
            if (conv == '[')
                i = parse_scanset(fmt, i, set);
            char* out = suppress ? 0 : va_arg(ap, char*);
            int limit = width > 0 ? width : 0x7FFFFFFF;
            if (conv == 's')
                skip_space(s);
            int got = 0;
            while (got < limit)
            {
                int c = next_char(s);
                if (c < 0)
                    break;
                int ok = (conv == 's') ? !isspace(c) : set[c];
                if (!ok)
                {
                    give_back(s, c);
                    break;
                }
                if (out != 0)
                    out[got] = (char)c;
                got++;
            }
            if (got == 0)
                return (assigned == 0 && s->hit_eof) ? -1 : assigned;
            if (out != 0)
            {
                out[got] = 0;
                assigned++;
            }
            continue;
        }
        if (conv == 'd' || conv == 'i' || conv == 'u' || conv == 'o' || conv == 'x' || conv == 'X' || conv == 'p')
        {
            int base = (conv == 'd' || conv == 'u') ? 10 : (conv == 'o' ? 8 : (conv == 'i' ? 0 : 16));
            int len = read_integer(s, width > 0 ? width : FIELD_MAX, base, field);
            if (len == 0)
                return (assigned == 0 && s->hit_eof) ? -1 : assigned;
            if (suppress)
                continue;
            char* end;
            int is_signed = (conv == 'd' || conv == 'i');
            if (narrow == 'W' && conv != 'p')
            {
                // A 64-bit destination: parse the whole field as `long long` and store through the
                // caller's pointer. `%p` has no wide form, so it stays on the 32-bit path above.
                if (is_signed) *va_arg(ap, long long*) = strtoll(field, &end, base);
                else           *va_arg(ap, unsigned long long*) = strtoull(field, &end, base);
                assigned++;
                continue;
            }
            unsigned int u = 0;
            int v = 0;
            if (is_signed) v = strtol(field, &end, base);
            else           u = strtoul(field, &end, base);
            if (conv == 'p')
            {
                *va_arg(ap, void**) = (void*)u;
            }
            else if (narrow == 'H')
            {
                if (is_signed) *va_arg(ap, signed char*) = (signed char)v;
                else           *va_arg(ap, unsigned char*) = (unsigned char)u;
            }
            else if (narrow == 'h')
            {
                if (is_signed) *va_arg(ap, short*) = (short)v;
                else           *va_arg(ap, unsigned short*) = (unsigned short)u;
            }
            else
            {
                if (is_signed) *va_arg(ap, int*) = v;
                else           *va_arg(ap, unsigned int*) = u;
            }
            assigned++;
            continue;
        }
        if (conv == 'f' || conv == 'F' || conv == 'e' || conv == 'E' || conv == 'g' || conv == 'G' || conv == 'a' || conv == 'A')
        {
            int len = read_float_field(s, width > 0 ? width : FIELD_MAX, field);
            if (len == 0)
                return (assigned == 0 && s->hit_eof) ? -1 : assigned;
            if (suppress)
                continue;
            char* end;
            float v = strtof(field, &end);
            if (end == field)
                return assigned;                         // "inf" was not a word after all, or a lone sign
            *va_arg(ap, float*) = v;
            assigned++;
            continue;
        }
        (void)failed;
        return assigned;                                 // an unknown conversion ends the scan
    }
    return assigned;
}

// ---- the three sources ----

struct string_source
{
    const char* text;
    int at;
};

static int string_get(void* ctx)
{
    struct string_source* src = (struct string_source*)ctx;
    int c = (unsigned char)src->text[src->at];
    if (c == 0)
        return -1;
    src->at++;
    return c;
}

static void string_unget(void* ctx, int c)
{
    struct string_source* src = (struct string_source*)ctx;
    src->at--;
}

static int stream_get(void* ctx)
{
    return fgetc((FILE*)ctx);
}

static void stream_unget(void* ctx, int c)
{
    ungetc(c, (FILE*)ctx);
}

int vsscanf(const char* text, const char* fmt, va_list ap)
{
    struct string_source src;
    src.text = text;
    src.at = 0;
    struct scan s;
    s.get = string_get;
    s.unget = string_unget;
    s.ctx = &src;
    s.count = 0;
    s.hit_eof = 0;
    return scan_core(&s, fmt, ap);
}

int vfscanf(FILE* f, const char* fmt, va_list ap)
{
    struct scan s;
    s.get = stream_get;
    s.unget = stream_unget;
    s.ctx = f;
    s.count = 0;
    s.hit_eof = 0;
    return scan_core(&s, fmt, ap);
}

int vscanf(const char* fmt, va_list ap)
{
    return vfscanf(stdin, fmt, ap);
}

int sscanf(const char* text, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsscanf(text, fmt, ap);
    va_end(ap);
    return r;
}

int fscanf(FILE* f, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vfscanf(f, fmt, ap);
    va_end(ap);
    return r;
}

int scanf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vscanf(fmt, ap);
    va_end(ap);
    return r;
}
