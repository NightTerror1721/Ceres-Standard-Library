// USE: mmu
// The MMU (ceres/mmu.h): an identity-mapped space keeps the program running, a page mapped somewhere else reads
// and writes the frame behind it, a read-only page faults and is made writable by the handler (the store runs
// again), and pages that do not exist yet are made on demand.
#include "ceres/test.h"
#include "ceres/mmu.h"
#include "ceres/sys.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#define ELSEWHERE 0x40000000u                        // far above RAM: only the MMU makes it mean anything
#define READONLY  0x40001000u
#define DEMAND    0x50000000u

static struct mmu_space space;
static int faults = 0;
static unsigned int last_access = 0;
static int demand_frames = 0;

static int on_fault(unsigned int va, unsigned int pc, unsigned int access)
{
    faults++;
    last_access = access;
    if (va >= READONLY && va < READONLY + MMU_PAGE)
        return mmu_protect(&space, READONLY, MMU_PAGE, MMU_WRITE) == 0;   // allow it, and try again
    if (va >= DEMAND && va < DEMAND + 4u * MMU_PAGE)
    {
        void* frame = aligned_alloc(MMU_PAGE, MMU_PAGE);
        if (frame == 0)
            return 0;
        memset(frame, 0, MMU_PAGE);
        demand_frames++;
        return mmu_map(&space, va & ~(MMU_PAGE - 1u), (unsigned int)frame, MMU_PAGE, MMU_WRITE) == 0;
    }
    return 0;
}

int main(void)
{
    TEST_SECTION("an identity space");
    CHECK_EQ(mmu_space_init(&space), 0);
    CHECK_EQ(mmu_identity(&space, MMU_WRITE | MMU_EXEC), 0);
    mmu_on_fault(on_fault);
    CHECK(mmu_active() == 0);
    mmu_activate(&space);
    CHECK(mmu_active() == &space);
    printf("still running, translated\n");
    unsigned int pa = 0;
    int flags = mmu_translate(&space, (unsigned int)&space, &pa);
    CHECK(flags != -1 && (flags & MMU_PRESENT));
    CHECK(pa == (unsigned int)&space);

    TEST_SECTION("a page mapped elsewhere");
    unsigned int* frame = (unsigned int*)aligned_alloc(MMU_PAGE, MMU_PAGE);
    CHECK_EQ(mmu_map(&space, ELSEWHERE, (unsigned int)frame, MMU_PAGE, MMU_WRITE), 0);
    volatile unsigned int* virt = (volatile unsigned int*)ELSEWHERE;
    virt[5] = 0xC0FFEE;
    CHECK_EQ(frame[5], 0xC0FFEEu);                     // the same memory, by its physical address
    CHECK_EQ(mmu_translate(&space, ELSEWHERE + 20, &pa) & (MMU_PRESENT | MMU_WRITE), (int)(MMU_PRESENT | MMU_WRITE));
    CHECK_EQ(pa, (unsigned int)frame + 20);
    CHECK_EQ(mmu_map(&space, ELSEWHERE + 1, 0, MMU_PAGE, 0), -1);   // not a page boundary

    TEST_SECTION("a read-only page");
    unsigned int* ro = (unsigned int*)aligned_alloc(MMU_PAGE, MMU_PAGE);
    ro[0] = 11;
    CHECK_EQ(mmu_map(&space, READONLY, (unsigned int)ro, MMU_PAGE, MMU_READ), 0);
    volatile unsigned int* rov = (volatile unsigned int*)READONLY;
    CHECK_EQ(rov[0], 11u);                              // reading is allowed
    CHECK_EQ(faults, 0);
    rov[0] = 22;                                        // writing faults; the handler allows it; it runs again
    CHECK_EQ(faults, 1);
    CHECK_EQ(last_access, FAULT_WRITE);
    CHECK_EQ(ro[0], 22u);

    TEST_SECTION("pages on demand");
    volatile unsigned char* lazy = (volatile unsigned char*)DEMAND;
    for (unsigned int i = 0; i < 3u * MMU_PAGE; i += 1024u)
        lazy[i] = (unsigned char)(i >> 10);
    CHECK_EQ(demand_frames, 3);                         // one fault, one frame, per page touched
    int sum = 0;
    for (unsigned int i = 0; i < 3u * MMU_PAGE; i += 1024u)
        sum += lazy[i];
    CHECK_EQ(sum, 66);                                  // 0 + 1 + ... + 11
    CHECK_EQ(faults, 4);

    TEST_SECTION("unmapped again");
    CHECK_EQ(mmu_unmap(&space, ELSEWHERE, MMU_PAGE), 0);
    CHECK_EQ(mmu_translate(&space, ELSEWHERE, 0), -1);
    mmu_deactivate();
    CHECK(mmu_active() == 0);
    printf("physical again\n");
    mmu_space_free(&space);
    return test_summary();
}
