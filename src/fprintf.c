// fprintf and vfprintf, kept apart from the format engine so that a program that only uses printf does not
// link the FILE layer (and, through it, malloc and strerror).
#include "stdio.h"
#include "file_priv.h"

int __vformat_ext(void (*put)(void*, int), void* ctx, const char* fmt, va_list ap);   // format.c

static void put_to_file(void* ctx, int c)
{
    __file_putc((struct __file*)ctx, c);
}

// The terminal's error stream is unbuffered, like stdout: its bytes are gathered here and go out in blocks, not
// one device write each.
struct chunked
{
    FILE* f;
    int n;
    char buf[64];
};

static void put_chunked(void* ctx, int c)
{
    struct chunked* k = (struct chunked*)ctx;
    if (k->n == (int)sizeof k->buf)
    {
        fwrite(k->buf, 1, (size_t)k->n, k->f);
        k->n = 0;
    }
    k->buf[k->n++] = (char)c;
}

int vfprintf(FILE* f, const char* fmt, va_list ap)
{
    if (f == 0 || !f->writable)
        return -1;
    if (f->kind == FILE_TERM_OUT)
        return vprintf(fmt, ap);                  // the terminal has its own chunked sink
    if (f->kind == FILE_TERM_ERR && f->buf_mode == _IONBF)
    {
        struct chunked k;
        k.f = f;
        k.n = 0;
        int r = __vformat_ext(put_chunked, &k, fmt, ap);
        if (k.n != 0)
            fwrite(k.buf, 1, (size_t)k.n, f);
        return r;
    }
    return __vformat_ext(put_to_file, f, fmt, ap);
}

int fprintf(FILE* f, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(f, fmt, ap);
    va_end(ap);
    return r;
}
