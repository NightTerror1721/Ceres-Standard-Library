// strtol, strtoul, strtof, atoi, atol, atof: what is parsed, where parsing stops, and errno.
#include "ceres/test.h"
#include "stdlib.h"
#include "limits.h"
#include "errno.h"
#include "math.h"

// Parses `text` in `base`, checks the value and that parsing stopped after `used` characters.
// The text is copied to a pointer first: two mentions of one string literal need not be one address.
#define CHECK_LONG(text, base, want, used) \
    do { const char* t_ = (text); char* e_ = 0; errno = 0; int v_ = strtol(t_, &e_, (base)); \
         CHECK_EQ(v_, (want)); CHECK_EQ((int)(e_ - t_), (used)); } while (0)

#define CHECK_FLOAT(text, want, used) \
    do { const char* t_ = (text); char* e_ = 0; errno = 0; float v_ = strtof(t_, &e_); \
         CHECK(v_ == (want)); CHECK_EQ((int)(e_ - t_), (used)); } while (0)

int main(void)
{
    char* end;

    TEST_SECTION("strtol: decimal");
    CHECK_LONG("0", 10, 0, 1);
    CHECK_LONG("42", 10, 42, 2);
    CHECK_LONG("-42", 10, -42, 3);
    CHECK_LONG("+42", 10, 42, 3);
    CHECK_LONG("  \t\n 42", 10, 42, 7);                // leading white space is skipped
    CHECK_LONG("42abc", 10, 42, 2);                    // trailing junk stops it
    CHECK_LONG("42 43", 10, 42, 2);
    CHECK_LONG("007", 10, 7, 3);
    CHECK_LONG("- 5", 10, 0, 0);                       // a sign must touch the digits
    CHECK_LONG("--5", 10, 0, 0);
    CHECK_LONG("+-5", 10, 0, 0);

    TEST_SECTION("strtol: no number");
    CHECK_LONG("", 10, 0, 0);
    CHECK_LONG("abc", 10, 0, 0);
    CHECK_LONG("   ", 10, 0, 0);
    CHECK_LONG("-", 10, 0, 0);
    CHECK_LONG("+", 10, 0, 0);
    end = 0;
    const char* junk = "  xyz";
    strtol(junk, &end, 10);
    CHECK(end == junk);                                // endptr is the ORIGINAL pointer, not after the blanks
    CHECK_EQ(strtol("12", 0, 10), 12);                 // endptr may be NULL

    TEST_SECTION("strtol: bases");
    CHECK_LONG("ff", 16, 255, 2);
    CHECK_LONG("FF", 16, 255, 2);
    CHECK_LONG("0xff", 16, 255, 4);                    // the prefix is allowed in base 16
    CHECK_LONG("0XfF", 16, 255, 4);
    CHECK_LONG("0xff", 0, 255, 4);                     // and selects base 16 in base 0
    CHECK_LONG("0x", 16, 0, 1);                        // no digit after 0x: just the "0"
    CHECK_LONG("0xg", 0, 0, 1);
    CHECK_LONG("0x", 0, 0, 1);
    CHECK_LONG("017", 0, 15, 3);                       // a leading 0 is octal in base 0
    CHECK_LONG("017", 8, 15, 3);
    CHECK_LONG("019", 0, 1, 2);                        // 9 is not an octal digit
    CHECK_LONG("017", 10, 17, 3);
    CHECK_LONG("1010", 2, 10, 4);
    CHECK_LONG("102", 2, 2, 2);                        // stops at the first digit outside the base
    CHECK_LONG("z", 36, 35, 1);
    CHECK_LONG("Zz", 36, 1295, 2);
    CHECK_LONG("10", 36, 36, 2);
    CHECK_LONG("g", 16, 0, 0);
    CHECK_LONG("-0x10", 0, -16, 5);
    CHECK_LONG("777", 8, 511, 3);
    CHECK_LONG("12", 3, 5, 2);
    CHECK_LONG("3", 3, 0, 0);

    TEST_SECTION("strtol: invalid base");
    errno = 0;
    CHECK_EQ(strtol("10", &end, 1), 0);
    CHECK_EQ(errno, EINVAL);
    errno = 0;
    CHECK_EQ(strtol("10", &end, 37), 0);
    CHECK_EQ(errno, EINVAL);
    errno = 0;
    CHECK_EQ(strtol("10", &end, -2), 0);
    CHECK_EQ(errno, EINVAL);

    TEST_SECTION("strtol: range");
    errno = 0;
    CHECK_EQ(strtol("2147483647", &end, 10), INT_MAX);
    CHECK_EQ(errno, 0);                                // INT_MAX itself is fine
    errno = 0;
    CHECK_EQ(strtol("-2147483648", &end, 10), INT_MIN);
    CHECK_EQ(errno, 0);                                // and so is INT_MIN
    errno = 0;
    CHECK_EQ(strtol("2147483648", &end, 10), INT_MAX);
    CHECK_EQ(errno, ERANGE);
    errno = 0;
    CHECK_EQ(strtol("-2147483649", &end, 10), INT_MIN);
    CHECK_EQ(errno, ERANGE);
    errno = 0;
    const char* nines = "99999999999999999999";
    CHECK_EQ(strtol(nines, &end, 10), INT_MAX);
    CHECK_EQ(errno, ERANGE);
    CHECK_EQ((int)(end - nines), 20);                  // it still consumes every digit
    errno = 0;
    CHECK_EQ(strtol("-99999999999999999999", &end, 10), INT_MIN);
    CHECK_EQ(errno, ERANGE);
    errno = 0;
    CHECK_EQ(strtol("0x7fffffff", &end, 16), INT_MAX);
    CHECK_EQ(errno, 0);
    errno = 0;
    CHECK_EQ(strtol("0x80000000", &end, 16), INT_MAX);
    CHECK_EQ(errno, ERANGE);
    errno = 5;
    CHECK_EQ(strtol("7", &end, 10), 7);
    CHECK_EQ(errno, 5);                                // success never clears errno

    TEST_SECTION("strtoul");
    errno = 0;
    CHECK(strtoul("4294967295", &end, 10) == 4294967295u);
    CHECK_EQ(errno, 0);
    errno = 0;
    CHECK(strtoul("4294967296", &end, 10) == UINT_MAX);
    CHECK_EQ(errno, ERANGE);
    errno = 0;
    CHECK(strtoul("-1", &end, 10) == UINT_MAX);        // negation of an unsigned value, as in C
    CHECK_EQ(errno, 0);
    CHECK(strtoul("-2", &end, 10) == 4294967294u);
    CHECK(strtoul("0xFFFFFFFF", &end, 0) == UINT_MAX);
    CHECK(strtoul("3000000000", &end, 10) == 3000000000u);
    CHECK(strtoul("ffffffff", &end, 16) == UINT_MAX);
    CHECK(strtoul("zzzzzz", &end, 36) == 2176782335u);
    const char* padded = "  99";
    CHECK(strtoul(padded, &end, 10) == 99u);
    CHECK_EQ((int)(end - padded), 4);
    CHECK(strtoul("x", &end, 10) == 0u);

    TEST_SECTION("atoi, atol");
    CHECK_EQ(atoi("123"), 123);
    CHECK_EQ(atoi("  -77xyz"), -77);
    CHECK_EQ(atoi(""), 0);
    CHECK_EQ(atoi("0x10"), 0);                         // base 10: the "x" stops it
    CHECK_EQ(atoi("2147483647"), INT_MAX);
    CHECK_EQ(atol("-5"), -5);

    TEST_SECTION("strtof: plain numbers");
    CHECK_FLOAT("0", 0.0f, 1);
    CHECK_FLOAT("1", 1.0f, 1);
    CHECK_FLOAT("-1", -1.0f, 2);
    CHECK_FLOAT("3.14159", 3.14159f, 7);
    CHECK_FLOAT("-2.5", -2.5f, 4);
    CHECK_FLOAT("0.1", 0.1f, 3);
    CHECK_FLOAT("0.001", 0.001f, 5);
    CHECK_FLOAT("123456", 123456.0f, 6);
    CHECK_FLOAT("1234.5678", 1234.5678f, 9);
    CHECK_FLOAT("16777216", 16777216.0f, 8);
    CHECK_FLOAT("  42.0", 42.0f, 6);
    CHECK_FLOAT("+7.25", 7.25f, 5);
    CHECK_FLOAT("100", 100.0f, 3);
    CHECK_FLOAT("0.000001", 0.000001f, 8);             // leading zeros do not eat the mantissa
    CHECK_FLOAT("0.0", 0.0f, 3);
    CHECK_FLOAT("00012", 12.0f, 5);

    TEST_SECTION("strtof: shapes");
    CHECK_FLOAT(".5", 0.5f, 2);
    CHECK_FLOAT("5.", 5.0f, 2);
    CHECK_FLOAT("-.5", -0.5f, 3);
    CHECK_FLOAT("+.5e+1", 5.0f, 6);
    CHECK_FLOAT(".", 0.0f, 0);                         // a lone dot is not a number
    CHECK_FLOAT("-.", 0.0f, 0);
    CHECK_FLOAT("e5", 0.0f, 0);
    CHECK_FLOAT("abc", 0.0f, 0);
    CHECK_FLOAT("", 0.0f, 0);
    CHECK_FLOAT("1.5.5", 1.5f, 3);                     // stops at the second dot
    CHECK_FLOAT("12abc", 12.0f, 2);
    CHECK(signbit(strtof("-0", 0)));                   // negative zero keeps its sign
    CHECK(!signbit(strtof("0", 0)));

    TEST_SECTION("strtof: exponents");
    CHECK_FLOAT("1e3", 1000.0f, 3);
    CHECK_FLOAT("1E3", 1000.0f, 3);
    CHECK_FLOAT("1e+3", 1000.0f, 4);
    CHECK_FLOAT("1e-3", 0.001f, 4);
    CHECK_FLOAT("-2.5e2", -250.0f, 6);
    CHECK_FLOAT("2.5e-2", 0.025f, 6);
    CHECK_FLOAT("1e10", 1e10f, 4);
    CHECK_FLOAT("1e-10", 1e-10f, 5);
    CHECK_FLOAT("5e0", 5.0f, 3);
    CHECK_FLOAT("1e", 1.0f, 1);                        // an exponent needs digits: "e" is left over
    CHECK_FLOAT("1e+", 1.0f, 1);
    CHECK_FLOAT("1e-", 1.0f, 1);
    CHECK_FLOAT("1ex", 1.0f, 1);
    CHECK_FLOAT("2e2x", 200.0f, 3);
    CHECK_FLOAT("12345e-2", 123.45f, 8);
    CHECK_FLOAT("0e10", 0.0f, 4);
    CHECK_FLOAT("0.5e1", 5.0f, 5);

    TEST_SECTION("strtof: long inputs");
    CHECK_NEAR(strtof("3.14159265358979", 0), 3.14159265f, 1e-6f);
    CHECK_NEAR(strtof("2.718281828459045", 0), 2.7182817f, 1e-6f);
    CHECK_NEAR(strtof("0.000000123456789", 0), 1.23456789e-7f, 1e-13f);
    CHECK_NEAR(strtof("123456789012", 0), 1.23456789012e11f, 1e5f);   // digits past the 9th only shift the exponent
    float huge = strtof("100000000000000000000", 0);                   // 1e20: the exponent, not the mantissa, grows
    CHECK(huge > 0.99e20f && huge < 1.01e20f);

    TEST_SECTION("strtof: range");
    errno = 0;
    CHECK(isinf(strtof("1e39", 0)));
    CHECK_EQ(errno, ERANGE);
    errno = 0;
    float neg_inf = strtof("-1e39", 0);
    CHECK(isinf(neg_inf) && neg_inf < 0.0f);
    CHECK_EQ(errno, ERANGE);
    errno = 0;
    CHECK(strtof("1e-60", 0) == 0.0f);
    CHECK_EQ(errno, ERANGE);
    errno = 0;
    CHECK(strtof("3.4e38", 0) > 3.3e38f);              // near FLT_MAX but representable
    CHECK_EQ(errno, 0);
    errno = 0;
    CHECK(strtof("1e-37", 0) > 0.0f);
    CHECK_EQ(errno, 0);
    errno = 0;
    CHECK(strtof("0e999", 0) == 0.0f);
    CHECK_EQ(errno, 0);                                // zero is zero, not an underflow
    errno = 0;
    CHECK(isinf(strtof("1e99999", 0)));                // an absurd exponent must not overflow the parser
    CHECK_EQ(errno, ERANGE);

    TEST_SECTION("strtof: inf and nan");
    CHECK(isinf(strtof("inf", 0)));
    CHECK(isinf(strtof("INF", 0)));
    CHECK(isinf(strtof("Infinity", 0)));
    float minus = strtof("-inf", 0);
    CHECK(isinf(minus) && minus < 0.0f);
    CHECK(isinf(strtof("+inf", 0)) && strtof("+inf", 0) > 0.0f);
    CHECK(isnan(strtof("nan", 0)));
    CHECK(isnan(strtof("NaN", 0)));
    const char* text = "infinity!";
    strtof(text, &end);
    CHECK_EQ((int)(end - text), 8);
    text = "infx";
    strtof(text, &end);
    CHECK_EQ((int)(end - text), 3);
    text = "nanny";
    strtof(text, &end);
    CHECK_EQ((int)(end - text), 3);
    text = "in";
    CHECK(strtof(text, &end) == 0.0f);
    CHECK(end == text);

    TEST_SECTION("atof, strtod");
    CHECK(atof("2.5") == 2.5f);
    CHECK(atof("  -0.125") == -0.125f);
    CHECK(atof("junk") == 0.0f);
    CHECK(strtod("6.5e1x", &end) == 65.0f);
    CHECK(*end == 'x');
    return test_summary();
}
