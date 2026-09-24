// malloc checking itself: the library built with CERES_HEAP_DEBUG=1 (tests/expected/test_heap_debug.flags).
// An overrun, a write after free and a double free are all found, and reported through the error handler.
#include "ceres/test.h"
#include "ceres/heap.h"
#include "ceres/config.h"
#include "stdlib.h"
#include "string.h"

static int reports = 0;
static void report(const char* what, void* p)
{
    reports++;
    putstr("reported: ");
    putstr(what);
    putstr("\n");
}

int main(void)
{
    TEST_SECTION("the build");
    CHECK_EQ(CERES_HEAP_DEBUG, 1);
    heap_set_error_handler(report);

    TEST_SECTION("in bounds, nothing to say");
    char* a = (char*)malloc(10);
    char* b = (char*)malloc(100);
    memset(a, 'a', 10);
    memset(b, 'b', 100);
    CHECK(malloc_usable_size(a) >= 10);
    CHECK_EQ(heap_check(), 0);
    b = (char*)realloc(b, 300);
    CHECK(b[99] == 'b');
    free(b);
    CHECK_EQ(heap_check(), 0);
    CHECK_EQ(reports, 0);

    TEST_SECTION("a write after free");
    char* gone = (char*)malloc(48);
    free(gone);
    CHECK_EQ(heap_check(), 0);
    gone[20] = 7;                                         // freed memory is filled with 0xDD: this shows
    CHECK(heap_check() != 0);
    gone[20] = (char)0xDD;
    CHECK_EQ(heap_check(), 0);

    TEST_SECTION("a double free");
    char* twice = (char*)malloc(16);
    free(twice);
    free(twice);
    CHECK_EQ(reports, 1);

    TEST_SECTION("an overrun");
    a[10] = 'x';                                          // one past the 10 bytes asked for
    CHECK(heap_check() != 0);
    free(a);                                              // refused: the block is left as it is
    CHECK_EQ(reports, 2);
    a[10] = (char)0xCA;                                   // the canary byte, put back
    CHECK_EQ(heap_check(), 0);
    free(a);
    CHECK_EQ(reports, 2);
    CHECK_EQ((int)heap_used(), 0);
    CHECK_EQ(heap_check(), 0);
    return test_summary();
}
