// Long lines, the line endings, and discarding what is waiting. The input is
// tests/expected/test_line_edges.stdin (under 64 bytes: see test_line.c).
#include "ceres/test.h"
#include "ceres/line.h"

int main(void)
{
    char buf[80];
    char small[8];

    TEST_SECTION("long lines");
    CHECK_EQ(line_read(small, sizeof(small)), 7);                // stores 7 characters and a NUL ...
    CHECK_STR(small, "1234567");
    CHECK_EQ(line_read(buf, sizeof(buf)), 4);                    // ... and the rest of that line is gone
    CHECK_STR(buf, "tail");

    TEST_SECTION("line endings");
    CHECK_EQ(line_read(buf, sizeof(buf)), 3);                    // "one\r\n" is ONE line ending
    CHECK_STR(buf, "one");
    CHECK_EQ(line_read(buf, sizeof(buf)), 3);                    // "two\r" alone ends a line too ...
    CHECK_STR(buf, "two");
    CHECK_EQ(line_read(buf, sizeof(buf)), 5);                    // ... and the byte after it is not lost
    CHECK_STR(buf, "three");

    TEST_SECTION("discarding what is waiting");
    for (int spin = 0; spin < 3000000 && line_available() < 22; spin++)
    {
    }
    CHECK_EQ(line_available(), 22);                              // "discard me\nand me too\n"
    line_discard();
    CHECK_EQ(line_available(), 0);
    return test_summary();
}
