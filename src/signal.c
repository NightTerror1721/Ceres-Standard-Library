// signal and raise. See signal.h for what makes each signal happen.
//
// The table is a small array of handlers; the hardware side is in src/ceres/fault.c, which asks
// __signal_of_fault() which signal a fault is and __signal_deliver() to run its handler.
#include "signal.h"
#include "stdlib.h"
#include "errno.h"
#include "ceres/sys.h"

#define SIGNAL_SLOTS 16

static __sighandler_t handlers[SIGNAL_SLOTS];      // all SIG_DFL (0) to begin with

int __signal_hw_ready;                             // set by sys_install_fault_handlers: the fault vectors are bound

extern void (*__abort_hook)(void);                 // exit.c: what abort() runs first, if anything

static int is_signal(int sig)
{
    return sig == SIGINT || sig == SIGILL || sig == SIGABRT || sig == SIGFPE || sig == SIGSEGV || sig == SIGTERM;
}

static int is_hardware(int sig)
{
    return sig == SIGFPE || sig == SIGILL || sig == SIGSEGV;
}

// abort() with a SIGABRT handler installed: the handler runs, and if it returns abort() stops the program.
static void run_abort_handler(void)
{
    __sighandler_t handler = handlers[SIGABRT];
    if (handler != SIG_DFL && handler != SIG_IGN)
        handler(SIGABRT);
}

__sighandler_t signal(int sig, __sighandler_t handler)
{
    if (!is_signal(sig) || handler == SIG_ERR)
    {
        errno = EINVAL;
        return SIG_ERR;
    }
    if (is_hardware(sig) && handler != SIG_DFL && handler != SIG_IGN && !__signal_hw_ready)
    {
        errno = ENOSYS;                            // nothing would ever deliver it: the fault vectors are not bound
        return SIG_ERR;
    }

    __sighandler_t previous = handlers[sig];
    handlers[sig] = handler;

    if (sig == SIGFPE && __signal_hw_ready)
    {
        // The machine only raises a division by zero when asked to; with no handler it goes back to setting the
        // Trap flag and carrying on, which is what a program that never heard of signals expects.
        unsigned int features = sys_features();
        sys_set_features(handler == SIG_DFL ? (features & ~(unsigned int)SYS_FEATURE_DIV_FAULT)
                                            : (features | SYS_FEATURE_DIV_FAULT));
    }
    if (sig == SIGABRT)
        __abort_hook = handler == SIG_DFL ? 0 : run_abort_handler;
    return previous;
}

int raise(int sig)
{
    if (!is_signal(sig))
    {
        errno = EINVAL;
        return -1;
    }
    __sighandler_t handler = handlers[sig];
    if (handler == SIG_IGN)
        return 0;
    if (handler == SIG_DFL)
    {
        if (sig == SIGABRT)
            abort();
        _Exit(128 + sig);                          // terminated by the signal, as a shell would say
    }
    handler(sig);
    return 0;
}

// ---- for the fault module ----

// Which signal a fault interrupt is; 0 for one that is none (Trap).
int __signal_of_fault(int irq)
{
    switch (irq)
    {
    case 2: return SIGILL;
    case 4: return SIGFPE;
    case 3:
    case 5:
    case 6:
    case 7: return SIGSEGV;
    }
    return 0;
}

// Runs the handler for `sig`. Returns 1 when there was one (or the signal is ignored), 0 for the default.
int __signal_deliver(int sig)
{
    __sighandler_t handler = handlers[sig];
    if (handler == SIG_DFL)
        return 0;
    if (handler != SIG_IGN)
        handler(sig);
    return 1;
}
