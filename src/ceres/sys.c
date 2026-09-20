#include "ceres/sys.h"
#include "ceres/heap.h"
#include "ceres.h"
#include "stdio.h"

// The system-control device takes a command BYTE: 1 shuts the machine down, 2 resets it.

void sys_exit(void)
{
    mmio_w8(SYS_CTRL_BASE, 1);
}

void sys_reset(void)
{
    mmio_w8(SYS_CTRL_BASE, 2);
}

void sys_panic(const char* msg)
{
    putstr("panic: ");
    putstr(msg);
    putchar('\n');
    sys_exit();
}

unsigned int sys_stack_free(void)
{
    struct heap_stats s;
    heap_stats(&s);
    unsigned int sp = sys_sp();
    return sp > s.brk ? sp - s.brk : 0;
}

static void print_range(const char* name, unsigned int from, unsigned int to)
{
    putstr("  ");
    putstr(name);
    putstr(" ");
    puthex(from);
    putstr(" .. ");
    puthex(to);
    putstr("  ");
    putuint(to - from);
    putstr(" bytes\n");
}

void sys_print_layout(void)
{
    struct sys_layout l;
    sys_get_layout(&l);
    putstr("memory layout\n");
    print_range(".text  ", l.text_start, l.text_end);
    print_range(".rodata", l.rodata_start, l.rodata_end);
    print_range(".data  ", l.data_start, l.data_end);
    print_range(".bss   ", l.bss_start, l.bss_end);
    putstr("  heap    "); puthex(l.heap_start); putstr(" ..\n");
    putstr("  sp      "); puthex(l.sp); putchar('\n');
}
