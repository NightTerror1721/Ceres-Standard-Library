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
#include "ceres/utf8.h"
#include "fconv_priv.h"

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

void __stdout_write(const char* s, int n);   // stdio.c: the terminal, or stdout once it is buffered or reopened

static void term_flush_chunk(struct __sink* s)
{
    if (s->used != 0)
        __stdout_write(s->chunk, (int)s->used);
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
// Every digit is exact (src/fconv.c): a float is converted from its exact decimal expansion, rounded to nearest
// with ties to even at the place the conversion asks for. A precision past the float's last nonzero digit gets
// zeros, put out after the body without a buffer to hold them.
union fbits { float f; unsigned int u; };

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

// A number laid out as prefix, body, `zeros` zero digits, then tail, padded to `width` (with zeros
// between the prefix and the body for the 0 flag). The zeros go straight to the sink, so a precision
// of any size needs no buffer to hold it.
static void put_number(struct __sink* s, const char* prefix, const char* body, int blen, int zeros,
                       const char* tail, int tlen, int width, int flags)
{
    int plen = 0;
    while (prefix[plen] != 0) plen++;
    int total = plen + blen + zeros + tlen;
    int fill = width > total ? width - total : 0;
    int zero_fill = (flags & F_ZERO) != 0 && (flags & F_LEFT) == 0;
    if ((flags & F_LEFT) == 0 && !zero_fill) pad(s, fill, ' ');
    puts_n(s, prefix, plen);
    if (zero_fill) pad(s, fill, '0');
    puts_n(s, body, blen);
    pad(s, zeros, '0');
    puts_n(s, tail, tlen);
    if (flags & F_LEFT) pad(s, fill, ' ');
}

static int exponent_tail(char* tail, int upper, char letter, int x, int min_digits)
{
    int nt = 0;
    tail[nt++] = upper ? (char)(letter - 32) : letter;
    int ex = x < 0 ? -x : x;
    tail[nt++] = x < 0 ? '-' : '+';
    if (min_digits == 2 && ex < 10) tail[nt++] = '0';
    nt += utoa_base((unsigned int)ex, 10, 0, tail + nt);
    return nt;
}

// %a: [-]0x1.hhhhhhp+e, the exact bits in hexadecimal (0x0.hhhhhhp-126 for a subnormal). Without a precision the
// trailing zero digits are dropped; with one the mantissa is rounded to it, to nearest with ties to even.
static void fmt_hex_float(struct __sink* s, float v, const char* prefix_sign, int upper, int width, int prec, int flags)
{
    union fbits b; b.f = v;
    unsigned int biased = (b.u >> 23) & 255u;
    unsigned int frac = (b.u & 0x7FFFFFu) << 1;            // 24 bits: six hex digits
    unsigned int lead = biased != 0 ? 1u : 0u;
    int e = biased != 0 ? (int)biased - 127 : (frac != 0 ? -126 : 0);
    int digits = 6;
    if (prec >= 0 && prec < 6)
    {
        int drop = (6 - prec) * 4;
        unsigned int kept = frac >> drop;
        unsigned int rest = frac & ((1u << drop) - 1u);
        unsigned int half = 1u << (drop - 1);
        if (rest > half || (rest == half && (kept & 1u)))
            kept++;
        if (kept >> (prec * 4))                             // carried into the leading digit
        {
            kept &= (1u << (prec * 4)) - 1u;
            lead++;
        }
        frac = kept << drop;
        digits = prec;
    }
    else if (prec < 0)
    {
        while (digits > 0 && ((frac >> ((6 - digits) * 4)) & 15u) == 0)
            digits--;                                       // only as many as the value needs
    }
    const char* hex = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char prefix[4];
    int np = 0;
    if (prefix_sign[0]) prefix[np++] = prefix_sign[0];
    prefix[np++] = '0';
    prefix[np++] = upper ? 'X' : 'x';
    prefix[np] = 0;
    char body[12];
    int n = 0;
    body[n++] = hex[lead];
    if (digits > 0 || (flags & F_ALT)) body[n++] = '.';
    for (int i = 0; i < digits; i++)
        body[n++] = hex[(frac >> (20 - 4 * i)) & 15u];
    char tail[8];
    int nt = exponent_tail(tail, upper, 'p', e, 1);
    int zeros = prec > 6 ? prec - 6 : 0;
    put_number(s, prefix, body, n, zeros, tail, nt, width, flags);
}

static void fmt_float(struct __sink* s, float v, int conv, int width, int prec, int flags)
{
    int upper = (conv == 'F' || conv == 'E' || conv == 'G' || conv == 'A');
    if (put_special(s, v, upper, width, flags)) return;
    union fbits sb; sb.f = v;
    int neg = (int)(sb.u >> 31);
    if (neg) v = -v;
    char prefix[2]; prefix[0] = 0; prefix[1] = 0;
    if (neg) prefix[0] = '-'; else if (flags & F_PLUS) prefix[0] = '+'; else if (flags & F_SPACE) prefix[0] = ' ';
    int lc = conv | 32;                                // to lower case
    if (lc == 'a')
    {
        fmt_hex_float(s, v, prefix, upper, width, prec, flags);
        return;
    }
    if (prec < 0) prec = 6;
    char dg[FCONV_MAX_DIGITS];
    char body[FCONV_MAX_DIGITS + 64];
    int n = 0;
    int X = 0;
    int nd;

    if (lc == 'f')
    {
        nd = v != 0.0f ? __fconv_digits(v, 0, prec, dg, &X) : 0;
        if (nd == 0) X = 0;
        // The integer part: the digits down to 10^0, or a 0.
        if (nd == 0 || X < 0) body[n++] = '0';
        else for (int i = 0; i <= X; i++) body[n++] = i < nd ? dg[i] : '0';
        if (prec > 0 || (flags & F_ALT)) body[n++] = '.';
        // The fraction, as far as there are digits; zeros after that go out as a count.
        int j = 0;
        if (nd != 0)
            for (; j < prec; j++)
            {
                int at = X + 1 + j;
                if (at >= nd) break;
                body[n++] = at < 0 ? '0' : dg[at];
            }
        put_number(s, prefix, body, n, prec - j, "", 0, width, flags);
        return;
    }

    // e / g: P significant digits.
    int P = prec;
    if (lc == 'g') { if (P == 0) P = 1; }
    else P = prec + 1;
    if (v != 0.0f)
        nd = __fconv_digits(v, 1, P, dg, &X);
    else
    {
        dg[0] = '0';
        nd = 1;
        X = 0;
    }
    int use_exp = 1;
    if (lc == 'g') use_exp = !(X >= -4 && X < P);
    int sig = nd;                                      // digits we have; the rest of P are zeros
    if (lc == 'g' && !(flags & F_ALT))                 // strip trailing zeros of the significant digits
    {
        while (sig > 1 && dg[sig - 1] == '0') sig--;
        P = sig;
    }
    int zeros = P - sig;
    char tail[8];
    int nt = 0;
    if (use_exp)
    {
        body[n++] = dg[0];
        if (P > 1 || (flags & F_ALT)) body[n++] = '.';
        for (int i = 1; i < sig; i++) body[n++] = dg[i];
        nt = exponent_tail(tail, upper, 'e', X, 2);
    }
    else if (X >= 0)                                   // 123.456 : the point goes after X+1 digits (X < P here)
    {
        for (int i = 0; i <= X; i++) body[n++] = i < sig ? dg[i] : '0';
        if (P > X + 1 || (flags & F_ALT)) body[n++] = '.';
        for (int i = X + 1; i < sig; i++) body[n++] = dg[i];
        zeros = P - (sig > X + 1 ? sig : X + 1);       // the fraction digits past the real ones
    }
    else                                               // 0.000123
    {
        body[n++] = '0';
        body[n++] = '.';
        for (int i = 0; i < -X - 1; i++) body[n++] = '0';
        for (int i = 0; i < sig; i++) body[n++] = dg[i];
    }
    put_number(s, prefix, body, n, zeros, tail, nt, width, flags);
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
        int lenmod = 0;                                // 'H' = hh, 'h', 'W' = ll/j/q (64-bit), 0 = 32 bits
        for (;;)
        {
            c = fmt[i];
            if (c == 'h') { lenmod = (lenmod == 'h') ? 'H' : 'h'; }
            else if (c == 'l') { lenmod = (lenmod == 'l') ? 'W' : 'l'; }
            else if (c == 'j' || c == 'q') { lenmod = 'W'; }   // intmax_t is long long
            else if (c == 'z' || c == 't' || c == 'L') { }
            else break;
            i++;
        }
        c = fmt[i];
        if (c == 0) break;
        if (c == '%') { s->put(s, '%'); }
        else if (c == 'c' && lenmod == 'l')
        {
            // A wide character (wint_t) as UTF-8; one that is not a character is U+FFFD.
            char bytes[UTF8_MAX];
            int n = utf8_encode(va_arg(ap, unsigned int), bytes);
            if (n == 0)
                n = utf8_encode(UTF8_REPLACEMENT, bytes);
            put_field(s, "", bytes, n, 0, width, flags & ~F_ZERO, 0);
        }
        else if (c == 'c')
        {
            char ch[2]; ch[0] = (char)va_arg(ap, int); ch[1] = 0;
            put_field(s, "", ch, 1, 0, width, flags & ~F_ZERO, 0);
        }
        else if (c == 's' && lenmod == 'l')
        {
            // A wide string as UTF-8. The precision counts bytes, and a character that would not fit whole is
            // left out; the width counts bytes too, as it does for %s.
            const wchar_t* w = va_arg(ap, const wchar_t*);
            if (w == 0)
            {
                put_field(s, "", "(null)", prec >= 0 && prec < 6 ? prec : 6, 0, width, flags & ~F_ZERO, 0);
            }
            else
            {
                int bytes = 0, count = 0;
                for (; w[count] != 0; count++)
                {
                    int n = w[count] < 0 ? 0 : utf8_width((unsigned int)w[count]);
                    if (n == 0)
                        n = 3;                                   // U+FFFD
                    if (prec >= 0 && bytes + n > prec)
                        break;
                    bytes += n;
                }
                int fill = width > bytes ? width - bytes : 0;
                if ((flags & F_LEFT) == 0) pad(s, fill, ' ');
                for (int k = 0; k < count; k++)
                {
                    char unit[UTF8_MAX];
                    int n = w[k] < 0 ? 0 : utf8_encode((unsigned int)w[k], unit);
                    if (n == 0)
                        n = utf8_encode(UTF8_REPLACEMENT, unit);
                    puts_n(s, unit, n);
                }
                if (flags & F_LEFT) pad(s, fill, ' ');
            }
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
        else if (c == 'f' || c == 'F' || c == 'e' || c == 'E' || c == 'g' || c == 'G' || c == 'a' || c == 'A')
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
