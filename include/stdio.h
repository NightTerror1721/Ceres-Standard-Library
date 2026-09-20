#pragma once

#include "stddef.h"
#include "stdarg.h"

// Console I/O over the terminal device (see ceres/terminal.h), formatted input and output, and streams
// (FILE) over the terminal and over memory. Files on the disk need a file system: until ceres/fs.h
// exists, fopen() fails with ENOSYS.

#define EOF          (-1)
#define BUFSIZ       256
#define FILENAME_MAX 64
#define FOPEN_MAX    8
#define SEEK_SET     0
#define SEEK_CUR     1
#define SEEK_END     2
#define _IOFBF       0
#define _IOLBF       1
#define _IONBF       2

struct __file;
typedef struct __file FILE;
typedef int fpos_t;

// Pointer VARIABLES, not objects: an `extern struct-of-unknown-size x;` cannot be declared in this subset.
extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

// ---- characters and strings on the console ----
int  putchar(int c);                    // write one byte; returns it
int  getchar(void);                     // WAITS for a byte and returns it; the same stream as fgetc(stdin)
int  getchar_nb(void);                  // the byte if one is buffered, else -1: the loop-friendly form
int  putstr(const char* s);             // write a NUL-terminated string; returns its length
int  puts(const char* s);               // putstr + newline

// The device has no end-of-input signal, so getchar() can never return EOF, and feof(stdin) is never
// true: at the end of a piped input a read waits forever. Use getchar_nb() when the program has other
// work to do, or give the user a way to say "done".

// ---- numbers, without the format engine (small and fast) ----
int  putint(int v);                     // decimal, with sign
int  putuint(unsigned int v);           // decimal, unsigned
int  puthex(unsigned int v);            // "0x" + minimal lowercase hex digits
int  putbin(unsigned int v, int bits);  // `bits` binary digits, most significant first
int  putfloat(float f, int decimals);   // like "%.*f": sign, integer, '.', `decimals` digits

// ---- formatted output (src/format.c) ----
// %d %i %u %o %x %X %b %c %s %p %n %f %F %e %E %g %G %%, with flags "- + space # 0", width and
// precision (a number or *), and the length modifiers hh h l ll z t j (all of them 32 bits at most).
// Returns the number of characters that were (or, for snprintf, would have been) produced.
int  printf(const char* fmt, ...);
int  vprintf(const char* fmt, va_list ap);
int  fprintf(FILE* f, const char* fmt, ...);
int  vfprintf(FILE* f, const char* fmt, va_list ap);
int  sprintf(char* buf, const char* fmt, ...);
int  vsprintf(char* buf, const char* fmt, va_list ap);
int  snprintf(char* buf, size_t n, const char* fmt, ...);   // always NUL-terminates when n > 0
int  vsnprintf(char* buf, size_t n, const char* fmt, va_list ap);

// ---- formatted input (src/scanf.c) ----
// %d %i %u %x %X %o %c %s %f %e %g %[set] %p %n %%, with a width, `*` to skip a conversion and the length
// modifiers hh h l ll z (%lf is a float: double IS float here). Returns how many conversions stored a
// value, or EOF when the input ended before the first one. Whitespace in the format matches any run of
// whitespace; any other character must match exactly, and the first mismatch stops the scan.
int  scanf(const char* fmt, ...);
int  vscanf(const char* fmt, va_list ap);
int  fscanf(FILE* f, const char* fmt, ...);
int  vfscanf(FILE* f, const char* fmt, va_list ap);
int  sscanf(const char* s, const char* fmt, ...);
int  vsscanf(const char* s, const char* fmt, va_list ap);

// ---- streams (src/file.c) ----
// stdout and stderr are unbuffered, so nothing is lost when the program stops and printf/fprintf/putchar
// always come out in order. stdin waits for each byte and has one character of pushback.
FILE* fopen(const char* path, const char* mode);          // NULL with ENOSYS until there is a file system
FILE* freopen(const char* path, const char* mode, FILE* f);   // ditto
FILE* fmemopen(void* buf, size_t size, const char* mode);     // a stream over `buf`; it never grows
int   fclose(FILE* f);
int   fflush(FILE* f);                                     // a no-op: nothing is held back
int   setvbuf(FILE* f, char* buf, int mode, size_t size);  // accepted and ignored
void  setbuf(FILE* f, char* buf);
FILE* tmpfile(void);                                       // NULL with ENOSYS
int   remove(const char* path);                            // -1 with ENOSYS
int   rename(const char* from, const char* to);            // -1 with ENOSYS

size_t fread(void* buf, size_t size, size_t n, FILE* f);
size_t fwrite(const void* buf, size_t size, size_t n, FILE* f);
int   fgetc(FILE* f);
int   getc(FILE* f);
int   fputc(int c, FILE* f);
int   putc(int c, FILE* f);
int   ungetc(int c, FILE* f);                              // one character of pushback
char* fgets(char* buf, int n, FILE* f);                    // through the newline, at most n-1 characters
int   fputs(const char* s, FILE* f);
int   getline(char** line, size_t* cap, FILE* f);          // POSIX: a malloc'd, growing buffer; -1 at EOF

int   fseek(FILE* f, int offset, int whence);              // memory streams only (ESPIPE on the terminal)
int   ftell(FILE* f);
void  rewind(FILE* f);
int   fgetpos(FILE* f, fpos_t* p);
int   fsetpos(FILE* f, const fpos_t* p);
int   feof(FILE* f);
int   ferror(FILE* f);
void  clearerr(FILE* f);
void  perror(const char* msg);                             // "msg: <strerror(errno)>" on stderr
