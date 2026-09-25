#pragma once

#include "../stddef.h"

// The MMU (CeresASM docs/27-Virtual-Memory-and-Paging.md): two-level page tables over 4 KiB pages, off until a
// program turns it on. An address space is a page directory and the tables below it, allocated from the heap;
// mapping a page says which physical frame a virtual page is and what it allows. The null page and the BIOS
// (below 0x400), the system stack at the top of RAM and every device (0xFF000000 up) are never translated, so
// they need no mapping.
//
//   struct mmu_space s;
//   mmu_space_init(&s);
//   mmu_identity(&s, MMU_WRITE | MMU_EXEC);   // RAM as it is: the program goes on running where it is
//   mmu_unmap(&s, guard, MMU_PAGE);           // ...but this page now faults
//   mmu_activate(&s);
//
// A page fault stops the program, reported by the fault module, unless the "mmu" module's handler is installed
// (mmu_on_fault): then a function of the program decides - map the page and return 1, and the access that faulted
// runs again (demand paging); return 0, and the fault is reported and the program stops.
//
// Addresses and sizes are whole pages: a va or pa that is not a multiple of MMU_PAGE is EINVAL. Changing a space
// that is active invalidates the pages it changes.

#define MMU_PAGE    4096u
#define MMU_READ    0x0u              // present: reads are always allowed
#define MMU_WRITE   0x2u
#define MMU_EXEC    0x4u
#define MMU_PRESENT 0x1u              // in what mmu_translate returns, with ACCESSED and DIRTY
#define MMU_ACCESSED 0x8u
#define MMU_DIRTY   0x10u

struct mmu_space
{
    unsigned int* directory;          // 1024 entries, 4 KiB-aligned
};

int  mmu_space_init(struct mmu_space* s);      // an empty space: nothing mapped; 0, or -1 (ENOMEM)
void mmu_space_free(struct mmu_space* s);      // its directory and tables (it must not be active)

int  mmu_map(struct mmu_space* s, unsigned int va, unsigned int pa, unsigned int bytes, unsigned int flags);
int  mmu_unmap(struct mmu_space* s, unsigned int va, unsigned int bytes);   // an access there now faults
int  mmu_protect(struct mmu_space* s, unsigned int va, unsigned int bytes, unsigned int flags);   // mapped pages only
int  mmu_translate(const struct mmu_space* s, unsigned int va, unsigned int* pa);   // its flags (with MMU_PRESENT), -1 when unmapped
int  mmu_identity(struct mmu_space* s, unsigned int flags);   // every page of RAM below the system stack, to itself

void mmu_activate(struct mmu_space* s);        // this space's tables, and translation on
void mmu_deactivate(void);                     // back to physical addresses
struct mmu_space* mmu_active(void);            // the space in use, or NULL

// Guard pages at the bottom of every task stack (ceres/task.h) spawned from now on: each stack gets one page more,
// page-aligned, whose first page `s` leaves unmapped, so a store that runs off the bottom of the stack faults
// instead of writing into the heap below. (A push or a frame too large - enter - is the stack limit's to catch.)
int  mmu_guard_task_stacks(struct mmu_space* s);

// ---- page faults (the optional "mmu" module: it binds the PageFault vector, so not with the fault module) ----
// handler(va, pc, access): the address, the instruction's, and FAULT_READ/WRITE/FETCH (ceres/sys.h). Return 1 once
// the page is mapped as it should be (the access runs again), 0 to report the fault and stop (status 139). It runs
// on the 4 KiB system stack with interrupts masked: keep it short, and no printf.
typedef int (*mmu_fault_fn)(unsigned int va, unsigned int pc, unsigned int access);
void mmu_on_fault(mmu_fault_fn handler);
