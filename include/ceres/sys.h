#pragma once

#include "stddef.h"

// The machine as a program sees it: how to stop it, and where its memory is.

void sys_exit(void);                 // shut the VM down (writes 1 to the system-control device)
void sys_reset(void);                // reset the VM (writes 2)
void sys_panic(const char* msg);     // print "panic: <msg>" on the terminal and shut down

unsigned int sys_sp(void);           // the stack pointer, as seen by this call (asm/sys.casm)
unsigned int sys_heap_start(void);   // the linker's __heap_start: first free byte above the image
unsigned int sys_stack_free(void);   // bytes between the top of the heap and sp

// The image and the ground around it. The linker defines the section bounds; the stack pointer
// is read at the moment of the call.
struct sys_layout
{
    unsigned int text_start,   text_end;
    unsigned int rodata_start, rodata_end;
    unsigned int data_start,   data_end;
    unsigned int bss_start,    bss_end;
    unsigned int heap_start,   sp;
};

void sys_get_layout(struct sys_layout* out);   // asm/sys.casm
void sys_print_layout(void);                   // a readable table on the terminal

// ---- reporting faults (the optional "fault" module) ----
// Without it, a fault (a misaligned load, a store to the null page, an illegal instruction, a stack that
// grew past its limit) falls through to the BIOS's default stub, which prints a bare "E" and halts. A
// program that calls sys_install_fault_handlers() carries handlers for interrupts 1, 2, 3, 5, 6 and 7
// instead; each reports the kind of fault, the address of the instruction and the flags, then stops the
// machine (with --debug the address can be turned into file:line). The module binds those vectors, and the
// linker allows ONE binding per vector for the whole program: a program with its own handlers must not
// call this. Pass a hook to take over the report; the machine stops when it returns.
typedef void (*fault_hook_t)(int irq, unsigned int pc, unsigned int flags);
void sys_install_fault_handlers(fault_hook_t hook);   // hook may be NULL: the default report
