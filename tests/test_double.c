// -fsoft-double: double is a real binary64 (tests/expected/test_double.flags), every operation a call to
// ceres/f64.h. What the compiler lowers - literals, arithmetic, comparisons, conversions, increments, compound
// assignment, conditions, arguments and returns, globals, arrays and struct fields, variadic arguments - checked
// against the bits a binary64 must have.
#include "ceres/test.h"
#include "stdio.h"
#include "string.h"
#include "stdbool.h"
#include "stdlib.h"
#include "float.h"
#include "errno.h"

static unsigned long long bits(double d)
{
    unsigned long long b;
    memcpy(&b, &d, 8);
    return b;
}

static double twice(double x) { return x * 2; }
static double third(void) { return 1.0 / 3; }

double global_pi = 3.141592653589793;
double global_sum = 0.1 + 0.2;
double table[3] = { 1.5, -2.25, 1e300 };
struct point { int id; double x, y; };
struct point origin = { 7, 0.5, -0.5 };

static long long sum_variadic(int n, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, n);
    double total = 0;
    for (int i = 0; i < n; i++)
        total += __builtin_va_arg(ap, double);
    __builtin_va_end(ap);
    return (long long)(total * 1000);
}

int main(void)
{
    TEST_SECTION("literals and sizes");
    CHECK_EQ((int)sizeof(double), 8);
    CHECK(bits(1.0) == 0x3FF0000000000000ull);
    CHECK(bits(0.1) == 0x3FB999999999999Aull);          // not the float's 0.1
    CHECK(bits(-2.5) == 0xC004000000000000ull);
    CHECK(bits(1e300) == 0x7E37E43C8800759Cull);

    TEST_SECTION("arithmetic");
    double a = 0.1, b = 0.2;
    CHECK(bits(a + b) == 0x3FD3333333333334ull);         // 0.30000000000000004
    CHECK(bits(a * b) == 0x3F947AE147AE147Cull);
    CHECK(bits(b - a) == 0x3FB999999999999Aull);
    CHECK(bits(third()) == 0x3FD5555555555555ull);
    CHECK(bits(twice(a)) == bits(b));
    CHECK(bits(-a) == 0xBFB999999999999Aull);
    double c = a;
    c += b;
    c *= 10;
    CHECK(bits(c) == 0x4008000000000001ull);             // 3.0000000000000004
    c = 1;
    c++;
    ++c;
    c--;
    CHECK(c == 2.0);
    CHECK(bits(global_sum) == 0x3FD3333333333334ull);    // folded at compile time, the same bits
    CHECK(bits(global_pi) == 0x400921FB54442D18ull);

    TEST_SECTION("comparisons and conditions");
    CHECK(a < b && b > a && a <= a && a >= a && a != b && !(a == b));
    double zero = 0.0, nan = zero / zero;
    CHECK(!(nan == nan) && nan != nan && !(nan < 1) && !(nan > 1));
    CHECK(nan ? 1 : 0);                                  // a NaN is true
    CHECK(!(zero ? 1 : 0) && !(-zero ? 1 : 0));
    int loops = 0;
    for (double x = 0; x < 1; x += 0.25)
        loops++;
    CHECK_EQ(loops, 4);

    TEST_SECTION("conversions");
    int i = 7;
    long long big = 9007199254740993LL;                  // 2^53 + 1: rounds to even
    float f = 0.1f;
    CHECK(bits(i) == 0x401C000000000000ull);
    CHECK(bits((double)big) == 0x4340000000000000ull);
    CHECK(bits(f) == 0x3FB99999A0000000ull);             // a float widens exactly
    CHECK((float)a == 0.1f);
    CHECK_EQ((int)2.99, 2);
    CHECK_EQ((int)-2.99, -2);
    CHECK((long long)1e18 == 1000000000000000000LL);
    CHECK_EQ((unsigned int)4000000000.0, 4000000000u);
    char ch = 65.7;
    CHECK_EQ(ch, 'A');
    bool yes = 0.5;
    CHECK(yes);
    double from_u = 4000000000u;
    CHECK(bits(from_u) == 0x41EDCD6500000000ull);
    float back = a * 3;                                  // a double expression into a float
    CHECK(back == 0.3f);

    TEST_SECTION("memory");
    CHECK(bits(table[2]) == 0x7E37E43C8800759Cull && table[1] == -2.25);
    table[0] = table[0] * 2;
    CHECK(table[0] == 3.0);
    CHECK(origin.id == 7 && origin.x == 0.5 && origin.y == -0.5);
    struct point p = { 1, 2.5, 3.5 };
    p.x += p.y;
    CHECK(p.x == 6.0);
    double arr[4];
    for (int k = 0; k < 4; k++)
        arr[k] = k / 4.0;
    CHECK(arr[3] == 0.75);

    TEST_SECTION("variadic");
    CHECK_EQ((int)sum_variadic(3, 0.5, 0.25, 1.0f), 1750);   // the float is promoted to double

    TEST_SECTION("printf, whole digits");
    char out[64];
    snprintf(out, sizeof out, "%.17g", 0.1);
    CHECK_STR(out, "0.10000000000000001");
    snprintf(out, sizeof out, "%.15g", 1.0 / 3);
    CHECK_STR(out, "0.333333333333333");
    snprintf(out, sizeof out, "%f", 1e22);
    CHECK_STR(out, "10000000000000000000000.000000");
    snprintf(out, sizeof out, "%e", 4.9406564584124654e-324);
    CHECK_STR(out, "4.940656e-324");
    snprintf(out, sizeof out, "%a", 1.0 / 3);
    CHECK_STR(out, "0x1.5555555555555p-2");
    snprintf(out, sizeof out, "%g", DBL_MAX);
    CHECK_STR(out, "1.79769e+308");
    snprintf(out, sizeof out, "%.0f %.1f %.3e", 2.5, 0.25, 1234.5);
    CHECK_STR(out, "2 0.2 1.234e+03");                       // ties to even, on the exact value
    snprintf(out, sizeof out, "%f %g", 0.1f, -0.0);
    CHECK_STR(out, "0.100000 -0");                           // a float promoted to double prints the same
    snprintf(out, sizeof out, "%.20f", 0.1);
    CHECK_STR(out, "0.10000000000000000555");
    printf("printed: %.17g %g %a\n", 3.141592653589793, 1e100, -2.5);

    TEST_SECTION("strtod and scanf");
    char* end;
    CHECK(bits(strtod("0.1", &end)) == 0x3FB999999999999Aull && *end == 0);
    CHECK(bits(strtod("2.2250738585072011e-308", 0)) == 0x000FFFFFFFFFFFFFull);   // the famous hard case
    CHECK(bits(strtod("0x1.8p1", 0)) == 0x4008000000000000ull);
    CHECK(bits(strtod("4.9e-324", 0)) == 1u);
    CHECK(bits(strtod("-inf", 0)) == 0xFFF0000000000000ull);
    errno = 0;
    CHECK(bits(strtod("1e309", 0)) == 0x7FF0000000000000ull);
    CHECK_EQ(errno, ERANGE);
    errno = 0;
    CHECK(strtod("1e-400", 0) == 0.0);
    CHECK_EQ(errno, ERANGE);
    CHECK(bits(atof("123456789012345678")) == 0x437B69B4BA630F35ull);
    double d = 0;
    float f2 = 0;
    CHECK_EQ(sscanf("3.25 1.5", "%lf %f", &d, &f2), 2);
    CHECK(d == 3.25 && f2 == 1.5f);
    long double ld = 0;
    CHECK_EQ(sscanf("0.1", "%Lf", &ld), 1);                     // long double is a double here: all eight bytes
    CHECK(bits(ld) == 0x3FB999999999999Aull);
    snprintf(out, sizeof out, "%.0a %.0a", 1.5, 3.5);
    CHECK_STR(out, "0x2p+0 0x2p+1");                             // ties to even on the lead digit
    CHECK(bits(DBL_EPSILON) == 0x3CB0000000000000ull && DBL_MANT_DIG == 53);
    return test_summary();
}
