// Address spaces over the MMU. See ceres/mmu.h.
#include "ceres/mmu.h"
#include "ceres/sys.h"
#include "ceres.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"

void __mmu_set_directory(unsigned int directory);   // asm/mmu.casm
void __mmu_enable(void);
void __mmu_disable(void);
void __mmu_invalidate(unsigned int address);

extern void (*__task_stack_guard)(unsigned char* stack, int guard);   // src/ceres/task_guard.c

#define FRAME_MASK 0xFFFFF000u
#define FLAG_MASK  0x00000FFFu

static struct mmu_space* active = 0;

static int aligned(unsigned int x)
{
    return (x & (MMU_PAGE - 1u)) == 0;
}

static unsigned int* new_page(void)
{
    unsigned int* p = (unsigned int*)aligned_alloc(MMU_PAGE, MMU_PAGE);
    if (p != 0)
        memset(p, 0, MMU_PAGE);
    return p;
}

int mmu_space_init(struct mmu_space* s)
{
    if (s == 0)
    {
        errno = EINVAL;
        return -1;
    }
    s->directory = new_page();
    if (s->directory == 0)
    {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

static struct mmu_space* guard_space = 0;

void mmu_space_free(struct mmu_space* s)
{
    if (s == 0 || s->directory == 0 || s == active)
        return;
    if (s == guard_space)
    {
        guard_space = 0;                                 // stacks spawned from now on are not guarded
        __task_stack_guard = 0;
    }
    for (int i = 0; i < 1024; i++)
        if (s->directory[i] & MMU_PRESENT)
            free((void*)(s->directory[i] & FRAME_MASK));
    free(s->directory);
    s->directory = 0;
}

// The table that holds va's entry; with `create`, made when there is none. 0 when there is none (or no memory).
static unsigned int* table_of(const struct mmu_space* s, unsigned int va, int create)
{
    unsigned int* entry = &s->directory[va >> 22];
    if (!(*entry & MMU_PRESENT))
    {
        if (!create)
            return 0;
        unsigned int* table = new_page();
        if (table == 0)
            return 0;
        *entry = (unsigned int)table | MMU_PRESENT;
    }
    return (unsigned int*)(*entry & FRAME_MASK);
}

static void changed(const struct mmu_space* s, unsigned int va)
{
    if (s == active)
        __mmu_invalidate(va);
}

static int check(const struct mmu_space* s, unsigned int va, unsigned int bytes)
{
    if (s == 0 || s->directory == 0 || !aligned(va) || !aligned(bytes) || va + bytes < va)
    {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int mmu_map(struct mmu_space* s, unsigned int va, unsigned int pa, unsigned int bytes, unsigned int flags)
{
    if (check(s, va, bytes) != 0 || !aligned(pa))
    {
        errno = EINVAL;
        return -1;
    }
    for (unsigned int off = 0; off < bytes; off += MMU_PAGE)
    {
        unsigned int* table = table_of(s, va + off, 1);
        if (table == 0)
        {
            errno = ENOMEM;
            return -1;
        }
        table[((va + off) >> 12) & 1023u] = ((pa + off) & FRAME_MASK) | MMU_PRESENT | (flags & (MMU_WRITE | MMU_EXEC));
        changed(s, va + off);
    }
    return 0;
}

int mmu_unmap(struct mmu_space* s, unsigned int va, unsigned int bytes)
{
    if (check(s, va, bytes) != 0)
        return -1;
    for (unsigned int off = 0; off < bytes; off += MMU_PAGE)
    {
        unsigned int* table = table_of(s, va + off, 0);
        if (table != 0)
        {
            table[((va + off) >> 12) & 1023u] = 0;
            changed(s, va + off);
        }
    }
    return 0;
}

int mmu_protect(struct mmu_space* s, unsigned int va, unsigned int bytes, unsigned int flags)
{
    if (check(s, va, bytes) != 0)
        return -1;
    for (unsigned int off = 0; off < bytes; off += MMU_PAGE)
    {
        unsigned int* table = table_of(s, va + off, 0);
        unsigned int* entry = table != 0 ? &table[((va + off) >> 12) & 1023u] : 0;
        if (entry == 0 || !(*entry & MMU_PRESENT))
        {
            errno = ENOENT;                              // only what is mapped can change what it allows
            return -1;
        }
        *entry = (*entry & ~(MMU_WRITE | MMU_EXEC)) | (flags & (MMU_WRITE | MMU_EXEC));
        changed(s, va + off);
    }
    return 0;
}

int mmu_translate(const struct mmu_space* s, unsigned int va, unsigned int* pa)
{
    if (s == 0 || s->directory == 0)
    {
        errno = EINVAL;
        return -1;
    }
    unsigned int* table = table_of(s, va, 0);
    unsigned int entry = table != 0 ? table[(va >> 12) & 1023u] : 0u;
    if (!(entry & MMU_PRESENT))
    {
        errno = ENOENT;
        return -1;
    }
    if (pa != 0)
        *pa = (entry & FRAME_MASK) | (va & (MMU_PAGE - 1u));
    return (int)(entry & FLAG_MASK);
}

int mmu_identity(struct mmu_space* s, unsigned int flags)
{
    unsigned int end = (sys_memory_size() - CERES_SYSTEM_STACK) & FRAME_MASK;
    return mmu_map(s, 0, 0, end, flags);
}

void mmu_activate(struct mmu_space* s)
{
    if (s == 0 || s->directory == 0)
        return;
    __mmu_set_directory((unsigned int)s->directory);   // also flushes the TLB
    active = s;
    __mmu_enable();
}

void mmu_deactivate(void)
{
    __mmu_disable();
    active = 0;
}

struct mmu_space* mmu_active(void)
{
    return active;
}

// ---- guard pages under task stacks ----

// The heap is identity-mapped in the guarding space (mmu_identity), so a stack's page goes back to itself. Only the
// page's entry changes - its table exists, since the page was mapped - so neither call can fail for want of memory.
// A stack spawned before the space was freed keeps its page unmapped: the space, and so the fault, is gone.
static void guard_stack(unsigned char* stack, int guard)
{
    if (guard_space == 0)
        return;
    if (guard)
        mmu_unmap(guard_space, (unsigned int)stack, MMU_PAGE);
    else
        mmu_map(guard_space, (unsigned int)stack, (unsigned int)stack, MMU_PAGE, MMU_WRITE);   // back, before free
}

int mmu_guard_task_stacks(struct mmu_space* s)
{
    if (s == 0 || s->directory == 0)
    {
        errno = EINVAL;
        return -1;
    }
    guard_space = s;
    __task_stack_guard = guard_stack;
    return 0;
}
