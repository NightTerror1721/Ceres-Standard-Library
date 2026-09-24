// <math.h>: every function against reference values (tests/math_tables.inc, computed in double), then the
// special values, errno, symmetries and round trips that a table cannot show.
//
// Compile with -DMATH_REPORT to print the worst relative error of each function instead of testing it
// against a tolerance: that is how the tolerances below were chosen.
#include "ceres/test.h"
#include "math.h"
#include "errno.h"
#include "float.h"
#include "limits.h"

#include "math_tables.inc"

// ---- helpers ----

static float rel_err(float got, float want)
{
    if (isnan(got) || isnan(want))
        return (isnan(got) && isnan(want)) ? 0.0f : 1.0e30f;
    if (isinf(got) || isinf(want))
        return (got == want) ? 0.0f : 1.0e30f;
    if (want == 0.0f)
        return got == 0.0f ? 0.0f : 1.0e30f;
    return fabs(got - want) / fabs(want);
}

#define CHECK_REL(a, b, tol) \
    do { float a_ = (a); float b_ = (b); __t_total++; \
         if (!(rel_err(a_, b_) <= (tol))) { __t_failed++; \
             printf("FAIL %s:%d: %s ~ %s (%g vs %g)\n", __FILE__, __LINE__, #a, #b, a_, b_); } } while (0)

// The value is +-0 (and, for the sign, exactly the zero asked for).
#define CHECK_ZERO(a, negative) \
    do { float z_ = (a); __t_total++; \
         if (z_ != 0.0f || (signbit(z_) != 0) != (negative)) { __t_failed++; \
             printf("FAIL %s:%d: %s is not %s0 (%g)\n", __FILE__, __LINE__, #a, (negative) ? "-" : "+", z_); } } while (0)

#define CHECK_INF(a, negative) \
    do { float i_ = (a); __t_total++; \
         if (!isinf(i_) || (i_ < 0.0f) != (negative)) { __t_failed++; \
             printf("FAIL %s:%d: %s is not %sinf (%g)\n", __FILE__, __LINE__, #a, (negative) ? "-" : "+", i_); } } while (0)

#define CHECK_NAN(a) \
    do { float n_ = (a); __t_total++; \
         if (!isnan(n_)) { __t_failed++; printf("FAIL %s:%d: %s is not NaN (%g)\n", __FILE__, __LINE__, #a, n_); } } while (0)

// `stmt` must leave errno clear before, and `code` set afterwards.
#define CHECK_ERRNO(expr, code) \
    do { errno = 0; (void)(expr); __t_total++; \
         if (errno != (code)) { __t_failed++; printf("FAIL %s:%d: errno after %s is %d, wanted %d\n", __FILE__, __LINE__, #expr, errno, (code)); } } while (0)

static void report_or_check(const char* name, float worst, float x, float tol)
{
#ifdef MATH_REPORT
    printf("%-6s worst rel err %.3e at x=%g\n", name, worst, x);
#else
    __t_total++;
    if (!(worst <= tol))
    {
        __t_failed++;
        printf("FAIL %s: rel err %.3e at x=%g exceeds %.1e\n", name, worst, x, tol);
    }
#endif
}

static void check_1(const char* name, float (*fn)(float), const float* xs, const float* want, int n, float tol)
{
    float worst = 0.0f, worst_x = 0.0f;
    for (int i = 0; i < n; i++)
    {
        float e = rel_err(fn(xs[i]), want[i]);
        if (!(e <= worst)) { worst = e; worst_x = xs[i]; }
    }
    report_or_check(name, worst, worst_x, tol);
}

static void check_2(const char* name, float (*fn)(float, float), const float* as, const float* bs, const float* want, int n, float tol)
{
    float worst = 0.0f, worst_x = 0.0f;
    for (int i = 0; i < n; i++)
    {
        float e = rel_err(fn(as[i], bs[i]), want[i]);
        if (!(e <= worst)) { worst = e; worst_x = as[i]; }
    }
    report_or_check(name, worst, worst_x, tol);
}

#define TABLE_1(fn, count, tol) check_1(#fn, fn, t_##fn##_x, t_##fn##_y, count, tol)
#define TABLE_2(fn, count, tol) check_2(#fn, fn, t_##fn##_a, t_##fn##_b, t_##fn##_y, count, tol)

// One function per section: at -O0 every local of a function gets its own frame slot, and a
// frame past 32 KB cannot be addressed (a 16-bit displacement), which a single main() of this size hits.
#ifndef MATH_REPORT
static void section_classification(void)
{
    TEST_SECTION("classification");
    CHECK_EQ(fpclassify(1.0f), FP_NORMAL);
    CHECK_EQ(fpclassify(-3.5f), FP_NORMAL);
    CHECK_EQ(fpclassify(FLT_MIN), FP_NORMAL);
    CHECK_EQ(fpclassify(FLT_MAX), FP_NORMAL);
    CHECK_EQ(fpclassify(0.0f), FP_ZERO);
    CHECK_EQ(fpclassify(-0.0f), FP_ZERO);
    CHECK_EQ(fpclassify(INFINITY), FP_INFINITE);
    CHECK_EQ(fpclassify(-INFINITY), FP_INFINITE);
    CHECK_EQ(fpclassify(NAN), FP_NAN);
    CHECK_EQ(fpclassify(FLT_TRUE_MIN), FP_SUBNORMAL);
    CHECK_EQ(fpclassify(-FLT_TRUE_MIN), FP_SUBNORMAL);
    CHECK(isfinite(1.0f) && isfinite(0.0f) && isfinite(FLT_TRUE_MIN) && isfinite(-FLT_MAX));
    CHECK(!isfinite(INFINITY) && !isfinite(-INFINITY) && !isfinite(NAN));
    CHECK(isnormal(1.0f) && isnormal(-FLT_MIN) && isnormal(FLT_MAX));
    CHECK(!isnormal(0.0f) && !isnormal(FLT_TRUE_MIN) && !isnormal(INFINITY) && !isnormal(NAN));
    CHECK(isnan(NAN) && !isnan(1.0f) && !isnan(INFINITY));
    CHECK(isinf(INFINITY) && isinf(-INFINITY) && !isinf(FLT_MAX) && !isinf(NAN));
    CHECK(signbit(-1.0f) && signbit(-0.0f) && signbit(-INFINITY) && !signbit(0.0f) && !signbit(3.0f));
    CHECK(INFINITY > FLT_MAX && -INFINITY < -FLT_MAX);
    CHECK(NAN != NAN);
    CHECK_EQ((int)float_bits(1.0f), 0x3F800000);
    CHECK(float_from_bits(0x40490FDBu) == 3.14159274f);
}

static void section_the_one_instruction_functions(void)
{
    TEST_SECTION("the one-instruction functions");
    CHECK(fabs(-2.5f) == 2.5f && fabs(2.5f) == 2.5f);
    CHECK(sqrt(4.0f) == 2.0f && sqrt(0.0f) == 0.0f);
    CHECK_REL(sqrt(2.0f), 1.41421356f, 1e-7f);
    CHECK_NAN(sqrt(-1.0f));
    CHECK(floor(-2.1f) == -3.0f && floor(2.9f) == 2.0f);
    CHECK(ceil(-2.9f) == -2.0f && ceil(2.1f) == 3.0f);
    CHECK(trunc(-2.7f) == -2.0f && trunc(2.7f) == 2.0f);
    CHECK(round(2.5f) == 3.0f && round(-2.5f) == -3.0f && round(2.4f) == 2.0f);
    CHECK(fmin(1.0f, 2.0f) == 1.0f && fmax(1.0f, 2.0f) == 2.0f);
    CHECK(fmod(7.0f, 3.0f) == 1.0f && fmod(-7.0f, 3.0f) == -1.0f && fmod(5.5f, 2.0f) == 1.5f);
    CHECK(copysign(3.0f, -1.0f) == -3.0f && copysign(-3.0f, 1.0f) == 3.0f);
    CHECK(fma(2.0f, 3.0f, 4.0f) == 10.0f);
    CHECK_REL(rcp(4.0f), 0.25f, 5e-2f);                 // fast approximations: only a few bits
    CHECK_REL(rsqrt(4.0f), 0.5f, 5e-2f);
    CHECK(M_PI == 3.14159265f && M_E == 2.71828183f && M_SQRT2 == 1.41421356f);
}

// The macros and the functions behind them (a call through a pointer, or a parenthesized name,
// reaches the CASM function) must agree on every kind of float, and an int argument is converted
// the way the prototype would.
static void section_macros_and_functions_agree(void)
{
    TEST_SECTION("builtin macros and the functions agree");
    float (*unary[9])(float) = { fabs, sqrt, floor, ceil, trunc, rint, nearbyint, rcp, rsqrt };
    float (*binary[3])(float, float) = { fmin, fmax, copysign };
    int (*classify[6])(float) = { isnan, isinf, isfinite, isnormal, signbit, fpclassify };
    float values[10];
    values[0] = 2.5f; values[1] = -2.5f; values[2] = 0.0f; values[3] = -0.0f; values[4] = 1e-40f;
    values[5] = INFINITY; values[6] = -INFINITY; values[7] = NAN; values[8] = 3.5f; values[9] = -1e30f;
    int same = 1;
    for (int i = 0; i < 10; i++)
    {
        float v = values[i];
        float m[9];
        m[0] = fabs(v); m[1] = sqrt(v); m[2] = floor(v); m[3] = ceil(v);
        m[4] = trunc(v); m[5] = rint(v); m[6] = nearbyint(v); m[7] = rcp(v); m[8] = rsqrt(v);
        for (int k = 0; k < 9; k++)
            if ((float_bits)(m[k]) != (float_bits)(unary[k](v)))   // the functions as the oracle
                same = 0;
        // ... and the bit macros against their functions, on every kind of float
        if (float_bits(v) != (float_bits)(v) ||
            (float_bits)(float_from_bits(float_bits(v))) != (float_bits)((float_from_bits)((float_bits)(v))))
            same = 0;
        int c[6];
        c[0] = isnan(v); c[1] = isinf(v); c[2] = isfinite(v); c[3] = isnormal(v); c[4] = signbit(v); c[5] = fpclassify(v);
        for (int k = 0; k < 6; k++)
            if ((c[k] != 0) != (classify[k](v) != 0) || (k == 5 && c[k] != classify[k](v)))
                same = 0;
        for (int j = 0; j < 10; j++)
        {
            float w = values[j];
            if (float_bits(fmin(v, w)) != float_bits(binary[0](v, w)) ||
                float_bits(fmax(v, w)) != float_bits(binary[1](v, w)) ||
                float_bits(copysign(v, w)) != float_bits(binary[2](v, w)))
                same = 0;
        }
    }
    CHECK(same);
    CHECK((sqrt)(9.0f) == 3.0f && (fabs)(-1.0f) == 1.0f && (isnan)(NAN) != 0);
    CHECK((float_bits)(1.0f) == 0x3F800000u && (float_from_bits)(0x3F800000u) == 1.0f);
    CHECK(sqrt(16) == 4.0f && fabs(-3) == 3.0f && floor(7) == 7.0f);   // int arguments, converted
    CHECK(fmin(2, 1.5f) == 1.5f && copysign(2, -1) == -2.0f);
    CHECK(isfinite(3) && !isnan(0) && fpclassify(0) == FP_ZERO);
    CHECK(float_from_bits(0x3F800000) == 1.0f);                     // an int bit pattern
}

static void section_trigonometric_identities(void)
{
    TEST_SECTION("trigonometric: identities");
    {
        float worst = 0.0f;
        for (int i = -300; i <= 300; i++)
        {
            float x = (float)i * 0.173f;
            float s = sin(x), c = cos(x);
            float e = fabs(s * s + c * c - 1.0f);
            if (e > worst) worst = e;
            CHECK(sin(-x) == -s);                        // odd and even symmetry are exact
            CHECK(cos(-x) == c);
            float s2, c2;
            sincos(x, &s2, &c2);
            CHECK(s2 == s && c2 == c);                   // sincos is the same computation
        }
        CHECK(worst < 4e-7f);
    }
    CHECK_ZERO(sin(0.0f), 0);
    CHECK_ZERO(sin(-0.0f), 1);
    CHECK(cos(0.0f) == 1.0f);
    CHECK_REL(sin(M_PI_2), 1.0f, 1e-7f);
    CHECK_REL(cos(M_PI / 3.0f), 0.5f, 3e-7f);
    CHECK_REL(tan(M_PI_4), 1.0f, 3e-7f);
    CHECK_REL(sin(M_PI / 6.0f), 0.5f, 3e-7f);
    {
        float worst = 0.0f;
        for (int i = -140; i <= 140; i++)
        {
            float x = (float)i * 0.01f;                  // atan(tan(x)), asin(sin(x)), acos(cos(x)) on their branches
            float e1 = fabs(atan(tan(x)) - x);
            float e2 = fabs(asin(sin(x)) - x);
            if (e1 > worst) worst = e1;
            if (e2 > worst) worst = e2;
            if (x >= 0.3f) { float e3 = fabs(acos(cos(x)) - x); if (e3 > worst) worst = e3; }
        }
        CHECK(worst < 3e-6f);
    }
    {
        float s, c;
        sincos(1000.0f, &s, &c);
        CHECK_REL(s, 0.82687954f, 3e-6f);
        CHECK_REL(c, 0.56237938f, 3e-6f);
        // Beyond ~6000 the digits fade but the result stays a bounded number.
        float big = sin(1.0e9f);
        CHECK(big >= -1.0f && big <= 1.0f);
        float huge = cos(3.0e38f);
        CHECK(huge >= -1.0f && huge <= 1.0f);
    }
}

static void section_inverse_trigonometric_quadrants_and_specials(void)
{
    TEST_SECTION("inverse trigonometric: quadrants and specials");
    CHECK_REL(atan2(1.0f, 1.0f), M_PI_4, 1e-7f);
    CHECK_REL(atan2(1.0f, -1.0f), 3.0f * M_PI_4, 3e-7f);
    CHECK_REL(atan2(-1.0f, -1.0f), -3.0f * M_PI_4, 3e-7f);
    CHECK_REL(atan2(-1.0f, 1.0f), -M_PI_4, 1e-7f);
    CHECK_ZERO(atan2(0.0f, 1.0f), 0);
    CHECK_ZERO(atan2(-0.0f, 1.0f), 1);
    CHECK_REL(atan2(0.0f, -1.0f), M_PI, 1e-7f);
    CHECK_REL(atan2(-0.0f, -1.0f), -M_PI, 1e-7f);
    CHECK_REL(atan2(0.0f, -0.0f), M_PI, 1e-7f);
    CHECK_ZERO(atan2(0.0f, 0.0f), 0);
    CHECK_REL(atan2(1.0f, 0.0f), M_PI_2, 1e-7f);
    CHECK_REL(atan2(-1.0f, 0.0f), -M_PI_2, 1e-7f);
    CHECK_REL(atan2(INFINITY, INFINITY), M_PI_4, 1e-7f);
    CHECK_REL(atan2(INFINITY, -INFINITY), 3.0f * M_PI_4, 3e-7f);
    CHECK_ZERO(atan2(1.0f, INFINITY), 0);
    CHECK_REL(atan2(1.0f, -INFINITY), M_PI, 1e-7f);
    CHECK_REL(atan2(INFINITY, 5.0f), M_PI_2, 1e-7f);
    CHECK_NAN(atan2(NAN, 1.0f));
    CHECK_NAN(atan2(1.0f, NAN));
    CHECK_REL(atan(INFINITY), M_PI_2, 1e-7f);
    CHECK_REL(atan(-INFINITY), -M_PI_2, 1e-7f);
    CHECK_NAN(atan(NAN));
    CHECK_ZERO(atan(0.0f), 0);
    CHECK_REL(asin(1.0f), M_PI_2, 1e-7f);
    CHECK_REL(asin(-1.0f), -M_PI_2, 1e-7f);
    CHECK_ZERO(acos(1.0f), 0);
    CHECK_REL(acos(-1.0f), M_PI, 1e-7f);
    CHECK_REL(acos(0.0f), M_PI_2, 1e-7f);
}

static void section_domain_and_range_errors(void)
{
    TEST_SECTION("domain and range errors");
    CHECK_ERRNO(fmod(5.0f, 0.0f), EDOM);            // the instruction traps and would give back 5
    CHECK_NAN(fmod(5.0f, 0.0f));
    CHECK_NAN(fmod(5.0f, -0.0f));
    CHECK_ERRNO(fmod(INFINITY, 2.0f), EDOM);
    CHECK_NAN(fmod(-INFINITY, 2.0f));
    CHECK(fmod(5.5f, INFINITY) == 5.5f && fmod(-5.5f, -INFINITY) == -5.5f);   // an infinite y leaves x
    CHECK_NAN(fmod(NAN, 2.0f));
    CHECK_NAN(fmod(2.0f, NAN));
    CHECK_NAN(fmod(NAN, 0.0f));
    CHECK_ERRNO(fmod(NAN, 0.0f), 0);                // NaN in is not a domain error of its own
    CHECK_ERRNO(log(-1.0f), EDOM);
    CHECK_NAN(log(-1.0f));
    CHECK_ERRNO(log(0.0f), ERANGE);
    CHECK_INF(log(0.0f), 1);
    CHECK_INF(log(INFINITY), 0);
    CHECK_NAN(log(NAN));
    CHECK_ERRNO(log2(-4.0f), EDOM);
    CHECK_ERRNO(log10(0.0f), ERANGE);
    CHECK_ERRNO(log1p(-2.0f), EDOM);
    CHECK_ERRNO(log1p(-1.0f), ERANGE);
    CHECK_INF(log1p(-1.0f), 1);
    CHECK_ERRNO(asin(2.0f), EDOM);
    CHECK_NAN(asin(-1.5f));
    CHECK_ERRNO(acos(-2.0f), EDOM);
    CHECK_ERRNO(acosh(0.5f), EDOM);
    CHECK_ERRNO(atanh(2.0f), EDOM);
    CHECK_ERRNO(atanh(1.0f), ERANGE);
    CHECK_INF(atanh(1.0f), 0);
    CHECK_INF(atanh(-1.0f), 1);
    CHECK_ERRNO(sin(INFINITY), EDOM);
    CHECK_NAN(cos(-INFINITY));
    CHECK_NAN(tan(INFINITY));
    CHECK_NAN(sin(NAN));
    CHECK_ERRNO(exp(1000.0f), ERANGE);
    CHECK_INF(exp(1000.0f), 0);
    CHECK_ERRNO(exp(-1000.0f), ERANGE);
    CHECK_ZERO(exp(-1000.0f), 0);
    CHECK_ERRNO(exp(INFINITY), 0);                       // exp(inf) = inf is not an error
    CHECK_INF(exp(INFINITY), 0);
    CHECK_ZERO(exp(-INFINITY), 0);
    CHECK_ERRNO(exp2(1000.0f), ERANGE);
    CHECK_ERRNO(exp2(-1000.0f), ERANGE);
    CHECK_ERRNO(exp(1.0f), 0);                           // and a good call leaves errno alone
    CHECK_ERRNO(sin(1.0f), 0);
    CHECK_ERRNO(log(2.0f), 0);
    CHECK_ERRNO(pow(2.0f, 10.0f), 0);
    CHECK_ERRNO(pow(-2.0f, 0.5f), EDOM);
    CHECK_NAN(pow(-2.0f, 0.5f));
    CHECK_ERRNO(pow(0.0f, -1.0f), ERANGE);
    CHECK_INF(pow(0.0f, -1.0f), 0);
    CHECK_ERRNO(pow(10.0f, 39.0f), ERANGE);
    CHECK_INF(pow(10.0f, 39.0f), 0);
    CHECK_ERRNO(pow(10.0f, -50.0f), ERANGE);
    CHECK_ZERO(pow(10.0f, -50.0f), 0);
    CHECK_ERRNO(pow(2.0f, 1000.0f), ERANGE);
    CHECK_ERRNO(sinh(100.0f), ERANGE);
    CHECK_INF(sinh(100.0f), 0);
    CHECK_INF(sinh(-100.0f), 1);
    CHECK_INF(cosh(100.0f), 0);
    CHECK_ERRNO(ldexp(1.0f, 200), ERANGE);
    CHECK_ERRNO(hypot(3.0e38f, 2.5e38f), ERANGE);
    CHECK_INF(hypot(3.0e38f, 2.5e38f), 0);
    CHECK_ERRNO(remainder(1.0f, 0.0f), EDOM);
    CHECK_ERRNO(lround(3.0e9f), ERANGE);
    CHECK_ERRNO(lrint(NAN), EDOM);
}

static void section_exp_and_log(void)
{
    TEST_SECTION("exp and log");
    CHECK(exp(0.0f) == 1.0f);
    CHECK_REL(exp(1.0f), M_E, 1e-7f);
    CHECK(log(1.0f) == 0.0f);
    CHECK_REL(log(M_E), 1.0f, 3e-7f);
    CHECK(log2(8.0f) == 3.0f);
    CHECK(log2(0.25f) == -2.0f);
    CHECK(log2(1.0f) == 0.0f);
    CHECK_REL(log10(1000.0f), 3.0f, 3e-7f);
    CHECK_REL(log10(0.001f), -3.0f, 3e-7f);
    CHECK(exp2(0.0f) == 1.0f && exp2(10.0f) == 1024.0f && exp2(-1.0f) == 0.5f && exp2(3.0f) == 8.0f);
    CHECK(exp2(-149.0f) == FLT_TRUE_MIN);
    CHECK(exp2(127.0f) == 1.70141183e38f);
    CHECK(expm1(0.0f) == 0.0f);
    CHECK(log1p(0.0f) == 0.0f);
    CHECK_REL(expm1(1e-7f), 1e-7f, 1e-6f);               // no cancellation for tiny arguments
    CHECK_REL(log1p(1e-7f), 1e-7f, 1e-6f);
    CHECK(exp(-104.0f) >= 0.0f);                         // deep in the subnormals: no crash, no negative
    CHECK(log(FLT_MIN) < -87.0f && log(FLT_MIN) > -88.0f);
    CHECK(log(FLT_TRUE_MIN) < -103.0f && log(FLT_TRUE_MIN) > -104.0f);   // a subnormal argument works
    CHECK_REL(log(FLT_MAX), 88.7228394f, 3e-7f);
    {
        float worst = 0.0f;
        for (int i = 1; i <= 400; i++)
        {
            float x = (float)i * 0.05f;
            float e1 = rel_err(exp(log(x)), x);
            float e2 = rel_err(log(exp(1.0f + x * 0.5f)), 1.0f + x * 0.5f);   // exp of 1..11 keeps log's argument away from 1
            if (e2 > worst) worst = e2;
            if (e1 > worst) worst = e1;
        }
        CHECK(worst < 3e-6f);
    }
}

static void section_pow(void)
{
    TEST_SECTION("pow");
    CHECK(pow(0.0f, 0.0f) == 1.0f);
    CHECK(pow(2.0f, 0.0f) == 1.0f);
    CHECK(pow(NAN, 0.0f) == 1.0f);
    CHECK(pow(1.0f, NAN) == 1.0f);
    CHECK(pow(1.0f, INFINITY) == 1.0f);
    CHECK(pow(-1.0f, INFINITY) == 1.0f);
    CHECK_NAN(pow(NAN, 1.0f));
    CHECK_NAN(pow(2.0f, NAN));
    CHECK_INF(pow(2.0f, INFINITY), 0);
    CHECK_ZERO(pow(0.5f, INFINITY), 0);
    CHECK_ZERO(pow(2.0f, -INFINITY), 0);
    CHECK_INF(pow(0.5f, -INFINITY), 0);
    CHECK_ZERO(pow(INFINITY, -1.0f), 0);
    CHECK_INF(pow(INFINITY, 2.0f), 0);
    CHECK_INF(pow(-INFINITY, 3.0f), 1);
    CHECK_INF(pow(-INFINITY, 2.0f), 0);
    CHECK_ZERO(pow(0.0f, 3.0f), 0);
    CHECK_ZERO(pow(-0.0f, 3.0f), 1);
    CHECK_ZERO(pow(-0.0f, 2.0f), 0);
    CHECK_INF(pow(-0.0f, -3.0f), 1);
    CHECK_INF(pow(-0.0f, -2.0f), 0);
    CHECK(pow(-8.0f, 3.0f) == -512.0f);
    CHECK(pow(-2.0f, 4.0f) == 16.0f);
    CHECK(pow(2.0f, 10.0f) == 1024.0f);
    CHECK(pow(2.0f, -1.0f) == 0.5f);
    CHECK(pow(3.0f, 3.0f) == 27.0f);
    CHECK(pow(10.0f, 2.0f) == 100.0f);
    CHECK_REL(pow(2.0f, 0.5f), M_SQRT2, 3e-7f);
    CHECK_REL(pow(-2.0f, -3.0f), -0.125f, 3e-7f);
}

static void section_hyperbolic_roots_and_hypot(void)
{
    TEST_SECTION("hyperbolic, roots and hypot");
    CHECK_ZERO(sinh(0.0f), 0);
    CHECK_ZERO(sinh(-0.0f), 1);
    CHECK(cosh(0.0f) == 1.0f);
    CHECK_ZERO(tanh(0.0f), 0);
    CHECK(tanh(20.0f) == 1.0f && tanh(-20.0f) == -1.0f);
    CHECK_INF(sinh(INFINITY), 0);
    CHECK_INF(cosh(-INFINITY), 0);
    CHECK(tanh(INFINITY) == 1.0f);
    CHECK_INF(asinh(INFINITY), 0);
    CHECK_INF(acosh(INFINITY), 0);
    CHECK_ZERO(asinh(0.0f), 0);
    CHECK_ZERO(acosh(1.0f), 0);
    CHECK_ZERO(atanh(0.0f), 0);
    CHECK_ZERO(cbrt(0.0f), 0);
    CHECK_ZERO(cbrt(-0.0f), 1);
    CHECK_REL(cbrt(-8.0f), -2.0f, 1e-7f);
    CHECK_REL(cbrt(27.0f), 3.0f, 1e-7f);
    CHECK_INF(cbrt(-INFINITY), 1);
    CHECK_NAN(cbrt(NAN));
    CHECK(hypot(3.0f, 4.0f) == 5.0f);
    CHECK(hypot(0.0f, -7.0f) == 7.0f);
    CHECK_INF(hypot(INFINITY, NAN), 0);
    CHECK_NAN(hypot(NAN, 1.0f));
    CHECK_REL(hypot(1.0e30f, 1.0e30f), 1.41421356e30f, 3e-7f);   // x*x alone would overflow
    {
        float worst = 0.0f;
        for (int i = 1; i <= 200; i++)
        {
            float x = (float)i * 1.37f;
            float c = cbrt(x);
            float e = rel_err(c * c * c, x);
            float e2 = rel_err(sinh(x * 0.01f) / cosh(x * 0.01f), tanh(x * 0.01f));
            if (e > worst) worst = e;
            if (e2 > worst) worst = e2;
        }
        CHECK(worst < 1e-6f);
    }
}

static void section_ldexp_frexp_modf(void)
{
    TEST_SECTION("ldexp, frexp, modf");
    CHECK(ldexp(1.0f, 10) == 1024.0f);
    CHECK(ldexp(3.0f, -1) == 1.5f);
    CHECK(ldexp(-1.5f, 4) == -24.0f);
    CHECK(ldexp(1.0f, -149) == FLT_TRUE_MIN);
    CHECK(ldexp(0.0f, 100) == 0.0f);
    CHECK_INF(ldexp(1.0f, 200), 0);
    CHECK_INF(ldexp(INFINITY, -5), 0);
    CHECK(scalbn(1.0f, 3) == 8.0f);
    int e = 99;
    CHECK(frexp(8.0f, &e) == 0.5f && e == 4);
    CHECK(frexp(1.0f, &e) == 0.5f && e == 1);
    CHECK(frexp(-0.75f, &e) == -0.75f && e == 0);
    CHECK(frexp(0.0f, &e) == 0.0f && e == 0);
    CHECK(frexp(0.1f, &e) > 0.5f && e == -3);
    CHECK(frexp(FLT_MAX, &e) < 1.0f && e == 128);
    {
        float sub = FLT_TRUE_MIN * 12345.0f;              // a subnormal
        float m = frexp(sub, &e);
        CHECK(m >= 0.5f && m < 1.0f);
        CHECK(ldexp(m, e) == sub);
        int bad = 0;
        for (int i = 1; i <= 120; i++)                    // frexp then ldexp gives the number back
        {
            float x = (float)i * 3.7f - 200.0f;
            float mm = frexp(x, &e);
            if (ldexp(mm, e) != x) bad++;
            if (x != 0.0f && (fabs(mm) < 0.5f || fabs(mm) >= 1.0f)) bad++;
        }
        CHECK_EQ(bad, 0);
    }
    float ip;
    CHECK(modf(3.75f, &ip) == 0.75f && ip == 3.0f);
    CHECK(modf(-3.75f, &ip) == -0.75f && ip == -3.0f);
    CHECK(modf(5.0f, &ip) == 0.0f && ip == 5.0f);
    CHECK_ZERO(modf(-5.0f, &ip), 1);
    CHECK(ip == -5.0f);
    CHECK_ZERO(modf(INFINITY, &ip), 0);
    CHECK(isinf(ip));
    CHECK(isnan(modf(NAN, &ip)) && isnan(ip));
    CHECK(modf(0.25f, &ip) == 0.25f && ip == 0.0f);
}

static void section_rounding(void)
{
    TEST_SECTION("rounding");
    CHECK_ZERO(nearbyint(0.5f), 0);                       // halves go to even
    CHECK(nearbyint(1.5f) == 2.0f && nearbyint(2.5f) == 2.0f && nearbyint(3.5f) == 4.0f);
    CHECK_ZERO(nearbyint(-0.5f), 1);
    CHECK(nearbyint(-1.5f) == -2.0f && nearbyint(-2.5f) == -2.0f && nearbyint(-3.5f) == -4.0f);
    CHECK(nearbyint(2.4f) == 2.0f && nearbyint(2.6f) == 3.0f && nearbyint(-2.6f) == -3.0f);
    CHECK(nearbyint(8388608.0f) == 8388608.0f && nearbyint(1.0e10f) == 1.0e10f);
    CHECK(rint(2.5f) == 2.0f && rint(3.5f) == 4.0f);
    CHECK(isinf(nearbyint(INFINITY)) && isnan(nearbyint(NAN)));
    CHECK_EQ(lround(0.5f), 1);                            // halves go away from zero
    CHECK_EQ(lround(1.5f), 2);
    CHECK_EQ(lround(2.5f), 3);
    CHECK_EQ(lround(-0.5f), -1);
    CHECK_EQ(lround(-2.5f), -3);
    CHECK_EQ(lround(2.4f), 2);
    CHECK_EQ(lround(2147483000.0f), 2147483008);
    CHECK_EQ(lround(3.0e9f), INT_MAX);
    CHECK_EQ(lround(-3.0e9f), INT_MIN);
    CHECK_EQ(lrint(2.5f), 2);
    CHECK_EQ(lrint(3.5f), 4);
    CHECK_EQ(lrint(-2.5f), -2);
    CHECK_EQ(lrint(NAN), 0);
}

static void section_remainder_fdim_nan(void)
{
    TEST_SECTION("llround and llrint: 64 bits");
    CHECK_EQ64(llround(3.0e9f), 3000000000LL);                  // past what an int holds
    CHECK_EQ64(llround(-3.0e9f), -3000000000LL);
    CHECK_EQ64(llround(2.5f), 3LL);                             // halves away from zero
    CHECK_EQ64(llround(-2.5f), -3LL);
    CHECK_EQ64(llrint(2.5f), 2LL);                              // halves to even
    CHECK_EQ64(llrint(3.5f), 4LL);
    CHECK_EQ64(llrint(1.0e18f), (long long)1.0e18f);
    CHECK_EQ64(llround(-9223372036854775808.0f), -9223372036854775807LL - 1);   // the bottom edge fits
    CHECK_ERRNO(llround(-9223372036854775808.0f), 0);             // ... and is not out of range
    CHECK_EQ64(llrint(-9223372036854775808.0f), -9223372036854775807LL - 1);
    CHECK_ERRNO(llrint(-9223372036854775808.0f), 0);
    CHECK_ERRNO(llround(9223372036854775808.0f), ERANGE);         // 2^63 does not
    CHECK_EQ64(llround(9223372036854775808.0f), 9223372036854775807LL);
    CHECK_EQ64(llrint(-1.0e19f), -9223372036854775807LL - 1);
    CHECK_ERRNO(llrint(-1.0e19f), ERANGE);
    CHECK_ERRNO(llround(NAN), EDOM);
    CHECK_ERRNO(llrint(3.0e9f), 0);

    TEST_SECTION("remainder, fdim, nan");
    CHECK(remainder(5.0f, 3.0f) == -1.0f);                // 5/3 rounds to 2
    CHECK(remainder(7.0f, 2.0f) == -1.0f);                // 3.5 rounds to the even 4
    CHECK(remainder(5.0f, 2.0f) == 1.0f);                 // 2.5 rounds to the even 2
    CHECK(remainder(-5.0f, 3.0f) == 1.0f);
    CHECK(remainder(10.0f, 5.0f) == 0.0f);
    CHECK(remainder(1.5f, 1.0f) == -0.5f);
    CHECK(remainder(2.5f, 1.0f) == 0.5f);
    CHECK(remainder(0.5f, 1.0f) == 0.5f);
    CHECK(remainder(3.0f, INFINITY) == 3.0f);
    CHECK_NAN(remainder(INFINITY, 2.0f));
    CHECK_NAN(remainder(NAN, 2.0f));
    CHECK(fdim(5.0f, 3.0f) == 2.0f && fdim(3.0f, 5.0f) == 0.0f && fdim(3.0f, 3.0f) == 0.0f);
    CHECK_NAN(fdim(NAN, 1.0f));
    CHECK(isnan(nan("")));
    CHECK(isnan(nan("123")));
}

static void section_aliases(void)
{
    TEST_SECTION("aliases");
    CHECK(sinf(0.0f) == 0.0f && cosf(0.0f) == 1.0f && sqrtf(9.0f) == 3.0f && fabsf(-2.0f) == 2.0f);
    CHECK(powf(2.0f, 3.0f) == 8.0f && floorf(1.5f) == 1.0f && ceilf(1.5f) == 2.0f && fmodf(7.0f, 4.0f) == 3.0f);
    CHECK_REL(expf(1.0f), M_E, 1e-7f);
    CHECK_REL(logf(M_E), 1.0f, 3e-7f);
    CHECK_REL(atan2f(1.0f, 1.0f), M_PI_4, 1e-7f);
    CHECK(fmaxf(1.0f, 2.0f) == 2.0f && roundf(2.5f) == 3.0f && truncf(-1.5f) == -1.0f);
}

#endif

int main(void)
{
    TEST_SECTION("tables: trigonometric");
    TABLE_1(sin, 34, 3e-7f);
    TABLE_1(cos, 34, 3e-7f);
    TABLE_1(tan, 22, 5e-7f);
    TABLE_1(asin, 15, 3e-7f);
    TABLE_1(acos, 15, 3e-7f);
    TABLE_1(atan, 20, 3e-7f);
    TABLE_2(atan2, 15, 3e-7f);

    TEST_SECTION("tables: exponential and logarithmic");
    TABLE_1(exp, 22, 3e-7f);
    TABLE_1(exp2, 16, 3e-7f);
    TABLE_1(expm1, 17, 3e-7f);
    TABLE_1(log, 17, 3e-7f);
    TABLE_1(log2, 18, 3e-7f);
    TABLE_1(log10, 16, 3e-7f);
    TABLE_1(log1p, 15, 3e-7f);
    TABLE_2(pow, 24, 3e-6f);

    TEST_SECTION("tables: hyperbolic and roots");
    TABLE_1(sinh, 16, 5e-7f);
    TABLE_1(cosh, 16, 5e-7f);
    TABLE_1(tanh, 13, 5e-7f);
    TABLE_1(asinh, 15, 5e-7f);
    TABLE_1(acosh, 9, 5e-7f);
    TABLE_1(atanh, 10, 5e-7f);
    TABLE_1(cbrt, 20, 3e-7f);
    TABLE_2(hypot, 11, 3e-7f);

#ifndef MATH_REPORT
    section_classification();
    section_the_one_instruction_functions();
    section_macros_and_functions_agree();
    section_trigonometric_identities();
    section_inverse_trigonometric_quadrants_and_specials();
    section_domain_and_range_errors();
    section_exp_and_log();
    section_pow();
    section_hyperbolic_roots_and_hypot();
    section_ldexp_frexp_modf();
    section_rounding();
    section_remainder_fdim_nan();
    section_aliases();
#endif
    return test_summary();
}
