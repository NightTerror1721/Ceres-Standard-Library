// sscanf and friends: every conversion, width, suppression, the return value, and where a scan stops.
#include "ceres/test.h"
#include "limits.h"
#include "math.h"
#include "string.h"

static int via_vsscanf(const char* text, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsscanf(text, fmt, ap);
    va_end(ap);
    return r;
}

int main(void)
{
    int a = -1, b = -1, c = -1;
    unsigned int u = 0, u2 = 0;
    float f = 0.0f, g = 0.0f, h = 0.0f;
    char s1[32], s2[32];

    TEST_SECTION("integers");
    CHECK_EQ(sscanf("12 -34", "%d %d", &a, &b), 2);
    CHECK(a == 12 && b == -34);
    CHECK_EQ(sscanf("+7", "%d", &a), 1);
    CHECK_EQ(a, 7);
    CHECK_EQ(sscanf("ff 1A 17", "%x %X %o", &u, &u2, &c), 3);
    CHECK(u == 255u && u2 == 26u && c == 15);
    CHECK_EQ(sscanf("0xff", "%x", &u), 1);                       // %x accepts its own prefix
    CHECK_EQ((int)u, 255);
    CHECK_EQ(sscanf("0x1f 017 42 -0x10", "%i %i %i %i", &a, &b, &c, &u), 4);
    CHECK(a == 31 && b == 15 && c == 42 && (int)u == -16);       // %i reads the base off the prefix
    CHECK_EQ(sscanf("4294967295", "%u", &u), 1);
    CHECK(u == UINT_MAX);
    CHECK_EQ(sscanf("2147483647 -2147483648", "%d %d", &a, &b), 2);
    CHECK(a == INT_MAX && b == INT_MIN);
    CHECK_EQ(sscanf("\n\t  \r 9", "%d", &a), 1);                 // leading white space of any kind
    CHECK_EQ(a, 9);
    CHECK_EQ(sscanf("2026-09-20", "%d-%d-%d", &a, &b, &c), 3);
    CHECK(a == 2026 && b == 9 && c == 20);

    TEST_SECTION("sizes");
    short sh = 0;
    signed char sc = 0;
    unsigned short ush = 0;
    unsigned char uc = 0;
    CHECK_EQ(sscanf("70000 300", "%hd %hhd", &sh, &sc), 2);
    CHECK_EQ(sh, 4464);                                          // 70000 wrapped to 16 bits
    CHECK_EQ(sc, 44);                                            // and 300 to 8
    CHECK_EQ(sscanf("65535 255", "%hu %hhu", &ush, &uc), 2);
    CHECK(ush == 65535 && uc == 255);
    long long wide = 0;                                          // %lld stores eight bytes: an int would be overrun
    CHECK_EQ(sscanf("5 6 7", "%ld %lu %lld", &a, &u, &wide), 3);
    CHECK(a == 5 && u == 6u && wide == 7);
    CHECK_EQ(sscanf("-5000000000 12345678901", "%jd %qd", &wide, &wide), 2);   // j and q are 64 bits too
    CHECK(wide == 12345678901LL);
    CHECK_EQ(sscanf("-5000000000", "%jd", &wide), 1);
    CHECK(wide == -5000000000LL);
    CHECK_EQ(sscanf("1 2", "%zd %td", &a, &b), 2);
    CHECK(a == 1 && b == 2);

    TEST_SECTION("floating point");
    CHECK_EQ(sscanf("3.5 -2e2 .25", "%f %f %f", &f, &g, &h), 3);
    CHECK(f == 3.5f && g == -200.0f && h == 0.25f);
    CHECK_EQ(sscanf("1.5e-1 2E+2 7.", "%e %E %g", &f, &g, &h), 3);
    CHECK(f == 0.15f && g == 200.0f && h == 7.0f);
    CHECK_EQ(sscanf("2.5", "%lf", &f), 1);                       // double is float here
    CHECK(f == 2.5f);
    CHECK_EQ(sscanf("inf -inf nan", "%f %f %f", &f, &g, &h), 3);
    CHECK(isinf(f) && f > 0.0f && isinf(g) && g < 0.0f && isnan(h));
    CHECK_EQ(sscanf("12abc", "%f", &f), 1);                      // stops at the first character that is not part of it
    CHECK(f == 12.0f);
    CHECK_EQ(sscanf("5", "%f", &f), 1);                          // an integer is a float
    CHECK(f == 5.0f);
    CHECK_EQ(sscanf("x", "%f", &f), 0);
    CHECK_EQ(sscanf("1.5.5", "%f", &f), 1);
    CHECK(f == 1.5f);

    TEST_SECTION("strings and characters");
    CHECK_EQ(sscanf("hello world", "%s %s", s1, s2), 2);
    CHECK_STR(s1, "hello");
    CHECK_STR(s2, "world");
    CHECK_EQ(sscanf("abcdef", "%3s%s", s1, s2), 2);              // a width cuts a %s
    CHECK_STR(s1, "abc");
    CHECK_STR(s2, "def");
    char x1, x2, x3;
    CHECK_EQ(sscanf("abc", "%c%c%c", &x1, &x2, &x3), 3);
    CHECK(x1 == 'a' && x2 == 'b' && x3 == 'c');
    CHECK_EQ(sscanf(" x", "%c", &x1), 1);
    CHECK_EQ(x1, ' ');                                           // %c does not skip white space
    CHECK_EQ(sscanf(" x", " %c", &x1), 1);                       // but a space in the format does
    CHECK_EQ(x1, 'x');
    char four[5];
    CHECK_EQ(sscanf("wxyz!", "%4c", four), 1);                   // %4c is exactly four characters, no NUL
    CHECK(four[0] == 'w' && four[3] == 'z');

    TEST_SECTION("scansets");
    CHECK_EQ(sscanf("abc123", "%[a-z]%d", s1, &a), 2);
    CHECK_STR(s1, "abc");
    CHECK_EQ(a, 123);
    CHECK_EQ(sscanf("one,two", "%[^,],%s", s1, s2), 2);
    CHECK_STR(s1, "one");
    CHECK_STR(s2, "two");
    CHECK_EQ(sscanf("]a]x", "%[]a]", s1), 1);                    // a ] first is a member
    CHECK_STR(s1, "]a]");
    CHECK_EQ(sscanf("  spaced", "%[a-z]", s1), 0);               // %[ does not skip white space: nothing matches
    CHECK_EQ(sscanf("a-b-c", "%[a-c-]", s1), 1);                 // a trailing - is itself
    CHECK_STR(s1, "a-b-c");
    CHECK_EQ(sscanf("aaab", "%2[a]", s1), 1);                    // width applies
    CHECK_STR(s1, "aa");
    CHECK_EQ(sscanf("Hello World", "%[^\n]", s1), 1);
    CHECK_STR(s1, "Hello World");

    TEST_SECTION("literals and white space");
    CHECK_EQ(sscanf("1,2", "%d,%d", &a, &b), 2);
    CHECK_EQ(sscanf("1;2", "%d,%d", &a, &b), 1);                 // the first mismatch ends the scan
    CHECK_EQ(a, 1);
    CHECK_EQ(sscanf("x=42", "x=%d", &a), 1);
    CHECK_EQ(a, 42);
    CHECK_EQ(sscanf("y=42", "x=%d", &a), 0);
    CHECK_EQ(sscanf("1\n\t 2", "%d %d", &a, &b), 2);             // one space in the format matches any run
    CHECK_EQ(sscanf("12", "%d %d", &a, &b), 1);                  // the input ended after the first
    CHECK_EQ(sscanf("50%", "%d%%", &a), 1);
    CHECK_EQ(a, 50);
    CHECK_EQ(sscanf("50", "%d%%", &a), 1);
    CHECK_EQ(sscanf("1 x 3", "%d %d %d", &a, &b, &c), 1);        // "x" is not a number
    CHECK_EQ(sscanf("foo 7", "foo %d", &a), 1);
    CHECK_EQ(sscanf("7", "foo %d", &a), 0);

    TEST_SECTION("widths");
    CHECK_EQ(sscanf("1234567", "%5d", &a), 1);
    CHECK_EQ(a, 12345);
    CHECK_EQ(sscanf("1234567", "%3d%2d%d", &a, &b, &c), 3);
    CHECK(a == 123 && b == 45 && c == 67);
    CHECK_EQ(sscanf("-12", "%2d", &a), 1);                       // the sign counts toward the width
    CHECK_EQ(a, -1);
    CHECK_EQ(sscanf("0x1f", "%3x", &u), 1);                      // "0x1" : the prefix counts too
    CHECK_EQ((int)u, 1);
    CHECK_EQ(sscanf("3.14159", "%4f", &f), 1);
    CHECK(f == 3.14f);

    TEST_SECTION("suppression, %n and %p");
    CHECK_EQ(sscanf("1 2", "%*d %d", &a), 1);
    CHECK_EQ(a, 2);
    CHECK_EQ(sscanf("skip this", "%*s %s", s1), 1);
    CHECK_STR(s1, "this");
    CHECK_EQ(sscanf("12 34", "%d %d%n", &a, &b, &c), 2);         // %n stores a count but is not counted
    CHECK_EQ(c, 5);
    CHECK_EQ(sscanf("ab", "a%nb", &c), 0);
    CHECK_EQ(c, 1);
    CHECK_EQ(sscanf("%%", "%%%%"), 0);                           // nothing to assign, and it matched
    void* p = 0;
    CHECK_EQ(sscanf("0x1000", "%p", &p), 1);
    CHECK_EQ((int)p, 0x1000);

    TEST_SECTION("the end of the input");
    a = 77;
    CHECK_EQ(sscanf("", "%d", &a), -1);                          // EOF before the first conversion
    CHECK_EQ(a, 77);
    CHECK_EQ(sscanf("   ", "%d", &a), -1);
    CHECK_EQ(sscanf("", "%s", s1), -1);
    CHECK_EQ(sscanf("", "%c", &x1), -1);
    CHECK_EQ(sscanf("", "abc"), -1);                             // and even before a literal
    CHECK_EQ(sscanf("", ""), 0);                                 // an empty format converts nothing
    CHECK_EQ(sscanf("5", "%d %d", &a, &b), 1);                   // but EOF after one conversion returns the count

    TEST_SECTION("vsscanf");
    CHECK_EQ(via_vsscanf("7 8 nine", "%d %d %s", &a, &b, s1), 3);
    CHECK(a == 7 && b == 8);
    CHECK_STR(s1, "nine");
    return test_summary();
}
