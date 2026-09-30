// Logging and inspection. See ceres/debug.h.
#include "ceres/debug.h"
#include "ceres.h"
#include "ceres/heap.h"
#include "ceres/sys.h"
#include "ceres/timer.h"
#include "stdarg.h"

int __vformat_ext(void (*put)(void*, int), void* ctx, const char* fmt, va_list ap);   // format.c

// The debug log's registers (CeresASM docs/07-IO-Devices-and-Ports.md).
#define DEBUGLOG_OUTPUT   (DEBUGLOG_BASE + 0x00)   // W: one character of the line; '\n' sends it
#define DEBUGLOG_LEVEL    (DEBUGLOG_BASE + 0x04)   // RW: 0 error ... 3 debug, the level of the next line
#define DEBUGLOG_BREAK    (DEBUGLOG_BASE + 0x0C)   // W: stop here under the debugger
#define DEBUGLOG_ENABLED  (DEBUGLOG_BASE + 0x10)   // R: 1 when the host collects the log

static int level_now = LOG_INFO;

void log_set_level(int level) { level_now = level; }
int  log_level(void) { return level_now; }

static void log_put(void* ctx, int c)
{
    (void)ctx;
    mmio_w32(DEBUGLOG_OUTPUT, (unsigned char)c);
}

static void log_str(const char* s)
{
    while (*s)
        log_put(NULL, *s++);
}

// Lines from here on go at `level`, which the device takes from 0 (error) to 3 (debug).
static void log_at(int level)
{
    mmio_w32(DEBUGLOG_LEVEL, level < LOG_ERROR ? LOG_ERROR : level > LOG_DEBUG ? LOG_DEBUG : level);
}

static void log_printf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    __vformat_ext(log_put, NULL, fmt, ap);
    va_end(ap);
}

void log_msg(int level, const char* fmt, ...)
{
    if (level > level_now)
        return;
    log_at(level);
    va_list ap;
    va_start(ap, fmt);
    __vformat_ext(log_put, NULL, fmt, ap);
    va_end(ap);
    log_put(NULL, '\n');
}

void dbg_break(void)
{
    mmio_w32(DEBUGLOG_BREAK, 1u);
}

int dbg_log_enabled(void)
{
    return (int)mmio_r32(DEBUGLOG_ENABLED);
}

void dbg_hexdump_base(const void* p, size_t n, unsigned int shown_base)
{
    const unsigned char* s = (const unsigned char*)p;
    log_at(LOG_INFO);
    for (size_t row = 0; row < n; row += 16)
    {
        log_printf("%08x ", shown_base + (unsigned int)row);
        for (size_t i = 0; i < 16; i++)
        {
            if (i == 8)
                log_put(NULL, ' ');
            if (row + i < n)
                log_printf(" %02x", s[row + i]);
            else
                log_str("   ");
        }
        log_str("  |");
        for (size_t i = 0; i < 16 && row + i < n; i++)
        {
            unsigned char c = s[row + i];
            log_put(NULL, c >= 32 && c < 127 ? c : '.');
        }
        log_str("|\n");
    }
}

void dbg_hexdump(const void* p, size_t n)
{
    dbg_hexdump_base(p, n, (unsigned int)p);
}

void dbg_where(void)
{
    unsigned int sp = sys_sp();
    unsigned int heap_top = sys_heap_start() + heap_used();
    log_at(LOG_INFO);
    log_printf("sp=0x%08x heap_top=0x%08x stack_free=%u\n", sp, heap_top, sys_stack_free());
}

#define PAINT 0xA5A5A5A5u
#define PAINT_GAP 64u                       // leave the caller's own frame alone

static unsigned int paint_top = 0;          // where the stack stood at the paint
static unsigned int paint_low = 0;          // the lowest painted address

void dbg_stack_paint(unsigned int max_bytes)
{
    unsigned int sp = sys_sp();
    unsigned int room = sys_stack_free();
    if (room <= PAINT_GAP + 256u)
    {
        paint_top = paint_low = 0;
        return;
    }
    room -= PAINT_GAP + 256u;               // and stay clear of the heap
    if (max_bytes > room)
        max_bytes = room;
    max_bytes &= ~3u;
    paint_top = sp;
    paint_low = sp - PAINT_GAP - max_bytes;
    unsigned int* w = (unsigned int*)paint_low;
    for (unsigned int i = 0; i < max_bytes / 4; i++)
        w[i] = PAINT;
}

unsigned int dbg_stack_used(void)
{
    if (paint_top == 0)
        return 0;
    unsigned int edge = paint_top - PAINT_GAP;          // the top of the painted window
    unsigned int a = paint_low;
    while (a < edge && *(volatile unsigned int*)a == PAINT)
        a += 4;
    return edge - a;
}

void dbg_span_begin(struct dbg_span* s, const char* name)
{
    s->name = name;
    s->t0 = timer_ticks();
}

unsigned int dbg_span_elapsed(const struct dbg_span* s)
{
    return timer_elapsed(s->t0);
}

unsigned int dbg_span_end(struct dbg_span* s)
{
    unsigned int used = dbg_span_elapsed(s);
    log_at(LOG_INFO);
    log_printf("%s: %u cycles\n", s->name, used);
    return used;
}
