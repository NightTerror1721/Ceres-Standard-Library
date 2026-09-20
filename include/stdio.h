#pragma once

// Character and number I/O over the terminal device (see ceres/terminal.h).
// Counts and sizes are `int`: the VM is 32-bit and this C subset has no size_t.

int  putchar(int c);                    // write one byte; returns it
int  getchar(void);                     // read one byte, or -1 if none is buffered
int  putstr(const char* s);             // write a NUL-terminated string; returns its length
int  puts(const char* s);               // putstr + newline

int  putint(int v);                     // decimal, with sign
int  putuint(unsigned int v);           // decimal, unsigned
int  puthex(unsigned int v);            // "0x" + minimal lowercase hex digits
int  putbin(unsigned int v, int bits);  // `bits` binary digits, most significant first
int  putfloat(float f, int decimals);   // fixed-point: sign, integer, '.', `decimals` digits

// %d/%i %u %x/%X %c %s %f %b %%  (plus %p as an alias for %x). The variadic
// builtins come from the compiler - there is no <stdarg.h> in this subset.
int  printf(const char* fmt, ...);
int  vprintf(const char* fmt, __builtin_va_list ap);
