#pragma once

// The call stack as it stands, and names for code addresses.
//
// Every function that calls another keeps a frame: [fp] is the caller's fp, and the address it returns to is
// the first word above the callee-saved registers the function pushed before its `enter` - [fp + 4] only when
// it pushed none (CeresASM docs/24-Calling-Convention.md; Ceres-C pushes r8-r11 there from -O1 on) - so the
// callers are a chain of frames from fp up, and the walk finds each return address. A
// leaf function built with frameless leaves (-O1 and up) keeps none, and is simply not in the chain - it is
// the innermost, and the fault report names it by the PC instead. The walk stays inside the program's own
// stack (between the stack limit and the top the machine gave it) and stops at a frame that does not go up,
// so a smashed frame ends it rather than faulting in the middle of a report.
//
// Names come from the table `ceresc --symtab` links into the program (`ceres link --symtab`, CeresASM
// de96a5f): every global function and its address. Without it, or for an address before the first
// function, backtrace_symbol() returns NULL and the printers show the bare address. A static function is
// not in the table, and is named after the global function before it.
//
//   unsigned int pcs[16];
//   int n = backtrace(pcs, 16);                 // this function, then its callers, innermost first
//   backtrace_print();                          // "  #0 parse_line+0x1c", one per caller

int  backtrace(unsigned int* pcs, int max);                    // return addresses: pcs[0] is in the function calling it
int  backtrace_from(unsigned int fp, unsigned int* pcs, int max);   // from a frame pointer (a fault's)
const char* backtrace_symbol(unsigned int pc, unsigned int* offset);   // the function pc is in, or NULL
int  backtrace_has_symbols(void);                              // nonzero when the program carries a symbol table
void backtrace_print(void);                                    // the same, one line each
void backtrace_print_from(unsigned int fp);                    // the chain from a frame pointer
void backtrace_print_address(unsigned int pc);                 // "name+0x1c", or the address when no name is known

unsigned int __backtrace_fp(void);                             // asm/backtrace.casm: the caller's fp
