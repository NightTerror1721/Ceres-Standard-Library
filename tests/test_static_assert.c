// static_assert (checked while compiling, so this file compiling IS most of the test) and __func__.
#include "ceres/test.h"
#include "assert.h"
#include "time.h"
#include "ceres/ns64.h"

enum { WORDS = 2 };

static_assert(sizeof(int) == 4, "an int is a word");
static_assert(WORDS * sizeof(int) == 8);                // the message is optional

struct pair
{
    int a;
    int b;
    static_assert(sizeof(int) == 4, "inside a struct");
};
static_assert(sizeof(struct pair) == WORDS * 4, "a pair is two words");
static_assert(sizeof(struct ns64) == sizeof(struct pair), "the library's own layouts hold");
static_assert(sizeof(struct tm) == 9 * sizeof(int), "a tm is nine words");

static const char* name_of_this(void)
{
    return __func__;
}

static const char* and_of_that(void)
{
    static_assert(1, "in a block");
    return __FUNCTION__;
}

int main(void)
{
    TEST_SECTION("__func__");
    CHECK_STR(name_of_this(), "name_of_this");
    CHECK_STR(and_of_that(), "and_of_that");
    CHECK_STR(__func__, "main");
    CHECK_EQ((int)__func__[0], 'm');

    TEST_SECTION("static_assert in a block does nothing at run time");
    static_assert(sizeof(char) == 1, "a char is a byte");
    int n = 3;
    CHECK_EQ(n, 3);

    return test_summary();
}
