// printf and its relatives: every conversion, flag, width and precision against the text C prints,
// the snprintf contract, output to the real terminal (including runs longer than its 64-byte chunk),
// and the small putint/puthex/... printers.
#include "ceres/test.h"
#include "stdlib.h"
#include "inttypes.h"
#include "limits.h"
#include "math.h"
#include "string.h"

static char buf[160];

// `head`, then `zeros` zero digits, then `tail`: the text of a long precision, built in its own buffer.
static char want[160];
static const char* zeros_between(const char* head, int zeros, const char* tail)
{
    strcpy(want, head);
    int n = (int)strlen(want);
    for (int i = 0; i < zeros; i++) want[n + i] = '0';
    want[n + zeros] = 0;
    strcat(want, tail);
    return want;
}

// Wrappers that forward their `...`, the way a user's own logging function would.
static int via_vsnprintf(char* out, size_t n, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(out, n, fmt, ap);
    va_end(ap);
    return r;
}

static int via_vsprintf(char* out, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsprintf(out, fmt, ap);
    va_end(ap);
    return r;
}

static int via_vprintf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}
#define FMT(expected, ...) \
    do { snprintf(buf, sizeof(buf), __VA_ARGS__); CHECK_STR(buf, expected); } while (0)

int main(void)
{
    TEST_SECTION("integers: flags, width, precision");
    FMT("42|   42|42   |00042|+42| 42", "%d|%5d|%-5d|%05d|%+d|% d", 42, 42, 42, 42, 42, 42);
    FMT("ff FF 0xff 0XFF 10 010", "%x %X %#x %#X %o %#o", 255, 255, 255, 255, 8, 8);
    FMT("4294967295", "%u", -1);
    FMT("-2147483648|2147483647", "%d|%i", INT_MIN, INT_MAX);
    FMT("00042|     042", "%.5d|%8.3d", 42, 42);
    FMT("   -3|+0003", "%+5d|%+05d", -3, 3);
    FMT("0|0||", "%x|%#x|%.0d|", 0, 0, 0);            // "%#x" of 0 has no prefix; "%.0d" of 0 prints nothing
    FMT("5|", "%.0d|%.0d", 5, 0);
    FMT("44 4464 5 7", "%hhd %hd %ld %zu", 300, 70000, 5, 7);
    FMT("-5000000000 12345678901", "%jd %qd", (intmax_t)-5000000000LL, 12345678901LL);   // j and q are 64 bits
    FMT("-5000000000|18446744073709551615", "%" PRIdMAX "|%" PRIuMAX, (intmax_t)-5000000000LL, (uintmax_t)-1);
    FMT("101 00000101", "%b %08b", 5, 5);
    FMT("   42|42   ", "%*d|%-*d", 5, 42, 5, 42);
    FMT("42   |", "%*d|", -5, 42);                    // a negative * width means left-justify
    FMT("42   ", "%-05d", 42);                        // '-' beats '0'
    FMT("-0042|+005| 0042", "%05d|%+.3d|% 05d", -42, 5, 42);
    FMT(" -007", "%5.3d", -7);
    FMT("777 DEADBEEF", "%o %X", 511, 0xDEADBEEFu);
    FMT("      00ab|0xff    |0x000000ff", "%10.4x|%-#8x|%#010x", 0xab, 255, 255);
    FMT("0|0", "%#o|%#x", 0, 0);
    FMT("-1 255", "%hhd %hhu", 255, 255);
    FMT("65535 -1", "%hu %hd", 65535, 65535);

    TEST_SECTION("text");
    FMT("ok!!", "%c%c%s", 'o', 'k', "!!");
    FMT("       abc|abc       |abc", "%10s|%-10s|%.3s", "abc", "abc", "abcdef");
    FMT("    x|y  |", "%5c|%-3c|", 'x', 'y');
    FMT("100%", "%d%%", 100);
    FMT("(null)", "%s", (char*)0);
    FMT("||   ab", "%s|%.0s|%5.2s", "", "abc", "abcdef");
    FMT("0xff|0x0|  0x10", "%p|%p|%6p", (void*)255, (void*)0, (void*)16);
    // Formats ceresc would warn about when written as literals (W3005): passed through a variable,
    // they show what the engine does with them at run time.
    const char* odd = "%y";
    FMT("%y", odd);                                    // an unknown conversion is printed back
    odd = "abc%";
    FMT("abc", odd);                                   // a lone % at the end prints nothing

    TEST_SECTION("floats: %f");
    FMT("3.141590 -2.500000 0.000000", "%f %f %f", 3.14159f, -2.5f, 0.0f);
    FMT("3.14|   3.142|1.3     |+2.5", "%.2f|%8.3f|%-8.1f|%+.1f", 3.14159f, 3.14159f, 1.26f, 2.46f);
    FMT("3.14", "%.*f", 2, 3.14159f);
    FMT("10000000000.000000", "%f", 1.0e10f);
    FMT("1.000000|10", "%f|%.0f", 0.9999999f, 9.6f);   // rounding carries into the integer part
    FMT("0.100000 123456.000000 0.000001", "%f %f %f", 0.1f, 123456.0f, 0.000001f);
    FMT("+1.500000| 1.500000|-000003.14", "%+f|% f|%010.2f", 1.5f, 1.5f, -3.14159f);
    FMT("3.|3", "%#.0f|%.0f", 3.0f, 3.0f);
    FMT("-0.000000", "%f", -0.0f);
    FMT("    99.9|4294967296", "%8.1f|%.0f", 99.95f, 4294967296.0f);
    FMT("-3|0.5", "%.0f|%.1f", -2.6f, 0.5f);
    // From 2^32 up a float is a whole number with exactly-known digits, and C prints them all.
    FMT("100000002004087734272.000000", "%f", 1.0e20f);
    FMT("340282346638528859811704183484516925440", "%.0f", 3.4028235e38f);   // FLT_MAX
    FMT("4294967296 4294967808 8589934592", "%.0f %.0f %.0f", 4294967296.0f, 4294967808.0f, 8589934592.0f);
    FMT("16777216.000000 16777218.000000", "%f %f", 16777216.0f, 16777218.0f);

    TEST_SECTION("floats: %e and %g");
    FMT("1.234568e+04", "%e", 12345.678f);
    FMT("1.5E-03|0.000000e+00", "%.1E|%e", 0.0015f, 0.0f);
    FMT("1.23e-04", "%.2e", 0.000123f);
    FMT("100000 1e+06 0.0001 1e-05", "%g %g %g %g", 100000.0f, 1000000.0f, 0.0001f, 0.00001f);
    FMT("0.5 100 1.23457e+08", "%g %g %g", 0.5f, 100.0f, 123456789.0f);
    FMT("0|1e-05|1E-05", "%g|%g|%G", 0.0f, 0.00001f, 0.00001f);
    FMT("2.5|3.14159", "%g|%g", 2.5f, 3.14159f);
    FMT("1.50000", "%#g", 1.5f);

    TEST_SECTION("floats: precision past the digits a float has");
    // A float yields at most 38 significant digits here; a longer precision is completed with zeros.
    // These used to read past the digit buffer and overrun the text one. 2.5, 1 and 0.5 have exact digits.
    FMT(zeros_between("2.5", 44, "e+00"), "%.45e", 2.5f);
    FMT(zeros_between("1.", 100, "e+00"), "%.100e", 1.0f);
    FMT(zeros_between("0.", 100, "e+00"), "%.100e", 0.0f);
    FMT(zeros_between("  1.", 100, "E+00"), "%108.100E", 1.0f);
    FMT(zeros_between("2.5", 43, ""), "%#.45g", 2.5f);
    FMT(zeros_between("1.", 99, ""), "%#.100g", 1.0f);
    FMT("2.5|1", "%.45g|%.100g", 2.5f, 1.0f);           // without # the zeros are stripped
    FMT(zeros_between("0.5", 89, ""), "%.90f", 0.5f);   // used to stop at 78 characters
    FMT(zeros_between("-0.5", 89, "|"), "%.90f|", -0.5f);

    TEST_SECTION("floats: inf and nan");

    FMT("inf -inf nan INF", "%f %f %f %F", INFINITY, -INFINITY, NAN, INFINITY);
    FMT("  inf|inf   |  inf", "%5.1f|%-6f|%05f", INFINITY, INFINITY, INFINITY);
    FMT("inf|nan|+inf", "%e|%g|%+f", INFINITY, NAN, INFINITY);

    TEST_SECTION("snprintf and sprintf");
    int r = snprintf(buf, 5, "%s", "abcdefgh");
    CHECK_EQ(r, 8);                                    // what WOULD have been written
    CHECK_STR(buf, "abcd");
    r = snprintf(buf, 6, "%s", "abcde");
    CHECK_EQ(r, 5);
    CHECK_STR(buf, "abcde");                           // fits exactly
    r = snprintf(buf, 5, "%s", "abcde");
    CHECK_EQ(r, 5);
    CHECK_STR(buf, "abcd");                            // one short: cut, still terminated
    buf[0] = 'X';
    r = snprintf(buf, 0, "%d", 12345);
    CHECK_EQ(r, 5);
    CHECK_EQ(buf[0], 'X');                             // n == 0 writes nothing at all
    r = snprintf(buf, 1, "abc");
    CHECK_EQ(r, 3);
    CHECK_EQ(buf[0], 0);
    int n = 0;
    r = sprintf(buf, "abc%nde", &n);
    CHECK_EQ(n, 3);                                    // %n reports the characters so far
    CHECK_EQ(r, 5);
    CHECK_STR(buf, "abcde");
    r = sprintf(buf, "%s-%d-%c", "x", -7, 'z');
    CHECK_EQ(r, 6);
    CHECK_STR(buf, "x--7-z");
    r = sprintf(buf, "");
    CHECK_EQ(r, 0);
    CHECK_STR(buf, "");
    FMT("4294967295 beef -2147483648", "%" PRIu32 " %" PRIx32 " %" PRId32, UINT32_MAX, 0xBEEFu, INT32_MIN);
    char small[8];
    memset(small, 'Z', sizeof(small));
    snprintf(small, 4, "%d", 123456);
    CHECK(small[3] == 0 && small[4] == 'Z');           // nothing written past the limit

    TEST_SECTION("the v forms");
    char text[32];
    CHECK_EQ(via_vsnprintf(text, sizeof(text), "%d-%s-%.2f", 7, "x", 1.5f), 8);
    CHECK_STR(text, "7-x-1.50");
    CHECK_EQ(via_vsprintf(text, "[%5s]", "ab"), 7);
    CHECK_STR(text, "[   ab]");
    CHECK_EQ(via_vsnprintf(text, 4, "%s", "abcdef"), 6);
    CHECK_STR(text, "abc");

    TEST_SECTION("ftoa");
    char fb[40];
    CHECK_STR(ftoa(3.14159f, fb, 3), "3.142");
    CHECK_STR(ftoa(-0.5f, fb, 1), "-0.5");
    CHECK_STR(ftoa(1.0f, fb, 20), "1.000000000");      // decimals are capped at 9
    CHECK_STR(ftoa(7.9f, fb, -1), "8");
    CHECK(ftoa(1.0f, fb, 2) == fb);

    TEST_SECTION("to the terminal");
    int rc = printf("hello %d\n", 42);
    CHECK_EQ(rc, 9);
    putchar('a');
    printf("b");
    putstr("c");
    rc = printf("d\n");
    CHECK_EQ(rc, 2);                                   // "abcd": every path keeps its order
    static char xs[320];
    memset(xs, 'x', sizeof(xs));
    static const int lengths[8] = { 1, 63, 64, 65, 127, 128, 129, 300 };
    for (int i = 0; i < 8; i++)
    {
        rc = printf("%.*s|\n", lengths[i], xs);        // around the 64-byte chunk boundaries
        CHECK_EQ(rc, lengths[i] + 2);
    }
    rc = printf("%-70s|%70s|\n", "left", "right");
    CHECK_EQ(rc, 143);
    rc = via_vprintf("%s=%d\n", "answer", 42);
    CHECK_EQ(rc, 10);

    TEST_SECTION("putint, putuint, puthex, putbin, putfloat");
    CHECK_EQ(putint(0), 1); putchar('\n');
    CHECK_EQ(putint(-2147483647 - 1), 11); putchar('\n');
    CHECK_EQ(putint(2147483647), 10); putchar('\n');
    CHECK_EQ(putint(-7), 2); putchar('\n');
    CHECK_EQ(putuint(4294967295u), 10); putchar('\n');
    CHECK_EQ(putuint(0), 1); putchar('\n');
    CHECK_EQ(puthex(0), 3); putchar('\n');
    CHECK_EQ(puthex(255), 4); putchar('\n');
    CHECK_EQ(puthex(0xDEADBEEFu), 10); putchar('\n');
    CHECK_EQ(putbin(5, 8), 8); putchar('\n');
    CHECK_EQ(putbin(5, 3), 3); putchar('\n');
    CHECK_EQ(putbin(0xFFFFFFFFu, 40), 32); putchar('\n');   // more than 32 bits is capped
    CHECK_EQ(putbin(1, 0), 1); putchar('\n');               // fewer than 1 is raised
    CHECK_EQ(putfloat(3.14159f, 2), 4); putchar('\n');
    CHECK_EQ(putfloat(-2.6f, 0), 2); putchar('\n');
    CHECK_EQ(putfloat(10000000000.0f, 1), 13); putchar('\n');
    CHECK_EQ(putfloat(0.5f, 12), 11); putchar('\n');         // decimals are capped at 9

    TEST_SECTION("input");
    CHECK_EQ(getchar_nb(), -1);                        // nothing was typed
    CHECK_EQ(EOF, -1);
    return test_summary();
}
