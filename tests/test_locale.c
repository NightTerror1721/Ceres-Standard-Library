// The one locale there is.
#include "ceres/test.h"
#include "locale.h"
#include "string.h"

int main(void)
{
    TEST_SECTION("setlocale");
    CHECK_STR(setlocale(LC_ALL, 0), "C");                 // a query
    CHECK_STR(setlocale(LC_ALL, ""), "C");                // the environment's locale: there is no environment
    CHECK_STR(setlocale(LC_ALL, "C"), "C");
    CHECK_STR(setlocale(LC_CTYPE, "POSIX"), "C");
    CHECK_STR(setlocale(LC_NUMERIC, 0), "C");
    CHECK_STR(setlocale(LC_TIME, "C"), "C");
    CHECK(setlocale(LC_ALL, "es_ES.UTF-8") == 0);         // there is no other
    CHECK(setlocale(LC_ALL, "en_US") == 0);
    CHECK(setlocale(-1, "C") == 0);                       // not a category
    CHECK(setlocale(99, 0) == 0);
    CHECK_STR(setlocale(LC_ALL, 0), "C");                 // a refused request changed nothing

    TEST_SECTION("localeconv");
    struct lconv* lc = localeconv();
    CHECK(lc != 0);
    CHECK_STR(lc->decimal_point, ".");
    CHECK_STR(lc->thousands_sep, "");
    CHECK_STR(lc->grouping, "");
    CHECK_STR(lc->currency_symbol, "");
    CHECK_STR(lc->positive_sign, "");
    CHECK_STR(lc->negative_sign, "");
    CHECK_EQ(lc->frac_digits, 127);                       // CHAR_MAX: not available
    CHECK_EQ(lc->p_sign_posn, 127);
    CHECK(localeconv() == lc);                            // always the same object
    return test_summary();
}
