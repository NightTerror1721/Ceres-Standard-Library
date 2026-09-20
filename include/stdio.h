#pragma once

#include "stddef.h"
#include "stdarg.h"

// Console I/O over the terminal device (see ceres/terminal.h) and formatted output to memory.
// The FILE layer (fopen, fprintf, stdin/stdout/stderr, scanf...) comes with the file system.

#define EOF        (-1)
#define BUFSIZ     256

// ---- characters and strings ----
int  putchar(int c);                    // write one byte; returns it
int  getchar(void);                     // WAITS for a byte and returns it (spins on the terminal status)
int  getchar_nb(void);                  // the byte if one is buffered, else -1: the loop-friendly form
int  putstr(const char* s);             // write a NUL-terminated string; returns its length
int  puts(const char* s);               // putstr + newline

// The device has no end-of-input signal, so getchar() can never return EOF: at the end of a
// piped input it waits forever. Use getchar_nb() when the program has other work to do.

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
int  sprintf(char* buf, const char* fmt, ...);
int  vsprintf(char* buf, const char* fmt, va_list ap);
int  snprintf(char* buf, size_t n, const char* fmt, ...);   // always NUL-terminates when n > 0
int  vsnprintf(char* buf, size_t n, const char* fmt, va_list ap);
