#include "ceres/sys.h"
#include "ceres/heap.h"
#include "ceres.h"
#include "stdio.h"

// The system-control device takes a command: 1 shuts the machine down, 2 resets it. A shutdown written as a word
// (or a halfword) carries the exit status in the second byte; written as a byte it is status 0.

// After a command the machine stops (or starts over) before the next instruction. Only a machine with
// no system-control device gets past the store, and there the halt, with interrupts masked, is the stop.
static void stop_here(void) __attribute__((__noreturn__));
static void stop_here(void)
{
    __builtin_cli();
    for (;;)
        __builtin_halt();
}

unsigned int sys_fault_address(void)
{
    return mmio_r32(SYS_CTRL_FAULT_ADDR);
}

unsigned int sys_fault_access(void)
{
    return mmio_r32(SYS_CTRL_FAULT_ACCESS);
}

unsigned int sys_stack_limit(void)
{
    return mmio_r32(SYS_CTRL_STACK_LIMIT);
}

void sys_set_stack_limit(unsigned int address)
{
    mmio_w32(SYS_CTRL_STACK_LIMIT, address);
}

void sys_exit(void)
{
    mmio_w32(SYS_CTRL_CMD, 1);
    stop_here();
}

void sys_exit_status(int status)
{
    mmio_w32(SYS_CTRL_CMD, ((unsigned int)status & 0xFFu) << 8 | 1u);
    stop_here();
}

unsigned int sys_memory_size(void)
{
    return mmio_r32(SYS_CTRL_MEM_SIZE);
}

unsigned int sys_features(void)
{
    return mmio_r32(SYS_CTRL_FEATURES);
}

void sys_set_features(unsigned int features)
{
    mmio_w32(SYS_CTRL_FEATURES, features);
}

void sys_reset(void)
{
    mmio_w32(SYS_CTRL_BASE, 2);
    stop_here();
}

void sys_panic(const char* msg)
{
    putstr("panic: ");
    putstr(msg);
    putchar('\n');
    sys_exit_status(134);                // abnormal termination, like abort()
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
