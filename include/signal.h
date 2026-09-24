#pragma once

// Signals. Six are defined, and what makes each one happen is different:
//
//   SIGABRT   abort(), or raise(SIGABRT). A handler that returns lets abort() go on and stop the program.
//   SIGINT    only raise(SIGINT): the machine has no interrupt key.
//   SIGTERM   only raise(SIGTERM).
//   SIGFPE    a division by zero, an integer or a float one. The machine ignores it unless asked: signal()
//             asks (a division-fault switch in the system-control device), and when the handler returns the
//             program goes on with the instruction after the division, the destination as it was.
//   SIGILL    an illegal instruction.
//   SIGSEGV   a store into the program's own code, the vector table or the BIOS, an unaligned access, a
//             stack that ran out, a page fault.
//
// The three that the hardware raises (SIGFPE, SIGILL, SIGSEGV) reach a handler through the fault module:
// link it (`// USE: fault`, src/ceres/fault.c and asm/optional/fault.casm) and call
// sys_install_fault_handlers(0) first, which binds the vectors - one owner per vector for the whole program,
// so a program with its own fault handlers cannot use these. Without it signal() refuses a handler for one of
// them (SIG_ERR, errno ENOSYS). A handler for SIGILL or SIGSEGV that returns is undefined in C, and here the
// fault is reported and the program stops (status 139); one that calls exit() or longjmp() is fine.
//
// A handler for one of those three runs where the interrupt does: on the 4 KiB system stack, with interrupts
// masked. Keep it short - putstr and putint, a flag, exit() or longjmp() - and do not call printf or malloc from
// it. A stack overflow inside a handler is one more fault, and nothing is left to report it.
//
// A handler stays installed after it has run. With no handler (SIG_DFL) raise() ends the program with status
// 128 plus the signal number, abort() with 134 - the number a shell reports for a program that was killed.

typedef int sig_atomic_t;                 // an int is read and written in one instruction

typedef void (*__sighandler_t)(int);

#define SIG_DFL  ((__sighandler_t)0)      // the default action
#define SIG_IGN  ((__sighandler_t)1)      // ignore the signal
#define SIG_ERR  ((__sighandler_t)-1)     // signal() failed

#define SIGINT   2
#define SIGILL   4
#define SIGABRT  6
#define SIGFPE   8
#define SIGSEGV  11
#define SIGTERM  15

__sighandler_t signal(int sig, __sighandler_t handler);   // the previous handler, or SIG_ERR (errno EINVAL or ENOSYS)
int raise(int sig);                                       // 0 when the signal was handled or ignored, -1 for a number that is not one
