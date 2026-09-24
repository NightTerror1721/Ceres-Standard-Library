#pragma once

#include "stddef.h"

// The machine as a program sees it: how to stop it, and where its memory is.

// The system-control device (0xFFFF0000). A shutdown is a word whose low byte is 1 and whose next byte is the
// exit status, which `ceres run` returns as its own; a plain byte write is status 0.
#define SYS_CTRL_CMD          (SYS_CTRL_BASE + 0x00)   // write: command | status << 8
#define SYS_CTRL_MEM_SIZE     (SYS_CTRL_BASE + 0x04)   // read: bytes of RAM
#define SYS_CTRL_FEATURES     (SYS_CTRL_BASE + 0x08)   // read/write: switches for behaviour that is off by default
#define SYS_FEATURE_DIV_FAULT 0x01                     // a division by zero raises interrupt 4 instead of only setting Trap

// The three that stop the machine never return. On a machine without the system-control device they
// halt with interrupts masked instead, which is as stopped as a program can make itself.
void sys_exit(void) __attribute__((noreturn));                // shut the VM down with status 0
void sys_exit_status(int status) __attribute__((noreturn));   // shut it down; the low eight bits of `status` become the exit status of `ceres run`
void sys_reset(void) __attribute__((noreturn));               // start the program again from its image (writes 2)
unsigned int sys_memory_size(void);  // how many bytes of RAM the machine has
unsigned int sys_features(void);     // the features register (0 unless something switched a feature on)
void sys_set_features(unsigned int features);
void sys_panic(const char* msg) __attribute__((noreturn));   // print "panic: <msg>" on the terminal and shut down

unsigned int sys_sp(void);           // the stack pointer, as seen by this call (asm/sys.casm) ...
#define sys_sp() __builtin_stack_pointer()   // ... read in place, with no call
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
