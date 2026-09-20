// Reading lines and numbers from the terminal. The input is tests/expected/test_line.stdin, and it must stay
// UNDER 64 BYTES: the host pushes a piped stdin into the terminal's 64-byte ring as fast as it can, with no
// flow control, so whatever does not fit is dropped. (test_ask and test_line_edges cover the rest.)
#include "ceres/test.h"
#include "ceres/line.h"

int main(void)
{
    char buf[80];

    TEST_SECTION("a line");
    CHECK_EQ(line_read(buf, sizeof(buf)), 11);
    CHECK_STR(buf, "hello world");
    CHECK_EQ(line_prompt("name? ", buf, sizeof(buf)), 5);
    CHECK_STR(buf, "Ceres");
    CHECK_EQ(line_read(buf, sizeof(buf)), 0);                    // an empty line
    CHECK_STR(buf, "");
    CHECK_EQ(line_read(buf, 0), -1);                             // no room at all
    CHECK_EQ(line_read(0, 10), -1);

    TEST_SECTION("integers");
    int v = 7;
    CHECK_EQ(read_int("n? ", &v), 0);
    CHECK_EQ(v, 42);
    CHECK_EQ(read_int("n? ", &v), 0);
    CHECK_EQ(v, -17);                                            // blanks around the number are fine
    CHECK_EQ(read_int("n? ", &v), -1);                           // "abc"
    CHECK_EQ(v, -17);                                            // and the value is left alone
    CHECK_EQ(read_int("n? ", &v), -1);                           // "12 apples": trailing junk
    CHECK_EQ(read_int("n? ", &v), -1);                           // an empty line

    TEST_SECTION("floats");
    float f = 1.0f;
    CHECK_EQ(read_float("x? ", &f), 0);
    CHECK(f == 3.5f);
    CHECK_EQ(read_float("x? ", &f), -1);                         // "1.2.3"
    CHECK(f == 3.5f);
    CHECK_EQ(line_available(), 0);                               // everything was consumed
    return test_summary();
}
