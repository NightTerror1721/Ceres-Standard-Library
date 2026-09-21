// The FILE layer over the terminal, over memory (fmemopen) and - through the operations table a disk
// stream carries - over CeresFS files. fopen() and friends are in fopen.c, so that a program that never
// opens a file does not link the file system.
//
// stdout and stderr are UNBUFFERED - every byte goes straight to the terminal - so printf, fprintf(stdout)
// and putchar can never come out of order, and nothing is lost if the program stops without flushing.
// stdin has no buffer either, only a one-character pushback for ungetc; a read waits for a byte, and the
// terminal cannot report the end of its input, so feof(stdin) is never true.
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"
#include "file_priv.h"
#include "ceres/terminal.h"

static struct __file stdin_file  = { FILE_TERM_IN,  1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0 };
static struct __file stdout_file = { FILE_TERM_OUT, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0 };
static struct __file stderr_file = { FILE_TERM_ERR, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0 };

FILE* stdin = &stdin_file;
FILE* stdout = &stdout_file;
FILE* stderr = &stderr_file;

int __file_is_terminal_out(struct __file* f)
{
    return f->kind == FILE_TERM_OUT || f->kind == FILE_TERM_ERR;
}

// ---- opening and closing ----

// Parses "r", "w", "a", each with an optional '+' and 'b' in either order after the letter.
int __file_parse_mode(const char* mode, int* readable, int* writable, int* truncate, int* append)
{
    if (mode == 0)
        return -1;
    int plus = 0;
    for (int i = 1; mode[i] != 0; i++)
    {
        if (mode[i] == '+') plus = 1;
        else if (mode[i] != 'b') return -1;
    }
    *truncate = 0;
    *append = 0;
    if (mode[0] == 'r')      { *readable = 1; *writable = plus; }
    else if (mode[0] == 'w') { *readable = plus; *writable = 1; *truncate = 1; }
    else if (mode[0] == 'a') { *readable = plus; *writable = 1; *append = 1; }
    else return -1;
    return 0;
}

// A stream over a buffer the program owns. "r": the whole buffer is readable. "w": it starts empty
// (and its first byte becomes 0). "a": it holds the text up to its first NUL and writes go after that.
// Writing never grows the buffer: past `size` the write is short and the stream is in error.
FILE* fmemopen(void* buf, size_t size, const char* mode)
{
    int readable, writable, truncate, append;
    if (buf == 0 || size == 0 || __file_parse_mode(mode, &readable, &writable, &truncate, &append) != 0)
    {
        errno = EINVAL;
        return 0;
    }
    struct __file* f = (struct __file*)malloc(sizeof(struct __file));
    if (f == 0)
    {
        errno = ENOMEM;
        return 0;
    }
    memset(f, 0, sizeof(struct __file));
    f->kind = FILE_MEMORY;
    f->readable = readable;
    f->writable = writable;
    f->unget = -1;
    f->owned = 1;
    f->mem = (unsigned char*)buf;
    f->size = size;
    f->append = append;
    if (truncate)
    {
        f->mem[0] = 0;
        f->len = 0;
    }
    else if (append)
    {
        f->len = strnlen((const char*)f->mem, size);
        f->pos = f->len;
    }
    else
    {
        f->len = size;
    }
    return f;
}

int fclose(FILE* f)
{
    if (f == 0)
        return EOF;
    int result = 0;
    if (f->kind == FILE_DISK)
        result = f->ops->close(f);
    if (f->owned)
        free(f);
    return result == 0 ? 0 : EOF;                        // the three terminal streams stay open
}

int fflush(FILE* f)
{
    if (f != 0 && f->kind == FILE_DISK)
        return f->ops->flush(f);                         // to the disk
    return 0;                                            // nothing else is ever held back
}

int setvbuf(FILE* f, char* buf, int mode, size_t size)
{
    return 0;                                            // accepted, and there is nothing to change
}

void setbuf(FILE* f, char* buf)
{
}

// ---- writing ----

static void memory_terminate(struct __file* f)
{
    if (f->len < f->size)
        f->mem[f->len] = 0;                              // w and a streams stay NUL-terminated while there is room
}

int __file_putc(struct __file* f, int c)
{
    if (f == 0 || !f->writable)
        return EOF;
    if (f->kind == FILE_MEMORY)
    {
        if (f->append)
            f->pos = f->len;
        if (f->pos >= f->size)
        {
            f->error = 1;                                // the buffer is full
            return EOF;
        }
        f->mem[f->pos] = (unsigned char)c;
        f->pos++;
        if (f->pos > f->len)
        {
            f->len = f->pos;
            memory_terminate(f);
        }
        return c & 0xFF;
    }
    if (f->kind == FILE_DISK)
    {
        if (f->ops->putc(f, c) < 0)
        {
            f->error = 1;
            return EOF;
        }
        return c & 0xFF;
    }
    term_write_char(c);
    return c & 0xFF;
}

int fputc(int c, FILE* f)
{
    return __file_putc(f, c);
}

int putc(int c, FILE* f)
{
    return __file_putc(f, c);
}

int fputs(const char* s, FILE* f)
{
    if (f == 0 || !f->writable)
        return EOF;
    if (__file_is_terminal_out(f))
    {
        putstr(s);
        return 1;
    }
    for (int i = 0; s[i] != 0; i++)
        if (__file_putc(f, s[i]) == EOF)
            return EOF;
    return 1;
}

size_t fwrite(const void* buf, size_t size, size_t count, FILE* f)
{
    if (f == 0 || !f->writable || size == 0 || count == 0)
        return 0;
    size_t total = size * count;
    const unsigned char* p = (const unsigned char*)buf;
    if (__file_is_terminal_out(f))
    {
        term_write((const char*)p, (int)total);
        return count;
    }
    size_t done = 0;
    while (done < total && __file_putc(f, p[done]) != EOF)
        done++;
    return done / size;
}

// ---- reading ----

// A byte from stdin if one is waiting (block == 0) or as soon as one arrives (block != 0).
static int terminal_byte(int block)
{
    return term_read_char(block ? TERM_READ_UNTIL_STATUS : TERM_READ_NON_BLOCKING);
}

int __stdin_getc_nb(void)
{
    if (stdin_file.unget >= 0)
    {
        int c = stdin_file.unget;
        stdin_file.unget = -1;
        return c;
    }
    return terminal_byte(0);
}

int fgetc(FILE* f)
{
    if (f == 0 || !f->readable)
        return EOF;
    if (f->unget >= 0)
    {
        int c = f->unget;
        f->unget = -1;
        return c;
    }
    if (f->kind == FILE_TERM_IN)
        return terminal_byte(1);                         // waits; the terminal never says "end"
    if (f->kind == FILE_DISK)
    {
        int c = f->ops->getc(f);
        if (c < 0)
        {
            f->eof = 1;
            return EOF;
        }
        return c;
    }
    if (f->pos >= f->len)
    {
        f->eof = 1;
        return EOF;
    }
    int c = f->mem[f->pos];
    f->pos++;
    return c;
}

int getc(FILE* f)
{
    return fgetc(f);
}

int ungetc(int c, FILE* f)
{
    if (f == 0 || !f->readable || c == EOF || f->unget >= 0)
        return EOF;                                      // one character of pushback, and EOF cannot be pushed
    f->unget = c & 0xFF;
    f->eof = 0;
    return c & 0xFF;
}

char* fgets(char* buf, int n, FILE* f)
{
    if (buf == 0 || n < 1 || f == 0 || !f->readable)
        return 0;
    int i = 0;
    while (i < n - 1)
    {
        int c = fgetc(f);
        if (c == EOF)
            break;
        buf[i] = (char)c;
        i++;
        if (c == '\n')
            break;
    }
    buf[i] = 0;
    return i == 0 ? 0 : buf;                             // nothing read: EOF, and the buffer holds just a NUL
}

size_t fread(void* buf, size_t size, size_t count, FILE* f)
{
    if (f == 0 || !f->readable || size == 0 || count == 0)
        return 0;
    size_t total = size * count;
    unsigned char* p = (unsigned char*)buf;
    size_t done = 0;
    while (done < total)
    {
        int c = fgetc(f);
        if (c == EOF)
            break;
        p[done] = (unsigned char)c;
        done++;
    }
    return done / size;
}

// POSIX getline: reads through the newline into a malloc'd buffer that grows as needed.
int getline(char** line, size_t* cap, FILE* f)
{
    if (line == 0 || cap == 0 || f == 0)
    {
        errno = EINVAL;
        return -1;
    }
    if (*line == 0 || *cap == 0)
    {
        *cap = 64;
        *line = (char*)malloc(*cap);
        if (*line == 0)
        {
            errno = ENOMEM;
            return -1;
        }
    }
    size_t n = 0;
    for (;;)
    {
        int c = fgetc(f);
        if (c == EOF)
            break;
        if (n + 2 > *cap)
        {
            size_t bigger = *cap * 2;
            char* grown = (char*)realloc(*line, bigger);
            if (grown == 0)
            {
                errno = ENOMEM;
                return -1;
            }
            *line = grown;
            *cap = bigger;
        }
        (*line)[n] = (char)c;
        n++;
        if (c == '\n')
            break;
    }
    if (n == 0)
        return -1;                                       // EOF before anything
    (*line)[n] = 0;
    return (int)n;
}

// ---- position and state ----

int fseek(FILE* f, int offset, int whence)
{
    if (f != 0 && f->kind == FILE_DISK)
    {
        if (whence == SEEK_CUR && f->unget >= 0)
            offset--;                                    // the pushed-back character has been read already
        f->unget = -1;
        if (f->ops->seek(f, offset, whence) != 0)
            return -1;
        f->eof = 0;
        return 0;
    }
    if (f == 0 || f->kind != FILE_MEMORY)
    {
        errno = ESPIPE;                                  // a terminal cannot be sought
        return -1;
    }
    long base;
    if (whence == SEEK_SET) base = 0;
    else if (whence == SEEK_CUR) base = (long)f->pos;
    else if (whence == SEEK_END) base = (long)f->len;
    else
    {
        errno = EINVAL;
        return -1;
    }
    long target = base + offset;
    if (target < 0 || (size_t)target > f->size)
    {
        errno = EINVAL;
        return -1;
    }
    f->pos = (size_t)target;
    f->unget = -1;
    f->eof = 0;
    return 0;
}

int ftell(FILE* f)
{
    if (f != 0 && f->kind == FILE_DISK)
        return f->ops->tell(f) - (f->unget >= 0 ? 1 : 0);
    if (f == 0 || f->kind != FILE_MEMORY)
    {
        errno = ESPIPE;
        return -1;
    }
    return (int)f->pos - (f->unget >= 0 ? 1 : 0);
}

void rewind(FILE* f)
{
    fseek(f, 0, SEEK_SET);
    if (f != 0)
        f->error = 0;
}

int fgetpos(FILE* f, fpos_t* p)
{
    int at = ftell(f);
    if (at < 0)
        return -1;
    *p = at;
    return 0;
}

int fsetpos(FILE* f, const fpos_t* p)
{
    return fseek(f, *p, SEEK_SET);
}

int feof(FILE* f)     { return f != 0 && f->eof; }
int ferror(FILE* f)   { return f != 0 && f->error; }

void clearerr(FILE* f)
{
    if (f != 0)
    {
        f->eof = 0;
        f->error = 0;
    }
}

void perror(const char* msg)
{
    if (msg != 0 && msg[0] != 0)
    {
        fputs(msg, stderr);
        fputs(": ", stderr);
    }
    fputs(strerror(errno), stderr);
    fputc('\n', stderr);
}
