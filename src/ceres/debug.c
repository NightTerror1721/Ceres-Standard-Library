// Logging and inspection. See ceres/debug.h.
#include "ceres/debug.h"
#include "ceres/heap.h"
#include "ceres/sys.h"
#include "ceres/timer.h"
#include "stdio.h"
#include "stdarg.h"

static int level_now = LOG_INFO;

void log_set_level(int level) { level_now = level; }
int  log_level(void) { return level_now; }

void log_msg(int level, const char* fmt, ...)
{
    if (level > level_now)
        return;
    const char* tag = "debug";
    if (level <= LOG_ERROR) tag = "error";
    else if (level == LOG_WARN) tag = "warn";
    else if (level == LOG_INFO) tag = "info";
    printf("[%s] ", tag);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
}

void dbg_hexdump_base(const void* p, size_t n, unsigned int shown_base)
{
    const unsigned char* s = (const unsigned char*)p;
    for (size_t row = 0; row < n; row += 16)
    {
        printf("%08x ", shown_base + (unsigned int)row);
        for (size_t i = 0; i < 16; i++)
        {
            if (i == 8)
                putchar(' ');
            if (row + i < n)
                printf(" %02x", s[row + i]);
            else
                putstr("   ");
        }
        putstr("  |");
        for (size_t i = 0; i < 16 && row + i < n; i++)
        {
            unsigned char c = s[row + i];
            putchar(c >= 32 && c < 127 ? c : '.');
        }
        putstr("|\n");
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
    printf("sp=0x%08x heap_top=0x%08x stack_free=%u\n", sp, heap_top, sys_stack_free());
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
    printf("%s: %u instructions\n", s->name, used);
    return used;
}
