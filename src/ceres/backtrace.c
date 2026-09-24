// The call stack and the symbol table. See ceres/backtrace.h. Nothing here uses printf or the heap: a fault
// report calls it on the system stack.
#include "ceres/backtrace.h"
#include "ceres/sys.h"
#include "ceres.h"
#include "stdio.h"

// The table ceres link --symtab appends to .rodata (CeresASM de96a5f), or two equal addresses when there is
// none: u32 count; count pairs of { address, name offset from the start }; the names. Its bounds come from
// asm/backtrace.casm.
unsigned int __backtrace_symtab(void);
unsigned int __backtrace_symtab_end(void);

#define SYSTEM_STACK ((unsigned int)CERES_SYSTEM_STACK)   // what the machine keeps at the top for handlers

// The program's own stack: from its limit up to where the machine started it.
static int in_stack(unsigned int fp)
{
    unsigned int top = sys_memory_size() - SYSTEM_STACK;
    unsigned int bottom = sys_stack_limit();
    if (bottom == 0xFFFFFFFFu)
        bottom = sys_heap_start();                        // a machine without the register
    return (fp & 3u) == 0 && fp >= bottom && fp <= top && top - fp >= 8u;   // no fp + 8: near 2^32 it wraps
}

unsigned int __backtrace_text_start(void);
unsigned int __backtrace_text_end(void);

#define OP_CALL  0x61u                                    // call simm24 (CeresASM opcodes.h)
#define OP_CALLR 0x62u                                    // call rs

// A frame is [fp] = the caller's fp, then the callee-saved registers the function pushed before its `enter`
// (Ceres-C saves r8-r11 that way from -O1 on), then the return address `call` pushed. How many registers
// were pushed differs from one function to the next, so the return address is found rather than assumed:
// the first word above [fp] that is an address in .text right after a `call` instruction, and below the
// caller's frame.
static unsigned int return_address(unsigned int fp, unsigned int caller)
{
    unsigned int text_start = __backtrace_text_start();
    unsigned int text_end = __backtrace_text_end();
    for (unsigned int at = fp + 4u; at < caller && at < fp + 4u + 4u * 16u; at += 4u)
    {
        unsigned int word = *(const unsigned int*)at;
        if ((word & 3u) != 0 || word < text_start + 4u || word >= text_end)   // text_end is one past the end
            continue;
        unsigned int opcode = *(const unsigned int*)(word - 4u) >> 24;
        if (opcode == OP_CALL || opcode == OP_CALLR)
            return word;
    }
    return 0;
}

int backtrace_from(unsigned int fp, unsigned int* pcs, int max)
{
    int n = 0;
    while (n < max && in_stack(fp))
    {
        unsigned int caller = *(const unsigned int*)fp;
        unsigned int limit = caller > fp && in_stack(caller) ? caller : sys_memory_size() - SYSTEM_STACK;
        unsigned int pc = return_address(fp, limit);
        if (pc == 0)
            break;
        pcs[n++] = pc;
        if (caller <= fp)
            break;                                        // frames go up the stack; anything else is not a chain
        fp = caller;
    }
    return n;
}

// The chain starts at this function's own frame, so its first return address is in the function that called
// it. The result goes through a volatile so the call is not a tail call: one would leave this function
// frameless at -O1 and up, and the chain would start a caller further out.
int backtrace(unsigned int* pcs, int max)
{
    volatile int n = backtrace_from(__backtrace_fp(), pcs, max);
    return n;
}

int backtrace_has_symbols(void)
{
    return __backtrace_symtab() < __backtrace_symtab_end();
}

const char* backtrace_symbol(unsigned int pc, unsigned int* offset)
{
    const unsigned int* table = (const unsigned int*)__backtrace_symtab();
    if ((unsigned int)table >= __backtrace_symtab_end())
        return 0;                                         // linked without --symtab
    unsigned int count = table[0];
    if (count == 0 || pc < table[1])
        return 0;
    // The last entry at or below pc: the entries are sorted by address.
    unsigned int lo = 0, hi = count;
    while (hi - lo > 1)
    {
        unsigned int mid = lo + (hi - lo) / 2;
        if (table[1 + 2 * mid] <= pc)
            lo = mid;
        else
            hi = mid;
    }
    if (offset != 0)
        *offset = pc - table[1 + 2 * lo];
    return (const char*)table + table[2 + 2 * lo];
}

void backtrace_print_address(unsigned int pc)
{
    unsigned int offset = 0;
    const char* name = backtrace_symbol(pc, &offset);
    if (name == 0)
    {
        puthex(pc);
        return;
    }
    putstr(name);
    if (offset != 0)
    {
        putstr("+");
        puthex(offset);
    }
}

void backtrace_print_from(unsigned int fp)
{
    unsigned int pcs[16];
    int n = backtrace_from(fp, pcs, 16);
    for (int i = 0; i < n; i++)
    {
        putstr("  #");
        putint(i);
        putstr(" ");
        backtrace_print_address(pcs[i] - 4u);             // the call, not the instruction after it
        putstr("\n");
    }
}

void backtrace_print(void)
{
    static volatile int done;
    backtrace_print_from(__backtrace_fp());
    done = 1;                                             // not a tail call: this function keeps its frame (see backtrace)
}
