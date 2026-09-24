#pragma once

#include "../stddef.h"

// Logging and inspection for programs under development. Everything goes to the terminal through
// printf, so it interleaves correctly with the program's own output.

#define LOG_ERROR 0
#define LOG_WARN  1
#define LOG_INFO  2
#define LOG_DEBUG 3

void log_set_level(int level);                       // messages above this level are dropped; the default is LOG_INFO
int  log_level(void);
void log_msg(int level, const char* fmt, ...) __attribute__((__format__(__printf__, 2, 3)));   // "[warn] text\n"

#define LOGE(...) log_msg(LOG_ERROR, __VA_ARGS__)
#define LOGW(...) log_msg(LOG_WARN,  __VA_ARGS__)
#define LOGI(...) log_msg(LOG_INFO,  __VA_ARGS__)
#define LOGD(...) log_msg(LOG_DEBUG, __VA_ARGS__)

// Sixteen bytes a line: the address, the bytes in hex, and the printable ones as text.
//   00000400  48 65 6c 6c 6f 00 01 02  03 04 05 06 07 08 09 0a  |Hello...........|
void dbg_hexdump(const void* p, size_t n);
void dbg_hexdump_base(const void* p, size_t n, unsigned int shown_base);   // the same, labelling the first byte with shown_base

// One line saying where the program stands: stack pointer, top of the heap and the room between them.
void dbg_where(void);

// How much stack a piece of code really uses. dbg_stack_paint() fills up to `max_bytes` of the unused
// stack below the caller with a known pattern (leaving 64 bytes untouched under the caller's frame);
// dbg_stack_used() reports how far into that window the stack has reached since - 0 if it has not. A
// high-water mark, so it only ever grows until the next paint. If the heap grows into the window the
// figure is an over-estimate.
void dbg_stack_paint(unsigned int max_bytes);
unsigned int dbg_stack_used(void);

// Work, measured in instructions rather than time: the VM is deterministic, so the number is repeatable.
struct dbg_span { unsigned int t0; const char* name; };
void dbg_span_begin(struct dbg_span* s, const char* name);
unsigned int dbg_span_elapsed(const struct dbg_span* s);   // instructions since dbg_span_begin, without printing
unsigned int dbg_span_end(struct dbg_span* s);       // prints "name: N instructions" and returns N
