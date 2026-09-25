// mmu_on_fault (ceres/mmu.h): the C side of the optional "mmu" module's page fault handler.
#include "ceres/mmu.h"
#include "ceres/sys.h"
#include "ceres/terminal.h"

extern unsigned int __mmu_fault_target;             // asm/optional/mmu_fault.casm
void __mmu_fault_present(void);

static mmu_fault_fn handler = 0;

static void err(const char* s)
{
    int n = 0;
    while (s[n] != 0)
        n++;
    term_write_error(s, n);
}

static void err_hex(unsigned int v)
{
    char text[11] = "0x00000000";
    for (int i = 0; i < 8; i++)
        text[9 - i] = "0123456789abcdef"[(v >> (4 * i)) & 15u];
    err(text);
}

// Runs on the system stack with interrupts masked.
static int dispatch(unsigned int va, unsigned int pc)
{
    unsigned int access = sys_fault_access() & 0xFFu;
    if (handler != 0 && handler(va, pc, access))
        return 1;
    err("page fault: ");
    err(access == FAULT_WRITE ? "store to " : access == FAULT_FETCH ? "fetch from " : "load from ");
    err_hex(va);
    err(" at ");
    err_hex(pc);
    err("\n");
    sys_exit_status(139);                            // as the fault module ends a program: 128 + SIGSEGV
    return 0;
}

void mmu_on_fault(mmu_fault_fn f)
{
    handler = f;
    __mmu_fault_target = (unsigned int)dispatch;
    __mmu_fault_present();
}
