// A program compiled with -fshort-double (tests/expected/test_short_double.cflags) against the library built
// without it: `double` is a float here, and what the library takes or gives as a double still meets it -
// printf through `...` (a float travels as a binary64 either way), scanf's %lf into a float, strtod, atof,
// difftime, and <math.h>'s standard names, which are the float family here.
#include "ceres/test.h"
#include "stdio.h"
#include "stdlib.h"
#include "math.h"
#include "time.h"
#include "stdarg.h"

// A variadic function of the program's own: va_arg of a `double` reads the binary64 and rounds it back.
static double sum(int n, ...)
{
    va_list ap;
    va_start(ap, n);
    double total = 0.0;
    for (int i = 0; i < n; i++)
        total += va_arg(ap, double);
    va_end(ap);
    return total;
}

int main(void)
{
    TEST_SECTION("double is a float");
    CHECK(sizeof(double) == 4 && sizeof(1.5) == 4 && sizeof(long double) == 4);
    CHECK(sum(3, 1.5, 2.25, 0.25f) == 4.0);

    TEST_SECTION("printf");
    char buf[64];
    double x = 1.5;
    snprintf(buf, sizeof buf, "%.3f %g %e", x, 0.1, -2.0);
    CHECK_STR(buf, "1.500 0.1 -2.000000e+00");
    snprintf(buf, sizeof buf, "%d %.2f %d", 7, 0.25, 9);        // the words after a float still line up
    CHECK_STR(buf, "7 0.25 9");

    TEST_SECTION("scanf");
    struct { double d; unsigned int canary; } z = { 0.0, 0xC0FFEEu };
    CHECK_EQ(sscanf("7.25", "%lf", &z.d), 1);                    // four bytes stored, not eight
    CHECK(z.d == 7.25 && z.canary == 0xC0FFEEu);
    double a = 0.0, b = 0.0;
    CHECK_EQ(sscanf("2.5 0.1", "%lf %Lf", &a, &b), 2);
    CHECK(a == 2.5 && b == 0.1);

    TEST_SECTION("strtod, atof, difftime");
    char* end;
    double v = strtod("3.25x", &end);
    CHECK(v == 3.25 && *end == 'x');
    CHECK(atof("-0.5") == -0.5 && strtold("1e3", 0) == 1000.0);
    CHECK(difftime(10, 4) == 6.0);

    TEST_SECTION("math");
    CHECK(sqrt(2.0) == sqrtf(2.0f) && sin(0.5) == sinf(0.5f) && pow(2.0, 10.0) == 1024.0);
    CHECK(fabs(-1.5) == 1.5 && floor(-2.5) == -3.0 && isnan(sqrt(-1.0)));
    return test_summary();
}
