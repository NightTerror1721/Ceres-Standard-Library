#pragma once
// The FILE structure and the helpers shared by file.c, format.c and scanf.c. NOT installed in include/:
// a program only ever sees `FILE*`.

#include "stddef.h"

#define FILE_TERM_IN    0    // stdin: the terminal's input, one waiting byte at a time
#define FILE_TERM_OUT   1    // stdout
#define FILE_TERM_ERR   2    // stderr (the same terminal; kept apart so the two can be told apart)
#define FILE_MEMORY     3    // fmemopen: a buffer in RAM

struct __file
{
    int kind;
    int readable;
    int writable;
    int eof;              // a read hit the end (never set on stdin: the terminal cannot signal one)
    int error;
    int unget;            // one pushed-back character, or -1
    int owned;            // the struct came from malloc: fclose frees it
    // memory streams
    unsigned char* mem;
    size_t size;          // the buffer's capacity
    size_t len;           // how much of it holds data (reads stop here)
    size_t pos;           // the read/write position
    int append;           // every write goes to the end
};

int __file_putc(struct __file* f, int c);        // one byte to any writable stream
int __file_is_terminal_out(struct __file* f);
