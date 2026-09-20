// fprintf and vfprintf, kept apart from the format engine so that a program that only uses printf does not
// link the FILE layer (and, through it, malloc and strerror).
#include "stdio.h"
#include "file_priv.h"

int __vformat_ext(void (*put)(void*, int), void* ctx, const char* fmt, va_list ap);   // format.c

static void put_to_file(void* ctx, int c)
{
    __file_putc((struct __file*)ctx, c);
}

int vfprintf(FILE* f, const char* fmt, va_list ap)
{
    if (f == 0 || !f->writable)
        return -1;
    if (__file_is_terminal_out(f))
        return vprintf(fmt, ap);                  // the terminal has its own chunked sink
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
