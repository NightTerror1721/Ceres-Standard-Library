// The machine as a program sees it: the memory map the linker built, the stack, and sys_panic.
// Nothing here prints an address (they change with the optimization level); it checks how the
// addresses relate to each other.
#include "ceres/test.h"
#include "ceres/sys.h"
#include "ceres/heap.h"
#include "ceres.h"

static int initialized = 5;          // .data
static int zeroed;                   // .bss
static const int constant = 7;       // .rodata

int main(void)
{
    struct sys_layout l;
    sys_get_layout(&l);

    TEST_SECTION("the image is laid out in order");
    CHECK_EQ((int)l.text_start, CERES_TEXT_BASE);              // .text begins where the loader puts it
    CHECK(l.text_end > l.text_start);
    CHECK(l.rodata_start >= l.text_end);
    CHECK(l.data_start >= l.rodata_start);
    CHECK(l.bss_start >= l.data_start);
    CHECK(l.bss_end >= l.bss_start);
    CHECK_EQ((int)l.heap_start, (int)l.bss_end);               // the heap starts where .bss ends
    CHECK_EQ((int)sys_heap_start(), (int)l.heap_start);

    TEST_SECTION("our own variables live in the right sections");
    unsigned int a = (unsigned int)&initialized;
    unsigned int z = (unsigned int)&zeroed;
    unsigned int c = (unsigned int)&constant;
    CHECK(a >= l.data_start && a < l.data_end);
    CHECK(z >= l.bss_start && z < l.bss_end);
    CHECK(c >= l.rodata_start && c < l.rodata_end);
    CHECK_EQ(initialized + zeroed + constant, 12);

    TEST_SECTION("the stack");
    unsigned int local = 0;
    unsigned int here = (unsigned int)&local;
    CHECK(l.sp > l.heap_start);                                // the stack lives above the image
    CHECK(sys_sp() > l.heap_start);
    CHECK(here > l.heap_start);                                // a local is on it
    CHECK(here < CERES_DEFAULT_RAM);                           // ... inside a default 16 MiB machine
    CHECK(sys_stack_free() > 1024u * 1024u);                   // plenty of ground between heap and stack
    CHECK(sys_stack_free() < CERES_DEFAULT_RAM);
    void* m = malloc(64 * 1024);
    CHECK(m != 0);
    CHECK(sys_stack_free() < CERES_DEFAULT_RAM);
    free(m);

    TEST_SECTION("panic");
    int verdict = test_summary();
    sys_panic("boom");                                         // prints, then shuts the machine down
    puts("NOT REACHED");
    return verdict;
}
