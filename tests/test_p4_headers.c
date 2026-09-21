// stdbool.h, iso646.h and the two ctype extensions.
#include "ceres/test.h"
#include "stdbool.h"
#include "iso646.h"
#include "ctype.h"

int main(void)
{
    TEST_SECTION("stdbool");
    bool yes = true;
    bool no = false;
    CHECK(yes);
    CHECK(!no);
    CHECK_EQ(yes + yes, 2);
    CHECK_EQ(__bool_true_false_are_defined, 1);
    _Bool alias = 5;                                       // any non-zero value is true
    CHECK_EQ(alias, 1);

    TEST_SECTION("iso646");
    int a = 6, b = 3;
    CHECK((a > 1 and b > 1));
    CHECK((a > 9 or b > 1));
    CHECK(not (a > 9));
    CHECK_EQ(a bitand b, 2);
    CHECK_EQ(a bitor b, 7);
    CHECK_EQ(a xor b, 5);
    CHECK_EQ(compl 0, -1);
    CHECK((a not_eq b));
    int c = 12;
    c and_eq 10;
    CHECK_EQ(c, 8);
    c or_eq 3;
    CHECK_EQ(c, 11);
    c xor_eq 1;
    CHECK_EQ(c, 10);

    TEST_SECTION("isascii and toascii");
    CHECK(isascii(0));
    CHECK(isascii('A'));
    CHECK(isascii(127));
    CHECK(!isascii(128));
    CHECK(!isascii(255));
    CHECK(!isascii(-1));
    CHECK_EQ(toascii('A'), 'A');
    CHECK_EQ(toascii(0xC1), 0x41);                         // the high bits go
    CHECK_EQ(toascii(0x1FF), 0x7F);
    return test_summary();
}
