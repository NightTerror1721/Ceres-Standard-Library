// The FILE layer over the terminal, over memory (fmemopen) and - through the operations table a disk or host
// stream carries - over CeresFS files and the host's files. fopen() and friends are in fopen.c, so that a program
// that never opens a file does not link the file system.
//
// stdout and stderr are UNBUFFERED unless setvbuf says otherwise - every byte goes straight to the terminal - so
// printf, fprintf(stdout) and putchar can never come out of order, and nothing is lost if the program stops
// without flushing. stderr is the terminal's error stream (term_write_error): the host's stderr under `ceres run`.
// stdin has no buffer either, only a one-character pushback for ungetc; a read waits for a byte, or for the end
// of the input (the host closed stdin), which makes it return EOF and feof(stdin) true.
//
// A disk or host stream is fully buffered (FILE_BUFSIZ bytes, allocated at its first read or write): a byte at a
// time goes through the buffer, and the medium sees blocks. fread and fwrite of a block at least as large as the
// buffer go straight through. setvbuf chooses _IOFBF, _IOLBF or _IONBF and the buffer for any stream; a buffered
// stdout takes printf, puts and putchar with it (__file_stdout_changed). What a stream holds back is written out
// by fflush, fclose, a seek, a switch from writing to reading, and exit().
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"
#include "file_priv.h"
#include "ceres/terminal.h"

static struct __file stdin_file  = { .kind = FILE_TERM_IN,  .readable = 1, .unget = -1, .buf_mode = _IONBF };
static struct __file stdout_file = { .kind = FILE_TERM_OUT, .writable = 1, .unget = -1, .buf_mode = _IONBF };
static struct __file stderr_file = { .kind = FILE_TERM_ERR, .writable = 1, .unget = -1, .buf_mode = _IONBF };

void (*__file_flush_all_hook)(void) = 0;

FILE* stdin = &stdin_file;
FILE* stdout = &stdout_file;
FILE* stderr = &stderr_file;

int __file_is_terminal_out(struct __file* f)
{
    return f->kind == FILE_TERM_OUT || f->kind == FILE_TERM_ERR;
}

static int has_ops(struct __file* f)
{
    return f->kind == FILE_DISK || f->kind == FILE_HOST;
}

// ---- the list of streams exit() flushes ----

static struct __file* open_streams = 0;
static int exit_registered = 0;

static void flush_everything(void)
{
    fflush(0);
}

void __file_list(struct __file* f)
{
    if (f->listed)
        return;
    f->next_open = open_streams;
    open_streams = f;
    f->listed = 1;
    if (!exit_registered)
    {
        exit_registered = 1;
        atexit(flush_everything);
    }
}

void __file_forget(struct __file* f)
{
    if (!f->listed)
        return;
    for (struct __file** at = &open_streams; *at != 0; at = &(*at)->next_open)
    {
        if (*at == f)
        {
            *at = f->next_open;
            break;
        }
    }
    f->listed = 0;
    f->next_open = 0;
}

// ---- moving bytes, below the buffer ----

// Writes n bytes where the stream's bytes go: the terminal, its error stream, or the medium. Returns how many
// were written; fewer means an error (set on the stream).
static size_t raw_write(struct __file* f, const unsigned char* p, size_t n)
{
    if (f->kind == FILE_TERM_OUT || f->kind == FILE_TERM_ERR)
    {
        for (size_t left = n; left > 0;)
        {
            int part = left > 0x40000000u ? 0x40000000 : (int)left;   // term_write takes an int
            if (f->kind == FILE_TERM_ERR)
                term_write_error((const char*)p, part);
            else
                term_write((const char*)p, part);
            p += part;
            left -= (size_t)part;
        }
        return n;
    }
    size_t done = 0;
    while (done < n)
    {
        unsigned int part = n - done > 0x40000000u ? 0x40000000u : (unsigned int)(n - done);
        int wrote = f->ops->write(f, p + done, part);
        if (wrote <= 0)
        {
            f->error = 1;
            break;
        }
        done += (size_t)wrote;
    }
    return done;
}

int __file_flush(struct __file* f)
{
    if (f->buf_state != BUF_WRITING)
        return 0;
    size_t pending = f->buf_len;
    f->buf_state = BUF_EMPTY;
    f->buf_len = 0;
    if (pending != 0 && raw_write(f, f->buf, pending) != pending)
        return EOF;
    return 0;
}

// Gives back what was read ahead and not used: the medium's position goes back to where the program is.
static void drop_read_ahead(struct __file* f)
{
    if (f->buf_state != BUF_READING)
        return;
    int unread = (int)(f->buf_len - f->buf_pos);
    f->buf_state = BUF_EMPTY;
    f->buf_len = 0;
    f->buf_pos = 0;
    if (unread != 0 && has_ops(f))
        f->ops->seek(f, -unread, SEEK_CUR);
}

// The stream's buffer, allocated the first time it is needed; 0 for an unbuffered stream (or when there is no
// memory for one, which makes it unbuffered).
static unsigned char* buffer_of(struct __file* f)
{
    if (f->buf_mode == _IONBF)
        return 0;
    if (f->buf == 0)
    {
        if (f->buf_size == 0)
            f->buf_size = FILE_BUFSIZ;
        f->buf = (unsigned char*)malloc(f->buf_size);
        if (f->buf == 0)
        {
            f->buf_mode = _IONBF;
            return 0;
        }
        f->buf_owned = 1;
        __file_list(f);
    }
    return f->buf;
}

void __file_release_buffer(struct __file* f)
{
    __file_flush(f);
    drop_read_ahead(f);
    if (f->buf_owned)
        free(f->buf);
    f->buf = 0;
    f->buf_owned = 0;
    f->buf_size = 0;
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
    f->buf_mode = _IONBF;                               // the program's buffer is the only one
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
    int result = __file_flush(f) == 0 ? 0 : -1;
    drop_read_ahead(f);
    if (has_ops(f) && f->ops->close(f) != 0)
        result = -1;
    if (f->buf_owned)
        free(f->buf);
    f->buf = 0;
    f->buf_owned = 0;
    __file_forget(f);
    if (f->owned)
        free(f);
    return result == 0 ? 0 : EOF;                        // the three terminal streams stay open
}

int fflush(FILE* f)
{
    if (f == 0)
    {
        int result = 0;
        for (struct __file* s = open_streams; s != 0; s = s->next_open)
            if (__file_flush(s) != 0)
                result = EOF;
        if (__file_flush_all_hook != 0)
            __file_flush_all_hook();                     // every open disk file, to the disk
        return result;
    }
    int result = __file_flush(f);
    drop_read_ahead(f);
    if (has_ops(f) && f->ops->flush(f) != 0)
        result = EOF;
    return result;
}

int setvbuf(FILE* f, char* buf, int mode, size_t size)
{
    if (f == 0 || (mode != _IOFBF && mode != _IOLBF && mode != _IONBF) || (buf != 0 && size == 0))
    {
        errno = EINVAL;
        return -1;
    }
    if (f->kind == FILE_MEMORY || f->kind == FILE_TERM_IN)
        return 0;                                        // accepted: these never hold anything back
    __file_release_buffer(f);
    f->buf_mode = mode;
    if (mode != _IONBF)
    {
        f->buf = (unsigned char*)buf;                    // 0: allocated at the first write
        f->buf_size = buf != 0 ? size : size != 0 ? size : FILE_BUFSIZ;
        if (buf != 0)
            __file_list(f);
    }
    if (f == stdout)
        __file_stdout_changed();
    return 0;
}

void setbuf(FILE* f, char* buf)
{
    setvbuf(f, buf, buf != 0 ? _IOFBF : _IONBF, BUFSIZ);
}

// ---- stdout, as printf and putchar reach it ----

extern void (*__stdout_write_hook)(const char* s, int n);   // stdio.c

static void write_to_stdout(const char* s, int n)
{
    fwrite(s, 1, (size_t)n, stdout);
}

// printf, puts and putchar write to the terminal themselves, the fast way. Once stdout is buffered or reopened they
// go through it instead, so that everything written to stdout comes out in the order it was written.
void __file_stdout_changed(void)
{
    int plain = stdout->kind == FILE_TERM_OUT && stdout->buf_mode == _IONBF;
    __stdout_write_hook = plain ? 0 : write_to_stdout;
}

// ---- writing ----

static void memory_terminate(struct __file* f)
{
    if (f->len < f->size)
        f->mem[f->len] = 0;                              // w and a streams stay NUL-terminated while there is room
}

static int memory_putc(struct __file* f, int c)
{
    if (f->append)
        f->pos = f->len;
    if (f->pos >= f->size)
    {
        f->error = 1;                                    // the buffer is full
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

int __file_putc(struct __file* f, int c)
{
    if (f == 0 || !f->writable)
        return EOF;
    if (f->kind == FILE_MEMORY)
        return memory_putc(f, c);
    drop_read_ahead(f);
    unsigned char byte = (unsigned char)c;
    unsigned char* buf = buffer_of(f);
    if (buf == 0)
        return raw_write(f, &byte, 1) == 1 ? c & 0xFF : EOF;
    buf[f->buf_len++] = byte;
    f->buf_state = BUF_WRITING;
    if (f->buf_len == f->buf_size || (f->buf_mode == _IOLBF && byte == '\n'))
        if (__file_flush(f) != 0)
            return EOF;
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

// size * count as a byte count, or 0 with the stream's error set when the product does not fit: a wrapped
// product would move fewer bytes than asked and report a count that matches neither.
static size_t byte_total(FILE* f, size_t size, size_t count)
{
    if (size == 0)
        return 0;
    if (count > (size_t)-1 / size)
    {
        f->error = 1;
        errno = EOVERFLOW;
        return 0;
    }
    return size * count;
}

// n bytes to a stream that is not in memory: into its buffer, or straight out when it has none or the block is
// at least as large as the buffer. The bytes written.
static size_t write_bytes(struct __file* f, const unsigned char* p, size_t n)
{
    drop_read_ahead(f);
    unsigned char* buf = buffer_of(f);
    if (buf == 0)
        return raw_write(f, p, n);
    int failed_before = f->error;
    f->error = 0;
    size_t done = 0;
    while (done < n)
    {
        if (f->buf_len == 0 && n - done >= f->buf_size)
        {
            done += raw_write(f, p + done, n - done);   // a large block: no copy
            break;
        }
        size_t room = f->buf_size - f->buf_len;
        size_t part = n - done < room ? n - done : room;
        memcpy(buf + f->buf_len, p + done, part);
        f->buf_len += part;
        f->buf_state = BUF_WRITING;
        done += part;
        if (f->buf_len == f->buf_size && __file_flush(f) != 0)
            break;
    }
    if (f->buf_mode == _IOLBF && f->buf_state == BUF_WRITING && memchr(buf, '\n', f->buf_len) != 0)
        __file_flush(f);
    int failed = f->error;
    f->error = failed | failed_before;
    return failed ? 0 : done;                            // what reached the medium is not known: report none
}

int fputs(const char* s, FILE* f)
{
    if (f == 0 || !f->writable)
        return EOF;
    size_t n = strlen(s);
    if (f->kind == FILE_MEMORY)
    {
        for (size_t i = 0; i < n; i++)
            if (memory_putc(f, s[i]) == EOF)
                return EOF;
        return 1;
    }
    return write_bytes(f, (const unsigned char*)s, n) == n ? 1 : EOF;
}

size_t fwrite(const void* buf, size_t size, size_t count, FILE* f)
{
    if (f == 0 || !f->writable || size == 0 || count == 0)
        return 0;
    size_t total = byte_total(f, size, count);
    if (total == 0)
        return 0;
    const unsigned char* p = (const unsigned char*)buf;
    if (f->kind == FILE_MEMORY)
    {
        size_t done = 0;
        while (done < total && memory_putc(f, p[done]) != EOF)
            done++;
        return done / size;
    }
    return write_bytes(f, p, total) / size;
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
    if (stdin_file.kind != FILE_TERM_IN)
        return fgetc(stdin);                             // reopened on a file: it is all there already
    return terminal_byte(0);
}

// The next byte of a disk or host stream, through its buffer. -1 at the end or on an error.
static int ops_getc(struct __file* f)
{
    __file_flush(f);
    if (f->buf_state == BUF_READING && f->buf_pos < f->buf_len)
        return f->buf[f->buf_pos++];
    unsigned char* buf = buffer_of(f);
    if (buf == 0)
    {
        unsigned char c;
        int n = f->ops->read(f, &c, 1);
        if (n < 0)
            f->error = 1;
        return n == 1 ? (int)c : -1;
    }
    int n = f->ops->read(f, buf, (unsigned int)f->buf_size);
    if (n <= 0)
    {
        if (n < 0)
            f->error = 1;
        f->buf_state = BUF_EMPTY;
        f->buf_len = 0;
        f->buf_pos = 0;
        return -1;
    }
    f->buf_state = BUF_READING;
    f->buf_len = (size_t)n;
    f->buf_pos = 1;
    return buf[0];
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
    {
        int c = terminal_byte(1);                        // waits for a byte, or for the end of the input
        if (c < 0)
        {
            f->eof = 1;
            return EOF;
        }
        return c;
    }
    if (has_ops(f))
    {
        int c = ops_getc(f);
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

// n bytes from a disk or host stream: what the buffer holds first, then a block straight into `p` when what is
// left is at least a buffer's worth (or there is no buffer), else through the buffer.
static size_t ops_read(struct __file* f, unsigned char* p, size_t n)
{
    __file_flush(f);
    size_t done = 0;
    if (f->unget >= 0 && n > 0)
    {
        p[done++] = (unsigned char)f->unget;
        f->unget = -1;
    }
    while (done < n)
    {
        if (f->buf_state == BUF_READING && f->buf_pos < f->buf_len)
        {
            size_t have = f->buf_len - f->buf_pos;
            size_t part = n - done < have ? n - done : have;
            memcpy(p + done, f->buf + f->buf_pos, part);
            f->buf_pos += part;
            done += part;
            continue;
        }
        f->buf_state = BUF_EMPTY;
        f->buf_len = 0;
        f->buf_pos = 0;
        unsigned char* buf = buffer_of(f);
        if (buf == 0 || n - done >= f->buf_size)
        {
            unsigned int part = n - done > 0x40000000u ? 0x40000000u : (unsigned int)(n - done);
            int got = f->ops->read(f, p + done, part);
            if (got <= 0)
            {
                if (got < 0) f->error = 1;
                else f->eof = 1;
                break;
            }
            done += (size_t)got;
            continue;
        }
        int got = f->ops->read(f, buf, (unsigned int)f->buf_size);
        if (got <= 0)
        {
            if (got < 0) f->error = 1;
            else f->eof = 1;
            break;
        }
        f->buf_state = BUF_READING;
        f->buf_len = (size_t)got;
    }
    return done;
}

size_t fread(void* buf, size_t size, size_t count, FILE* f)
{
    if (f == 0 || !f->readable || size == 0 || count == 0)
        return 0;
    size_t total = byte_total(f, size, count);
    unsigned char* p = (unsigned char*)buf;
    if (has_ops(f))
        return ops_read(f, p, total) / size;
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
    if (f != 0 && has_ops(f))
    {
        if (__file_flush(f) != 0)
            return -1;
        if (whence == SEEK_CUR)
        {
            if (f->unget >= 0)
                offset--;                                // the pushed-back character has been read already
            if (f->buf_state == BUF_READING)
                offset -= (int)(f->buf_len - f->buf_pos);   // and the medium is ahead by the read-ahead
        }
        f->unget = -1;
        f->buf_state = BUF_EMPTY;
        f->buf_len = 0;
        f->buf_pos = 0;
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
    if (f != 0 && has_ops(f))
    {
        int at = f->ops->tell(f);
        if (at < 0)
            return -1;
        if (f->buf_state == BUF_READING)
            at -= (int)(f->buf_len - f->buf_pos);
        else if (f->buf_state == BUF_WRITING)
            at += (int)f->buf_len;
        return at - (f->unget >= 0 ? 1 : 0);
    }
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
