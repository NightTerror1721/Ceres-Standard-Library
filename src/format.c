// format.c - one format interpreter for printf/snprintf/sprintf/vprintf/vsnprintf.
// The output goes through a "sink" (a function that takes one character), so the same code
// serves the terminal, a memory buffer, and (later) a FILE.
//
// Conversions: %d %i %u %o %x %X %b %c %s %p %n %f %F %e %E %g %G %%
// Flags: - + space # 0; width and precision, either digits or *; length modifiers hh h l ll z t j L
// (a `long` is 32 bits, `long long` is 64: `ll` reads a wide argument as two words, `hh`/`h` narrow
// the 32-bit read, and everything else is 32 bits). A float argument is read as the f32 it is: there
// is no double promotion (see stdarg.h).
//
// Accuracy: a float has a 24-bit mantissa, so about 7 significant digits are exact; the digits
// after those are deterministic noise, where C would print the exact binary value. %.0f rounds
// half up, not half to even.
#include "stdio.h"
#include "stdarg.h"
#include "string.h"
#include "stddef.h"
#include "stdint.h"
#include "math.h"
#include "ceres/terminal.h"

#define TERM_CHUNK 64

struct __sink
{
    void (*put)(struct __sink*, int);
    char* buf;                  // memory sinks: the destination
    unsigned int cap;           // memory sinks: its size
    unsigned int len;           // characters produced so far (also what %n reports)
    unsigned int used;          // terminal sink: bytes waiting in chunk
    void (*ext)(void*, int);    // external sink (fprintf): a function and its context
    void* ext_ctx;
    char chunk[TERM_CHUNK];     // terminal sink: goes out as ONE block transfer instead of a store per byte
};

static void term_flush_chunk(struct __sink* s)
{
    if (s->used != 0)
        term_write(s->chunk, (int)s->used);
    s->used = 0;
}

static void sink_term(struct __sink* s, int c)
{
    if (s->used == TERM_CHUNK)
        term_flush_chunk(s);
    s->chunk[s->used] = (char)c;
    s->used = s->used + 1;
    s->len++;
}

static void sink_ext(struct __sink* s, int c)
{
    s->ext(s->ext_ctx, c);
    s->len++;
}

static void sink_mem(struct __sink* s, int c)
{
    if (s->cap != 0 && s->len < s->cap - 1)
        s->buf[s->len] = (char)c;
    s->len++;
}

static void pad(struct __sink* s, int n, int c)
{
    while (n > 0) { s->put(s, c); n--; }
}

static void puts_n(struct __sink* s, const char* p, int n)
{
    for (int i = 0; i < n; i++) s->put(s, p[i]);
}

// flags
#define F_LEFT  1
#define F_PLUS  2
#define F_SPACE 4
#define F_ALT   8
#define F_ZERO  16

// sign/prefix + zeros + body, padded to `width`.
static void put_field(struct __sink* s, const char* prefix, const char* body, int blen,
                      int zeros, int width, int flags, int allow_zero_pad)
{
    int plen = 0;
    while (prefix[plen] != 0) plen++;
    int total = plen + zeros + blen;
    int fill = width > total ? width - total : 0;
    int zero_fill = (flags & F_ZERO) != 0 && (flags & F_LEFT) == 0 && allow_zero_pad;
    if ((flags & F_LEFT) == 0 && !zero_fill) pad(s, fill, ' ');
    puts_n(s, prefix, plen);
    if (zero_fill) pad(s, fill, '0');
    pad(s, zeros, '0');
    puts_n(s, body, blen);
    if (flags & F_LEFT) pad(s, fill, ' ');
}

static int utoa_base(unsigned int v, int base, int upper, char* out)   // digits into out, returns length
{
    char tmp[34];
    int n = 0;
    if (v == 0) { tmp[0] = '0'; n = 1; }
    while (v != 0)
    {
        int d = (int)(v % (unsigned int)base);
        tmp[n] = (char)(d < 10 ? '0' + d : (upper ? 'A' : 'a') + (d - 10));
        n++;
        v /= (unsigned int)base;
    }
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    return n;
}

static void fmt_integer(struct __sink* s, unsigned int v, int is_signed, int neg, int base, int upper,
                        int width, int prec, int flags)
{
    char digits[34];
    char prefix[4];
    int np = 0;
    if (is_signed)
    {
        if (neg) prefix[np++] = '-';
        else if (flags & F_PLUS) prefix[np++] = '+';
        else if (flags & F_SPACE) prefix[np++] = ' ';
    }
    int n = utoa_base(v, base, upper, digits);
    if ((flags & F_ALT) && base == 16 && v != 0) { prefix[np++] = '0'; prefix[np++] = upper ? 'X' : 'x'; }
    if ((flags & F_ALT) && base == 8 && v != 0 && prec <= n) { prec = n + 1; }
    prefix[np] = 0;
    if (prec == 0 && v == 0) n = 0;                    // "%.0d" of 0 prints nothing
    int zeros = prec > n ? prec - n : 0;
    put_field(s, prefix, digits, n, zeros, width, flags, prec < 0);
}

// The wide counterpart: same shape, but the magnitude is 64 bits. Two words divide down one digit at
// a time; digit count is bounded by 64 for base 2.
static int utoa_base64(uint64_t v, int base, int upper, char* out)
{
    char tmp[66];
    int n = 0;
    if (v == 0ULL) { tmp[0] = '0'; n = 1; }
    while (v != 0ULL)
    {
        unsigned int d = (unsigned int)(v % (uint64_t)base);
        tmp[n] = (char)(d < 10u ? '0' + (int)d : (upper ? 'A' : 'a') + ((int)d - 10));
        n++;
        v = v / (uint64_t)base;
    }
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    return n;
}

static void fmt_integer64(struct __sink* s, uint64_t v, int is_signed, int neg, int base, int upper,
                          int width, int prec, int flags)
{
    char digits[66];
    char prefix[4];
    int np = 0;
    if (is_signed)
    {
        if (neg) prefix[np++] = '-';
        else if (flags & F_PLUS) prefix[np++] = '+';
        else if (flags & F_SPACE) prefix[np++] = ' ';
    }
    int n = utoa_base64(v, base, upper, digits);
    if ((flags & F_ALT) && base == 16 && v != 0ULL) { prefix[np++] = '0'; prefix[np++] = upper ? 'X' : 'x'; }
    if ((flags & F_ALT) && base == 8 && v != 0ULL && prec <= n) { prec = n + 1; }
    prefix[np] = 0;
    if (prec == 0 && v == 0ULL) n = 0;
    int zeros = prec > n ? prec - n : 0;
    put_field(s, prefix, digits, n, zeros, width, flags, prec < 0);
}

// ---- floating point -------------------------------------------------------------
union fbits { float f; unsigned int u; };

// `nd` significant decimal digits of a positive finite v, rounded; *exp10 = decimal exponent.
// Good for ~7 exact digits (a float has a 24-bit mantissa); later digits are deterministic noise.
static void float_digits(float v, int nd, char* out, int* exp10)
{
    int e = 0;
    if (v != 0.0f)
    {
        while (v >= 10.0f) { v = v / 10.0f; e++; }
        while (v < 1.0f)   { v = v * 10.0f; e--; }
    }
    int d[40];
    if (nd > 38) nd = 38;
    float m = v;
    for (int i = 0; i <= nd; i++)
    {
        int digit = (int)m;
        if (digit > 9) digit = 9;
        if (digit < 0) digit = 0;
        d[i] = digit;
        m = (m - (float)digit) * 10.0f;
    }
    if (d[nd] >= 5)                                    // round half up on the discarded digit
    {
        int i = nd - 1;
        while (i >= 0 && d[i] == 9) { d[i] = 0; i--; }
        if (i >= 0) d[i]++;
        else { d[0] = 1; for (int k = 1; k < nd; k++) d[k] = 0; e++; }
    }
    for (int i = 0; i < nd; i++) out[i] = (char)('0' + d[i]);
    *exp10 = e;
}

static int put_special(struct __sink* s, float v, int upper, int width, int flags)
{
    union fbits b; b.f = v;
    if (((b.u >> 23) & 255u) != 255u) return 0;
    const char* txt;
    int neg = (int)(b.u >> 31);
    if ((b.u & 0x7FFFFFu) != 0) { txt = upper ? "NAN" : "nan"; neg = 0; }
    else txt = upper ? "INF" : "inf";
    char prefix[2]; prefix[0] = 0; prefix[1] = 0;
    if (neg) prefix[0] = '-'; else if (flags & F_PLUS) prefix[0] = '+'; else if (flags & F_SPACE) prefix[0] = ' ';
    put_field(s, prefix, txt, 3, 0, width, flags & ~F_ZERO, 0);
    return 1;
}

static void fmt_float(struct __sink* s, float v, int conv, int width, int prec, int flags)
{
    int upper = (conv == 'F' || conv == 'E' || conv == 'G');
    if (put_special(s, v, upper, width, flags)) return;
    union fbits sb; sb.f = v;
    int neg = (int)(sb.u >> 31);
    if (neg) v = -v;
    char prefix[2]; prefix[0] = 0; prefix[1] = 0;
    if (neg) prefix[0] = '-'; else if (flags & F_PLUS) prefix[0] = '+'; else if (flags & F_SPACE) prefix[0] = ' ';
    if (prec < 0) prec = 6;
    char body[80];
    int n = 0;
    int lc = conv | 32;                                // to lower case

    if (lc == 'f')
    {
        char ip[48];
        int ni = 0;
        float ipart = trunc(v);
        float frac = v - ipart;
        if (ipart < 4294967296.0f) ni = utoa_base((unsigned int)ipart, 10, 0, ip);
        else
        {
            // At or above 2^32 the value is m * 2^e2 with m < 2^24, and it has no fractional bits. Its exact
            // digits come from writing m in a little-endian decimal array and doubling it e2 times
            // (at most 39 digits): dividing a float by ten instead loses bits at every step.
            union fbits ib; ib.f = ipart;
            unsigned int m = (ib.u & 0x7FFFFFu) | 0x800000u;
            int e2 = (int)((ib.u >> 23) & 255u) - 150;
            char dec[48];
            int k = 0;
            while (m != 0) { dec[k] = (char)(m % 10u); k++; m /= 10u; }
            for (; e2 > 0; e2--)
            {
                int carry = 0;
                for (int j = 0; j < k; j++) { int t = dec[j] * 2 + carry; dec[j] = (char)(t % 10); carry = t / 10; }
                if (carry != 0) { dec[k] = (char)carry; k++; }
            }
            for (int i = 0; i < k; i++) ip[i] = (char)('0' + dec[k - 1 - i]);
            ni = k;
        }
        int pr = prec > 9 ? 9 : prec;
        unsigned int scale = 1;
        for (int i = 0; i < pr; i++) scale *= 10u;
        unsigned int fd = (unsigned int)(frac * (float)scale + 0.5f);
        int carry = 0;
        if (pr > 0 && fd >= scale) { fd = 0; carry = 1; }
        else if (pr == 0 && frac >= 0.5f) carry = 1;
        if (carry)                                     // the rounding carried into the integer part
        {
            int i = ni - 1;
            while (i >= 0 && carry)
            {
                if (ip[i] == '9') ip[i] = '0';
                else { ip[i] = (char)(ip[i] + 1); carry = 0; }
                i--;
            }
            if (carry)
            {
                for (int k = ni; k > 0; k--) ip[k] = ip[k - 1];
                ip[0] = '1';
                ni++;
            }
        }
        for (int i = 0; i < ni; i++) { body[n] = ip[i]; n++; }
        if (prec > 0 || (flags & F_ALT)) { body[n] = '.'; n++; }
        if (pr > 0)
        {
            char fs[12];
            int nf = utoa_base(fd, 10, 0, fs);
            for (int i = nf; i < pr; i++) { body[n] = '0'; n++; }
            for (int i = 0; i < nf; i++) { body[n] = fs[i]; n++; }
        }
        for (int i = pr; i < prec && n < 78; i++) { body[n] = '0'; n++; }
        put_field(s, prefix, body, n, 0, width, flags, 1);
        return;
    }

    // e / g
    int P = prec;
    if (lc == 'g') { if (P == 0) P = 1; }
    else P = prec + 1;
    char dg[40];
    int X = 0;
    float_digits(v, P, dg, &X);
    int use_exp = 1;
    if (lc == 'g') use_exp = !(X >= -4 && X < P);
    if (lc == 'g' && !(flags & F_ALT))                 // strip trailing zeros of the significant digits
    {
        int keep = P;
        while (keep > 1 && dg[keep - 1] == '0') keep--;
        P = keep;
    }
    if (use_exp)
    {
        body[n] = dg[0]; n++;
        if (P > 1 || (flags & F_ALT)) { body[n] = '.'; n++; }
        for (int i = 1; i < P; i++) { body[n] = dg[i]; n++; }
        body[n] = upper ? 'E' : 'e'; n++;
        int ex = X < 0 ? -X : X;
        body[n] = X < 0 ? '-' : '+'; n++;
        if (ex < 10) { body[n] = '0'; n++; }
        char es[8];
        int ne = utoa_base((unsigned int)ex, 10, 0, es);
        for (int i = 0; i < ne; i++) { body[n] = es[i]; n++; }
    }
    else if (X >= 0)                                   // 123.456 : the point goes after X+1 digits
    {
        for (int i = 0; i <= X; i++) { body[n] = i < P ? dg[i] : '0'; n++; }
        if (P > X + 1 || (flags & F_ALT)) { body[n] = '.'; n++; }
        for (int i = X + 1; i < P; i++) { body[n] = dg[i]; n++; }
    }
    else                                               // 0.000123
    {
        body[n] = '0'; n++;
        body[n] = '.'; n++;
        for (int i = 0; i < -X - 1; i++) { body[n] = '0'; n++; }
        for (int i = 0; i < P; i++) { body[n] = dg[i]; n++; }
    }
    put_field(s, prefix, body, n, 0, width, flags, 1);
}

// ---- the interpreter ----------------------------------------------------------------
static int vformat(struct __sink* s, const char* fmt, va_list ap)
{
    for (int i = 0; fmt[i] != 0; i++)
    {
        char c = fmt[i];
        if (c != '%') { s->put(s, c); continue; }
        i++;
        int flags = 0;
        for (;;)
        {
            c = fmt[i];
            if (c == '-') flags |= F_LEFT;
            else if (c == '+') flags |= F_PLUS;
            else if (c == ' ') flags |= F_SPACE;
            else if (c == '#') flags |= F_ALT;
            else if (c == '0') flags |= F_ZERO;
            else break;
            i++;
        }
        int width = 0;
        if (fmt[i] == '*')
        {
            width = va_arg(ap, int);
            if (width < 0) { flags |= F_LEFT; width = -width; }
            i++;
        }
        else while (fmt[i] >= '0' && fmt[i] <= '9') { width = width * 10 + (fmt[i] - '0'); i++; }
        int prec = -1;
        if (fmt[i] == '.')
        {
            i++;
            prec = 0;
            if (fmt[i] == '*') { prec = va_arg(ap, int); i++; }
            else while (fmt[i] >= '0' && fmt[i] <= '9') { prec = prec * 10 + (fmt[i] - '0'); i++; }
        }
        int lenmod = 0;                                // 'H' = hh, 'h', 'W' = ll (64-bit), 0 = 32 bits
        for (;;)
        {
            c = fmt[i];
            if (c == 'h') { lenmod = (lenmod == 'h') ? 'H' : 'h'; }
            else if (c == 'l') { lenmod = (lenmod == 'l') ? 'W' : 'l'; }
            else if (c == 'z' || c == 't' || c == 'j' || c == 'L' || c == 'q') { }
            else break;
            i++;
        }
        c = fmt[i];
        if (c == 0) break;
        if (c == '%') { s->put(s, '%'); }
        else if (c == 'c')
        {
            char ch[2]; ch[0] = (char)va_arg(ap, int); ch[1] = 0;
            put_field(s, "", ch, 1, 0, width, flags & ~F_ZERO, 0);
        }
        else if (c == 's')
        {
            const char* str = va_arg(ap, const char*);
            if (str == 0) str = "(null)";
            int n = 0;
            while (str[n] != 0 && (prec < 0 || n < prec)) n++;
            put_field(s, "", str, n, 0, width, flags & ~F_ZERO, 0);
        }
        else if (c == 'd' || c == 'i')
        {
            if (lenmod == 'W')
            {
                // A 64-bit argument travels through `...` as two words, low first; read both and
                // reassemble. The sign is taken from the assembled value's top bit.
                unsigned int lo = va_arg(ap, unsigned int);
                unsigned int hi = va_arg(ap, unsigned int);
                uint64_t v = ((uint64_t)hi << 32) | (uint64_t)lo;
                int neg = ((int64_t)v) < 0;
                uint64_t mag = neg ? (uint64_t)(-(int64_t)v) : v;
                fmt_integer64(s, mag, 1, neg, 10, 0, width, prec, flags);
            }
            else
            {
                int v = va_arg(ap, int);
                if (lenmod == 'H') v = (int)((signed char)v);
                else if (lenmod == 'h') v = (int)((short)v);
                int neg = v < 0;
                unsigned int mag = neg ? (unsigned int)(-(v + 1)) + 1u : (unsigned int)v;
                fmt_integer(s, mag, 1, neg, 10, 0, width, prec, flags);
            }
        }
        else if (c == 'u' || c == 'x' || c == 'X' || c == 'o' || c == 'b')
        {
            int base = c == 'u' ? 10 : (c == 'o' ? 8 : (c == 'b' ? 2 : 16));
            if (lenmod == 'W')
            {
                unsigned int lo = va_arg(ap, unsigned int);
                unsigned int hi = va_arg(ap, unsigned int);
                uint64_t v = ((uint64_t)hi << 32) | (uint64_t)lo;
                fmt_integer64(s, v, 0, 0, base, c == 'X', width, prec, flags);
            }
            else
            {
                unsigned int v = va_arg(ap, unsigned int);
                if (lenmod == 'H') v = v & 255u;
                else if (lenmod == 'h') v = v & 65535u;
                fmt_integer(s, v, 0, 0, base, c == 'X', width, prec, flags);
            }
        }
        else if (c == 'p')
        {
            unsigned int v = va_arg(ap, unsigned int);
            if (v == 0)
                put_field(s, "0x", "0", 1, 0, width, flags & ~F_ZERO, 0);   // %#x drops the prefix for 0; a pointer keeps it
            else
                fmt_integer(s, v, 0, 0, 16, 0, width, prec, flags | F_ALT);
        }
        else if (c == 'f' || c == 'F' || c == 'e' || c == 'E' || c == 'g' || c == 'G')
        {
            float f = va_arg(ap, float);
            fmt_float(s, f, c, width, prec, flags);
        }
        else if (c == 'n') { int* p = va_arg(ap, int*); *p = (int)s->len; }
        else { s->put(s, '%'); s->put(s, c); }
    }
    return (int)s->len;
}

int vsnprintf(char* buf, size_t n, const char* fmt, va_list ap)
{
    struct __sink s;
    s.put = sink_mem; s.buf = buf; s.cap = n; s.len = 0; s.used = 0;
    int r = vformat(&s, fmt, ap);
    if (n != 0) buf[s.len < n - 1 ? s.len : n - 1] = 0;
    return r;
}

int vsprintf(char* buf, const char* fmt, va_list ap) { return vsnprintf(buf, 0x7FFFFFFF, fmt, ap); }

int snprintf(char* buf, size_t n, const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

int sprintf(char* buf, const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int r = vsnprintf(buf, 0x7FFFFFFF, fmt, ap);
    va_end(ap);
    return r;
}

int vprintf(const char* fmt, va_list ap)
{
    struct __sink s;
    s.put = sink_term; s.buf = 0; s.cap = 0; s.len = 0; s.used = 0;
    int r = vformat(&s, fmt, ap);
    term_flush_chunk(&s);
    return r;
}

// The engine into any character sink: what vfprintf (fprintf.c) uses to write to a stream.
int __vformat_ext(void (*put)(void*, int), void* ctx, const char* fmt, va_list ap)
{
    struct __sink s;
    s.put = sink_ext; s.buf = 0; s.cap = 0; s.len = 0; s.used = 0; s.ext = put; s.ext_ctx = ctx;
    return vformat(&s, fmt, ap);
}

int printf(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}
