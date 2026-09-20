// read_yes_no and read_choice. The input is tests/expected/test_ask.stdin (under 64 bytes: see test_line.c).
#include "ceres/test.h"
#include "ceres/line.h"

int main(void)
{
    TEST_SECTION("yes or no");
    CHECK_EQ(read_yes_no("ok?", 0), 1);                          // "y"
    CHECK_EQ(read_yes_no("ok?", 1), 0);                          // "N"
    CHECK_EQ(read_yes_no("ok?", 1), 1);                          // an empty line: the default (yes)
    CHECK_EQ(read_yes_no("ok?", 0), 0);                          // an empty line: the default (no)
    CHECK_EQ(read_yes_no("ok?", 0), 0);                          // "maybe" is refused, then "no" is taken
    CHECK_EQ(read_yes_no("ok?", 0), 1);                          // "  YES  "

    TEST_SECTION("a numbered choice");
    const char* options[3] = { "red", "green", "blue" };
    CHECK_EQ(read_choice("pick", options, 3), 3);
    CHECK_EQ(read_choice("pick", options, 3), 2);                // "9" and "x" are refused, then "2"
    return test_summary();
}
