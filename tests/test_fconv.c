// Exact float <-> text (M8): printf's digits are the float's own, rounded to nearest with ties to even; strtof
// gives the float nearest what was written, however long; %a and hexadecimal input; the shortest digits that
// read back.
#include "ceres/test.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"
#include "float.h"
#include "math.h"

static char out[200];

static const char* fmt1(const char* f, float v)
{
    snprintf(out, sizeof out, f, v);
    return out;
}

static unsigned int bits(float f)
{
    union { float f; unsigned int u; } b;
    b.f = f;
    return b.u;
}

static float from(unsigned int u)
{
    union { float f; unsigned int u; } b;
    b.u = u;
    return b.f;
}

static unsigned int seed = 2024;
static unsigned int next(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed;
}

int main(void)
{
    TEST_SECTION("printf's digits are exact");
    CHECK_STR(fmt1("%.20f", 0.1f), "0.10000000149011611938");
    CHECK_STR(fmt1("%.30e", 1.0f / 3.0f), "3.333333432674407958984375000000e-01");
    CHECK_STR(fmt1("%f", FLT_MAX), "340282346638528859811704183484516925440.000000");
    CHECK_STR(fmt1("%.3e", from(1)), "1.401e-45");                  // the smallest subnormal
    CHECK_STR(fmt1("%.60f", from(1)), "0.000000000000000000000000000000000000000000001401298464324817");
    CHECK_STR(fmt1("%.10g", 16777217.0f), "16777216");              // not a float: it was rounded when written
    CHECK_STR(fmt1("%g", 0.0001f), "0.0001");
    CHECK_STR(fmt1("%g", 0.00001f), "1e-05");
    CHECK_STR(fmt1("%#.3g", 1.0f), "1.00");
    CHECK_STR(fmt1("%e", 0.0f), "0.000000e+00");
    CHECK_STR(fmt1("%.3f", -0.0f), "-0.000");

    TEST_SECTION("ties go to the even digit");
    CHECK_STR(fmt1("%.0f", 0.5f), "0");
    CHECK_STR(fmt1("%.0f", 1.5f), "2");
    CHECK_STR(fmt1("%.0f", 2.5f), "2");
    CHECK_STR(fmt1("%.1f", 0.25f), "0.2");
    CHECK_STR(fmt1("%.1f", 0.75f), "0.8");
    CHECK_STR(fmt1("%.2f", 0.125f), "0.12");
    CHECK_STR(fmt1("%.0f", 0.4999999f), "0");
    CHECK_STR(fmt1("%.0e", 25.0f), "2e+01");
    CHECK_STR(fmt1("%.0f", 999.5f), "1000");                        // and a carry makes one more digit
    CHECK_STR(fmt1("%.2f", 0.004f), "0.00");
    CHECK_STR(fmt1("%.2f", 0.006f), "0.01");

    TEST_SECTION("%a");
    CHECK_STR(fmt1("%a", 1.0f), "0x1p+0");
    CHECK_STR(fmt1("%a", 0.1f), "0x1.99999ap-4");
    CHECK_STR(fmt1("%A", -12.0f), "-0X1.8P+3");
    CHECK_STR(fmt1("%a", from(1)), "0x0.000002p-126");
    CHECK_STR(fmt1("%a", 0.0f), "0x0p+0");
    CHECK_STR(fmt1("%.1a", 1.96875f), "0x2.0p+0");                  // 0x1.f8 rounded to one digit
    CHECK_STR(fmt1("%.0a", 1.5f), "0x2p+0");                        // a tie, and the lead 1 is odd: up to even
    CHECK_STR(fmt1("%.0a", 2.5f), "0x1p+1");                        // 0x1.4p+1: below the half, down
    CHECK_STR(fmt1("%.8a", 1.5f), "0x1.80000000p+0");
    CHECK_STR(fmt1("%#a", 2.0f), "0x1.p+1");

    TEST_SECTION("strtof is correctly rounded");
    CHECK_EQ(bits(strtof("0.1", 0)), 0x3DCCCCCDu);
    CHECK(strtof("16777217", 0) == 16777216.0f);                    // halfway: to the even one
    CHECK(strtof("16777219", 0) == 16777220.0f);
    CHECK(strtof("16777218.0000000000000000000000001", 0) == 16777218.0f);
    CHECK(strtof("16777217.0000000000000000000000001", 0) == 16777218.0f);   // just past halfway
    CHECK(strtof("3.4028235e38", 0) == FLT_MAX);
    errno = 0;
    CHECK(strtof("3.4028236e38", 0) == INFINITY);                  // past the halfway point to 2^128
    CHECK_EQ(errno, ERANGE);
    CHECK_EQ(bits(strtof("1e-45", 0)), 1u);
    CHECK_EQ(bits(strtof("7.1e-46", 0)), 1u);
    errno = 0;
    CHECK_EQ(bits(strtof("7e-46", 0)), 0u);                         // under half the smallest: zero
    CHECK_EQ(errno, ERANGE);
    CHECK_EQ(bits(strtof("1.17549435e-38", 0)), 0x00800000u);       // FLT_MIN
    char longtext[200];
    strcpy(longtext, "0.");
    for (int i = 0; i < 150; i++) strcat(longtext, "3");
    CHECK(strtof(longtext, 0) == 1.0f / 3.0f);
    CHECK(strtof("1000000000000000000000000000000000000000e-39", 0) == 1.0f);   // 10^39 x 10^-39

    TEST_SECTION("hexadecimal floats");
    char* end = 0;
    CHECK(strtof("0x1.8p3", &end) == 12.0f);
    CHECK_EQ(*end, 0);
    CHECK(strtof("0X.8P1", 0) == 1.0f);
    CHECK_EQ(bits(strtof("0x1p-149", 0)), 1u);
    CHECK(strtof("-0x1.fffffep127", 0) == -FLT_MAX);
    errno = 0;
    CHECK(strtof("0x1p128", 0) == INFINITY);
    CHECK_EQ(errno, ERANGE);
    CHECK(strtof("0x1.000001p0", 0) == 1.0f);                       // halfway, to even
    CHECK(strtof("0x1.000003p0", 0) == from(0x3F800002u));
    CHECK(strtof("0xz", &end) == 0.0f);                             // "0x" alone is the 0
    CHECK_EQ(*end, 'x');
    float scanned = 0.0f;
    CHECK_EQ(sscanf("0x1.4p2 rest", "%a", &scanned), 1);
    CHECK(scanned == 5.0f);
    CHECK_EQ(sscanf("0x z", "%f", &scanned), 0);               // a prefix and no digits: no number

    TEST_SECTION("every float reads back from nine digits");
    int wrong = 0;
    for (int i = 0; i < 300; i++)
    {
        unsigned int u = next();
        if (((u >> 23) & 255u) == 255u) continue;                     // not inf or nan
        char text[32];
        snprintf(text, sizeof text, "%.9g", from(u));
        if (bits(strtof(text, 0)) != u) wrong++;
        char a[32];
        snprintf(a, sizeof a, "%a", from(u));
        if (bits(strtof(a, 0)) != u) wrong++;
    }
    CHECK_EQ(wrong, 0);

    TEST_SECTION("the shortest digits");
    char s[32];
    ftoa_shortest(s, sizeof s, 0.1f);          CHECK_STR(s, "0.1");
    ftoa_shortest(s, sizeof s, 1.0f / 3.0f);   CHECK_STR(s, "0.33333334");
    ftoa_shortest(s, sizeof s, 100.0f);        CHECK_STR(s, "100");
    ftoa_shortest(s, sizeof s, 1e30f);         CHECK_STR(s, "1e+30");
    ftoa_shortest(s, sizeof s, -2.5e-7f);      CHECK_STR(s, "-2.5e-07");
    ftoa_shortest(s, sizeof s, FLT_MIN);       CHECK_STR(s, "1.1754944e-38");
    ftoa_shortest(s, sizeof s, 16777216.0f);   CHECK_STR(s, "16777216");
    ftoa_shortest(s, sizeof s, -0.0f);         CHECK_STR(s, "-0");
    ftoa_shortest(s, sizeof s, INFINITY);      CHECK_STR(s, "inf");
    CHECK_EQ(ftoa_shortest(s, 4, 123.25f), 6);                      // the length it needed; the text is cut
    CHECK_STR(s, "123");
    wrong = 0;
    for (int i = 0; i < 200; i++)
    {
        unsigned int u = next() & 0x7FFFFFFFu;
        if ((u >> 23) == 255u) continue;
        ftoa_shortest(s, sizeof s, from(u));
        if (bits(strtof(s, 0)) != u) wrong++;
    }
    CHECK_EQ(wrong, 0);

    TEST_SECTION("strfromf");
    CHECK_EQ(strfromf(s, sizeof s, "%.3f", 2.0f / 3.0f), 5);
    CHECK_STR(s, "0.667");
    CHECK_EQ(strfromf(s, sizeof s, "%a", 0.5f), 6);
    CHECK_STR(s, "0x1p-1");
    CHECK_EQ(strfromf(s, sizeof s, "%5.1f", 1.0f), -1);             // no width: not a C23 strfromf format
    CHECK_EQ(errno, EINVAL);
    CHECK_EQ(strfromf(s, sizeof s, "%.3", 1.0f), -1);                // no conversion at all
    CHECK_EQ(errno, EINVAL);
    return test_summary();
}
