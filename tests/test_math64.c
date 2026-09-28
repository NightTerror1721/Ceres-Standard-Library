// <math.h> in double: every function against node's Math.* on the same inputs (tests/math64_tables.inc, bit
// patterns from tools/gen_math64_tables.js), erf, erfc, tgamma and lgamma against values worked out to 100 digits,
// then the special values, errno and the identities a table cannot show.
//
// The distance is counted in units in the last place. node's functions are within about an ulp themselves, so a
// tolerance is ours plus one.
#include "ceres/test.h"
#include "math.h"
#include "errno.h"
#include "float.h"
#include "stdio.h"

#include "math64_tables.inc"
#include "math64_special_refs.inc"

typedef unsigned long long u64;

static double d_of(u64 b)
{
    union { u64 u; double d; } v;
    v.u = b;
    return v.d;
}

static u64 bits_of(double d)
{
    union { u64 u; double d; } v;
    v.d = d;
    return v.u;
}

// How many doubles lie between got and want (0 when they are the same double, or both NaN).
static double ulps(double got, double want)
{
    if (isnan(got) || isnan(want))
        return (isnan(got) && isnan(want)) ? 0.0 : 1e30;
    if (got == want)
        return 0.0;
    if (isinf(got) || isinf(want))
        return 1e30;
    // Doubles ordered as integers: the sign-magnitude bits turned into a line.
    long long a = (long long)bits_of(got);
    long long b = (long long)bits_of(want);
    if (a < 0) a = (long long)0x8000000000000000ull - a;
    if (b < 0) b = (long long)0x8000000000000000ull - b;
    double d = (double)(a - b);
    return d < 0 ? -d : d;
}

static int shown;

static void check_1(const char* name, double (*fn)(double), const u64 (*t)[2], int n, double tol)
{
    double worst = 0.0;
    for (int i = 0; i < n; i++)
    {
        double x = d_of(t[i][0]);
        double got = fn(x);
        double e = ulps(got, d_of(t[i][1]));
        if (e > worst)
            worst = e;
        if (e > tol && shown++ < 12)
            printf("  %s(%.17g) = %.17g, node %.17g: %.0f ulps\n", name, x, got, d_of(t[i][1]), e);
    }
    CHECK(worst <= tol);
}

static void check_2(const char* name, double (*fn)(double, double), const u64 (*t)[3], int n, double tol)
{
    double worst = 0.0;
    for (int i = 0; i < n; i++)
    {
        double a = d_of(t[i][0]), b = d_of(t[i][1]);
        double got = fn(a, b);
        double e = ulps(got, d_of(t[i][2]));
        if (e > worst)
            worst = e;
        if (e > tol && shown++ < 12)
            printf("  %s(%.17g, %.17g) = %.17g, node %.17g: %.0f ulps\n", name, a, b, got, d_of(t[i][2]), e);
    }
    CHECK(worst <= tol);
}

#define TABLE_1(fn, tol) check_1(#fn, (fn), t64_##fn, (int)(sizeof(t64_##fn) / sizeof(t64_##fn[0])), tol)
#define TABLE_2(fn, tol) check_2(#fn, (fn), t64_##fn, (int)(sizeof(t64_##fn) / sizeof(t64_##fn[0])), tol)

static double special(const char* name, double x)
{
    if (name[0] == 'e' && name[3] == 0) return erf(x);
    if (name[0] == 'e') return erfc(x);
    if (name[0] == 't') return tgamma(x);
    return lgamma(x);
}

#define CHECK_ERRNO(expr, code) \
    do { errno = 0; (void)(expr); __t_total++; \
         if (errno != (code)) { __t_failed++; printf("FAIL %s:%d: errno after %s is %d, wanted %d\n", __FILE__, __LINE__, #expr, errno, (code)); } } while (0)

int main(void)
{
    TEST_SECTION("tables: trigonometric");
    TABLE_1(sin, 2);
    TABLE_1(cos, 2);
    TABLE_1(tan, 4);
    TABLE_1(asin, 2);
    TABLE_1(acos, 2);
    TABLE_1(atan, 2);
    TABLE_2(atan2, 3);

    TEST_SECTION("tables: exponential and logarithmic");
    TABLE_1(exp, 2);
    TABLE_1(exp2, 2);
    TABLE_1(expm1, 2);
    TABLE_1(log, 2);
    TABLE_1(log2, 2);
    TABLE_1(log10, 2);
    TABLE_1(log1p, 2);
    TABLE_2(pow, 2);

    TEST_SECTION("tables: hyperbolic and the rest");
    TABLE_1(sinh, 3);
    TABLE_1(cosh, 3);
    TABLE_1(tanh, 4);
    TABLE_1(asinh, 3);
    TABLE_1(acosh, 3);
    TABLE_1(atanh, 3);
    TABLE_1(cbrt, 2);
    TABLE_2(hypot, 2);

    TEST_SECTION("erf, erfc, tgamma, lgamma");
    {
        int wrong = 0;
        for (int i = 0; i < (int)(sizeof(special_refs) / sizeof(special_refs[0])); i++)
        {
            double x = d_of(special_refs[i].x);
            double got = special(special_refs[i].name, x);
            double want = d_of(special_refs[i].want);
            double tol = special_refs[i].name[0] == 't' ? 5.0 : 4.0;
            double e = ulps(got, want);
            // lgamma below 0 is ln(pi / |sin(pi x)|) - lgamma(1 - x), two terms that cancel near its zeros: there its
            // error is counted in ulps of the larger term.
            if (special_refs[i].name[0] == 'l' && x < 0.0)
            {
                double size = lgamma(1.0 - x);
                if (size < 1.0)
                    size = 1.0;
                e = fabs(got - want) / (size * DBL_EPSILON);
            }
            if (e > tol)
            {
                wrong++;
                if (shown++ < 12)
                    printf("  %s(%.17g) = %.17g, want %.17g: %.0f ulps\n", special_refs[i].name, x, got, want, e);
            }
        }
        CHECK_EQ(wrong, 0);
    }

    TEST_SECTION("exact values");
    CHECK(sqrt(4.0) == 2.0 && sqrt(2.0) == 1.4142135623730951 && fabs(-2.5) == 2.5);
    CHECK(floor(-2.5) == -3.0 && ceil(-2.5) == -2.0 && trunc(-2.5) == -2.0 && rint(2.5) == 2.0 && round(2.5) == 3.0);
    CHECK(round(0.49999999999999994) == 0.0 && round(-2.5) == -3.0);
    CHECK(fmin(1.0, 2.0) == 1.0 && fmax(1.0, 2.0) == 2.0 && copysign(3.0, -0.0) == -3.0);
    CHECK(fma(0.1, 10.0, -1.0) == 5.551115123125783e-17);   // one rounding: 0.1*10 is not rounded to 1 first
    CHECK(fmod(7.0, 3.0) == 1.0 && fmod(-7.0, 3.0) == -1.0 && fmod(1e300, 3.0) == 0.0);
    CHECK(remainder(5.5, 2.0) == -0.5 && remainder(6.5, 2.0) == 0.5);
    CHECK(exp(0.0) == 1.0 && log(1.0) == 0.0 && log2(8.0) == 3.0 && log10(1000.0) == 3.0 && log10(1e22) == 22.0);
    CHECK(pow(2.0, 0.5) == sqrt(2.0) && pow(10.0, 22.0) == 1e22 && pow(-2.0, 3.0) == -8.0 && pow(2.0, -1074.0) == 4.9406564584124654e-324);
    CHECK(sin(1e22) == -0.8522008497671888);                // the famous one: the reduction keeps every bit
    CHECK(cbrt(27.0) == 3.0 && cbrt(-8.0) == -2.0 && hypot(3.0, 4.0) == 5.0);
    CHECK(tgamma(5.0) == 24.0 && tgamma(0.5) == 1.7724538509055161 && lgamma(1.0) == 0.0 && lgamma(2.0) == 0.0);
    {
        int e;
        double m = frexp(48.0, &e);
        CHECK(m == 0.75 && e == 6);
        m = frexp(4.9406564584124654e-324, &e);
        CHECK(m == 0.5 && e == -1073);
        CHECK(ldexp(0.75, 6) == 48.0 && ldexp(1.0, -1074) == 4.9406564584124654e-324 && ilogb(1e300) == 996);
        double ip;
        CHECK(modf(-3.25, &ip) == -0.25 && ip == -3.0);
        CHECK(nextafter(1.0, 2.0) == 1.0000000000000002 && nextafter(0.0, -1.0) == -4.9406564584124654e-324);
        CHECK(lround(2.5) == 3 && lrint(2.5) == 2 && llround(-1e15 - 0.5) == -1000000000000001LL);
    }

    TEST_SECTION("special values and errno");
    CHECK(isnan(sqrt(-1.0)) && isnan(log(-1.0)) && isnan(acos(2.0)) && isnan(pow(-2.0, 0.5)));
    CHECK(isinf(exp(710.0)) && exp(-746.0) == 0.0 && isinf(log(0.0)) && log(0.0) < 0.0);
    CHECK(pow(0.0, -1.0) > 0.0 && isinf(pow(0.0, -1.0)) && pow(-0.0, -1.0) < 0.0 && pow(NAN, 0.0) == 1.0 && pow(1.0, NAN) == 1.0);
    CHECK(atan2(0.0, -1.0) == M_PI && atan2(-0.0, -1.0) == -M_PI && signbit(atan2(-0.0, 1.0)));
    CHECK(isinf(tgamma(0.0)) && isnan(tgamma(-2.0)) && isinf(lgamma(-2.0)) && erf(10.0) == 1.0 && erfc(30.0) == 0.0);
    CHECK_ERRNO(log(-1.0), EDOM);
    CHECK_ERRNO(log(0.0), ERANGE);
    CHECK_ERRNO(exp(1000.0), ERANGE);
    CHECK_ERRNO(pow(-8.0, 1.0 / 3.0), EDOM);
    CHECK_ERRNO(pow(0.0, -2.0), ERANGE);
    CHECK_ERRNO(sin(INFINITY), EDOM);
    CHECK_ERRNO(fmod(1.0, 0.0), EDOM);
    CHECK_ERRNO(tgamma(-3.0), EDOM);
    CHECK_ERRNO(acosh(0.5), EDOM);
    CHECK_ERRNO(sqrt(2.0), 0);

    TEST_SECTION("classification");
    CHECK(isnan(NAN) && !isnan(1.0) && isinf(HUGE_VAL) && isfinite(1e300) && !isfinite(HUGE_VAL));
    CHECK(isnormal(DBL_MIN) && !isnormal(DBL_MIN / 2) && fpclassify(DBL_MIN / 2) == FP_SUBNORMAL);
    CHECK(fpclassify(1e-310) == FP_SUBNORMAL && fpclassify(0.0) == FP_ZERO && fpclassify(-HUGE_VAL) == FP_INFINITE);
    CHECK(signbit(-0.0) && !signbit(0.0) && signbit(-1e-310));
    CHECK(isnormal(1e-40f) == 0 && isnormal(1e-40) != 0);        // a float subnormal is a normal double

    TEST_SECTION("identities");
    {
        int wrong = 0;
        for (double x = -20.0; x <= 20.0; x += 0.3718)
        {
            double s, c;
            sincos(x, &s, &c);
            if (s != sin(x) || c != cos(x) || fabs(s * s + c * c - 1.0) > 4 * DBL_EPSILON)
                wrong++;
            if (sin(-x) != -sin(x) || cos(-x) != cos(x))
                wrong++;
            double e = exp(x);
            if (fabs(log(e) - x) > 4 * DBL_EPSILON * (fabs(x) > 1 ? fabs(x) : 1))
                wrong++;
        }
        CHECK_EQ(wrong, 0);
    }

    TEST_SECTION("the float family is still there");
    CHECK(sqrtf(2.0f) == 1.41421356f && sinf(0.5f) == 0.479425550f && powf(2.0f, 10.0f) == 1024.0f);
    CHECK(sizeof(sqrtf(2.0f)) == 4 && sizeof(sqrt(2.0f)) == 8 && sizeof(sinl(1.0)) == 8);
    return test_summary();
}
