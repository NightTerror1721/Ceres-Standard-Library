#pragma once
// The FILE structure and the helpers shared by file.c, fopen.c, fprintf.c and scanf.c. NOT installed in include/:
// a program only ever sees `FILE*`.

#include "stddef.h"

#define FILE_TERM_IN    0    // stdin: the terminal's input, one waiting byte at a time
#define FILE_TERM_OUT   1    // stdout
#define FILE_TERM_ERR   2    // stderr: the terminal's error stream (the host's stderr under `ceres run`)
#define FILE_MEMORY     3    // fmemopen: a buffer in RAM
#define FILE_DISK       4    // fopen: a CeresFS file, reached through the operations in `ops`
#define FILE_HOST       5    // fopen("host:..."): a host file (ceres/hostfs.h), through `ops` as well

#define FILE_BUFSIZ     512  // the buffer a disk or host stream gets unless setvbuf says otherwise: a sector

struct __file;

// What a disk or host stream does, filled in by src/fopen.c. Kept out of file.c so that a program that only prints
// never links the file system. Everything moves in blocks: a byte at a time goes through the stream's buffer.
struct __file_ops
{
    int (*read)(struct __file* f, void* buf, unsigned int n);          // bytes read, 0 at the end, -1 on an error
    int (*write)(struct __file* f, const void* buf, unsigned int n);   // bytes written (fewer when full), -1 on an error
    int (*seek)(struct __file* f, int offset, int whence);             // 0, or -1 with errno set
    int (*tell)(struct __file* f);
    int (*close)(struct __file* f);
    int (*flush)(struct __file* f);                                     // what the medium still holds back, out
};

struct __file
{
    int kind;
    int readable;
    int writable;
    int eof;              // a read hit the end (on stdin: the host closed the input and it was all read)
    int error;
    int unget;            // one pushed-back character, or -1
    int owned;            // the struct came from malloc: fclose frees it
    // memory streams
    unsigned char* mem;
    size_t size;          // the buffer's capacity
    size_t len;           // how much of it holds data (reads stop here)
    size_t pos;           // the read/write position
    int append;           // every write goes to the end
    // disk and host streams
    const struct __file_ops* ops;
    int fd;               // the CeresFS descriptor, or the host handle
    int temporary;        // tmpfile(): the file is removed on close
    char name[24];        // ... and this is its name
    // the buffer (setvbuf): what a disk or host stream reads ahead or holds back, and what a buffered stdout or
    // stderr holds back
    unsigned char* buf;   // 0 until the first I/O needs it
    size_t buf_size;
    size_t buf_len;       // read-ahead bytes, or bytes not yet written
    size_t buf_pos;       // the next read-ahead byte
    int buf_mode;         // _IOFBF, _IOLBF or _IONBF
    int buf_state;        // 0 empty, 1 holds read-ahead, 2 holds bytes to write
    int buf_owned;        // malloc'd here, freed on close
    struct __file* next_open;   // the streams fflush(NULL) and exit flush
    int listed;           // on that list
};

#define BUF_EMPTY    0
#define BUF_READING  1
#define BUF_WRITING  2

// Set by the first fopen() of a disk file to the function that writes CeresFS out (fs_sync). fflush(NULL) calls it
// after flushing every stream's buffer, and so does exit().
extern void (*__file_flush_all_hook)(void);

int __file_parse_mode(const char* mode, int* readable, int* writable, int* truncate, int* append);   // "r" "w" "a" + optional "+" and "b"; 0 ok
int __file_putc(struct __file* f, int c);        // one byte to any writable stream
int __file_is_terminal_out(struct __file* f);    // stdout or stderr, still on the terminal
void __file_list(struct __file* f);              // put f on the list of streams exit and fflush(NULL) flush
void __file_forget(struct __file* f);            // and take it off
int __file_flush(struct __file* f);              // write out what f holds back; 0, or EOF with f->error set
void __file_release_buffer(struct __file* f);    // flush and free a buffer the library allocated
void __file_stdout_changed(void);                // stdout was buffered or reopened: printf and putchar follow it
