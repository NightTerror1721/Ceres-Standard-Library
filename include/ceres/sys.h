#pragma once

#include "stddef.h"

// The machine as a program sees it: how to stop it, and where its memory is.

// The system-control device (0xFFFF0000). A shutdown is a word whose low byte is 1 and whose next byte is the
// exit status, which `ceres run` returns as its own; a plain byte write is status 0.
#define SYS_CTRL_CMD          (SYS_CTRL_BASE + 0x00)   // write: command | status << 8
#define SYS_CTRL_MEM_SIZE     (SYS_CTRL_BASE + 0x04)   // read: bytes of RAM
#define SYS_CTRL_FEATURES     (SYS_CTRL_BASE + 0x08)   // read/write: switches for behaviour that is off by default
#define SYS_CTRL_STACK_LIMIT  (SYS_CTRL_BASE + 0x0C)   // read/write: the lowest address the stack may reach
#define SYS_CTRL_FAULT_ADDR   (SYS_CTRL_BASE + 0x10)   // read: the data address of the last memory fault
#define SYS_CTRL_FAULT_ACCESS (SYS_CTRL_BASE + 0x14)   // read: its access (FAULT_READ/WRITE/FETCH) | size << 8
#define SYS_CTRL_ARGC         (SYS_CTRL_BASE + 0x18)   // read: argc, what main was started with
#define SYS_CTRL_ARGV         (SYS_CTRL_BASE + 0x1C)   // read: the address of argv
#define SYS_CTRL_ENVP         (SYS_CTRL_BASE + 0x20)   // read: the address of envp
#define FAULT_READ   1u
#define FAULT_WRITE  2u
#define FAULT_FETCH  3u
#define SYS_FEATURE_DIV_FAULT 0x01                     // a division by zero raises interrupt 4 instead of only setting Trap
#define SYS_FEATURE_IEEE_DIVIDE 0x02                   // a float division by zero gives +-inf or NaN, as IEEE 754 (CeresASM ad9c95a)

// The three that stop the machine never return. On a machine without the system-control device they
// halt with interrupts masked instead, which is as stopped as a program can make itself.
void sys_exit(void) __attribute__((__noreturn__));                // shut the VM down with status 0
void sys_exit_status(int status) __attribute__((__noreturn__));   // shut it down; the low eight bits of `status` become the exit status of `ceres run`
void sys_reset(void) __attribute__((__noreturn__));               // start the program again from its image (writes 2)
unsigned int sys_memory_size(void);  // how many bytes of RAM the machine has
unsigned int sys_features(void);     // the features register (0 unless something switched a feature on)
void sys_set_features(unsigned int features);
void sys_panic(const char* msg) __attribute__((__noreturn__));   // print "panic: <msg>" on the terminal and shut down

// What the program was started with (`ceres run prog.cres --env NAME=value -- a b`, CeresASM 9c6afbb): main(int
// argc, char** argv, char** envp) receives the same three, and these reach them anywhere (src/env.c). argv[0] is
// the program's path; argv[argc] and the envp entry after the last are NULL. 0 and NULL on an older machine.
int    sys_argc(void);
char** sys_argv(void);
char** sys_envp(void);

unsigned int sys_sp(void);           // the stack pointer, as seen by this call (asm/sys.casm) ...
#define sys_sp() ((unsigned int)__builtin_stack_pointer())   // ... read in place, with no call
unsigned int sys_heap_start(void);   // the linker's __heap_start: first free byte above the image
unsigned int sys_stack_free(void);   // bytes between the top of the heap and sp

// The lowest address the stack may reach: a push below it is a StackOverflow (CeresASM fcf7d4c). It starts at
// the end of the image, and malloc raises it to the top of the heap each time the heap grows, so a stack that
// runs down into allocations faults instead of rewriting them. It cannot be put below the image; all-ones on
// a machine that has no such register.
unsigned int sys_stack_limit(void);
void sys_set_stack_limit(unsigned int address);

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
// grew past its limit) falls through to the BIOS's default handler, which shuts the machine down with status
// 1; `ceres run` then names the fault, the instruction's address and, for a memory fault, the access on stderr
// (CeresASM cdf151c; before it the stub printed a bare "E" and halted, and the run never ended). A
// program that calls sys_install_fault_handlers() carries handlers for interrupts 1, 2, 3, 5, 6 and 7
// instead; each reports the kind of fault, the address of the instruction and the flags, then stops the
// machine (with --debug the address can be turned into file:line). For a memory fault (3, 6, 7) it also says
// what the instruction was doing - "store 2 bytes to 0x10001" - from the machine's fault registers (CeresASM
// de96a5f); and a program linked with a symbol table (ceresc --symtab) gets the function the fault is in and
// the callers above it, as ceres/backtrace.h names them. The module binds those vectors, and the linker allows
// ONE binding per vector for the whole program: a program with its own handlers must not call this. Pass a
// hook to take over the report; the machine stops when it returns. A hook may use the three functions below.
typedef void (*fault_hook_t)(int irq, unsigned int pc, unsigned int flags);
void sys_install_fault_handlers(fault_hook_t hook);   // hook may be NULL: the default report
unsigned int sys_fault_address(void);   // the data address the last memory fault was reaching
unsigned int sys_fault_access(void);    // FAULT_READ, FAULT_WRITE or FAULT_FETCH in bits 0-7, with the size in bytes << 8 (bits 8-31)
unsigned int sys_fault_frame(void);     // the faulting program's fp, for backtrace_from() (0 outside a fault)
