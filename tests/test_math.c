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
    return fabsf(got - want) / fabsf(want);
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

#define TABLE_1(fn, count, tol) check_1(#fn "f", fn##f, t_##fn##_x, t_##fn##_y, count, tol)
#define TABLE_2(fn, count, tol) check_2(#fn "f", fn##f, t_##fn##_a, t_##fn##_b, t_##fn##_y, count, tol)

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
    CHECK(fabsf(-2.5f) == 2.5f && fabsf(2.5f) == 2.5f);
    CHECK(sqrtf(4.0f) == 2.0f && sqrtf(0.0f) == 0.0f);
    CHECK_REL(sqrtf(2.0f), 1.41421356f, 1e-7f);
    CHECK_NAN(sqrtf(-1.0f));
    CHECK(floorf(-2.1f) == -3.0f && floorf(2.9f) == 2.0f);
    CHECK(ceilf(-2.9f) == -2.0f && ceilf(2.1f) == 3.0f);
    CHECK(truncf(-2.7f) == -2.0f && truncf(2.7f) == 2.0f);
    CHECK(roundf(2.5f) == 3.0f && roundf(-2.5f) == -3.0f && roundf(2.4f) == 2.0f);
    CHECK(fminf(1.0f, 2.0f) == 1.0f && fmaxf(1.0f, 2.0f) == 2.0f);
    CHECK(fmodf(7.0f, 3.0f) == 1.0f && fmodf(-7.0f, 3.0f) == -1.0f && fmodf(5.5f, 2.0f) == 1.5f);
    CHECK(copysignf(3.0f, -1.0f) == -3.0f && copysignf(-3.0f, 1.0f) == 3.0f);
    CHECK(fmaf(2.0f, 3.0f, 4.0f) == 10.0f);
    CHECK_REL(rcpf(4.0f), 0.25f, 5e-2f);                 // fast approximations: only a few bits
    CHECK_REL(rsqrtf(4.0f), 0.5f, 5e-2f);
    CHECK((float)M_PI == 3.14159265f && (float)M_E == 2.71828183f && (float)M_SQRT2 == 1.41421356f);
    CHECK(M_PI == 3.141592653589793 && M_E == 2.718281828459045);   // the constants are doubles
}

// The macros and the functions behind them (a call through a pointer, or a parenthesized name,
// reaches the CASM function) must agree on every kind of float, and an int argument is converted
// the way the prototype would.
static void section_macros_and_functions_agree(void)
{
    TEST_SECTION("builtin macros and the functions agree");
    float (*unary[9])(float) = { fabsf, sqrtf, floorf, ceilf, truncf, rintf, nearbyintf, rcpf, rsqrtf };
    float (*binary[3])(float, float) = { fminf, fmaxf, copysignf };
    int (*classify[6])(float) = { isnan, isinf, isfinite, isnormal, signbit, fpclassify };
    float values[10];
    values[0] = 2.5f; values[1] = -2.5f; values[2] = 0.0f; values[3] = -0.0f; values[4] = 1e-40f;
    values[5] = INFINITY; values[6] = -INFINITY; values[7] = NAN; values[8] = 3.5f; values[9] = -1e30f;
    int same = 1;
    for (int i = 0; i < 10; i++)
    {
        float v = values[i];
        float m[9];
        m[0] = fabsf(v); m[1] = sqrtf(v); m[2] = floorf(v); m[3] = ceilf(v);
        m[4] = truncf(v); m[5] = rintf(v); m[6] = nearbyintf(v); m[7] = rcpf(v); m[8] = rsqrtf(v);
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
            if (float_bits(fminf(v, w)) != float_bits(binary[0](v, w)) ||
                float_bits(fmaxf(v, w)) != float_bits(binary[1](v, w)) ||
                float_bits(copysignf(v, w)) != float_bits(binary[2](v, w)))
                same = 0;
        }
    }
    CHECK(same);
    CHECK((sqrtf)(9.0f) == 3.0f && (fabsf)(-1.0f) == 1.0f && (isnan)(NAN) != 0);
    CHECK((float_bits)(1.0f) == 0x3F800000u && (float_from_bits)(0x3F800000u) == 1.0f);
    CHECK(sqrtf(16) == 4.0f && fabsf(-3) == 3.0f && floorf(7) == 7.0f);   // int arguments, converted
    CHECK(fminf(2, 1.5f) == 1.5f && copysignf(2, -1) == -2.0f);
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
            float s = sinf(x), c = cosf(x);
            float e = fabsf(s * s + c * c - 1.0f);
            if (e > worst) worst = e;
            CHECK(sinf(-x) == -s);                        // odd and even symmetry are exact
            CHECK(cosf(-x) == c);
            float s2, c2;
            sincosf(x, &s2, &c2);
            CHECK(s2 == s && c2 == c);                   // sincos is the same computation
        }
        CHECK(worst < 4e-7f);
    }
    CHECK_ZERO(sinf(0.0f), 0);
    CHECK_ZERO(sinf(-0.0f), 1);
    CHECK(cosf(0.0f) == 1.0f);
    CHECK_REL(sinf(M_PI_2), 1.0f, 1e-7f);
    CHECK_REL(cosf(M_PI / 3.0f), 0.5f, 3e-7f);
    CHECK_REL(tanf(M_PI_4), 1.0f, 3e-7f);
    CHECK_REL(sinf(M_PI / 6.0f), 0.5f, 3e-7f);
    {
        float worst = 0.0f;
        for (int i = -140; i <= 140; i++)
        {
            float x = (float)i * 0.01f;                  // atan(tan(x)), asin(sin(x)), acos(cos(x)) on their branches
            float e1 = fabsf(atanf(tanf(x)) - x);
            float e2 = fabsf(asinf(sinf(x)) - x);
            if (e1 > worst) worst = e1;
            if (e2 > worst) worst = e2;
            if (x >= 0.3f) { float e3 = fabsf(acosf(cosf(x)) - x); if (e3 > worst) worst = e3; }
        }
        CHECK(worst < 3e-6f);
    }
    {
        float s, c;
        sincosf(1000.0f, &s, &c);
        CHECK_REL(s, 0.82687954f, 3e-6f);
        CHECK_REL(c, 0.56237938f, 3e-6f);
        // Past 6000 the reduction is Payne and Hanek's, exact for any float (the tables hold the values): sin and
        // cos of one argument still agree.
        float big = sinf(1.0e9f), bigc = cosf(1.0e9f);
        CHECK(fabsf(big * big + bigc * bigc - 1.0f) < 4e-7f);
        float bs, bc;
        sincosf(-3.0e38f, &bs, &bc);
        CHECK(bs == sinf(-3.0e38f) && bc == cosf(3.0e38f));
    }
}

static void section_exponent_next_error_and_gamma(void)
{
    TEST_SECTION("ilogb, logb, nextafter");
    float_t ft = 1.5f;
    double_t dt = ft;
    CHECK(sizeof(float_t) == sizeof(float) && dt == 1.5f);
    CHECK_EQ(ilogbf(1.0f), 0);
    CHECK_EQ(ilogbf(0.75f), -1);
    CHECK_EQ(ilogbf(-1024.5f), 10);
    CHECK_EQ(ilogbf(FLT_MAX), 127);
    CHECK_EQ(ilogbf(float_from_bits(1u)), -149);                   // the smallest subnormal
    CHECK_EQ(ilogbf(0.0f), FP_ILOGB0);
    CHECK_EQ(ilogbf(NAN), FP_ILOGBNAN);
    CHECK_EQ(ilogbf(INFINITY), INT_MAX);
    CHECK(logbf(96.0f) == 6.0f && logbf(-0.1f) == -4.0f);
    CHECK_INF(logbf(0.0f), 1);
    CHECK_INF(logbf(-INFINITY), 0);
    CHECK_NAN(logbf(NAN));
    CHECK(float_bits(nextafterf(1.0f, 2.0f)) == float_bits(1.0f) + 1u);
    CHECK(float_bits(nextafterf(1.0f, 0.0f)) == float_bits(1.0f) - 1u);
    CHECK(float_bits(nextafterf(-1.0f, -2.0f)) == float_bits(-1.0f) + 1u);
    CHECK(nextafterf(0.0f, 1.0f) == float_from_bits(1u));
    CHECK(nextafterf(0.0f, -1.0f) == -float_from_bits(1u));
    CHECK_ZERO(nextafterf(0.0f, -0.0f), 1);                        // equal: y, sign and all
    CHECK(nextafterf(2.0f, 2.0f) == 2.0f);
    errno = 0;
    CHECK_INF(nextafterf(FLT_MAX, INFINITY), 0);
    CHECK_EQ(errno, ERANGE);
    CHECK_NAN(nextafterf(NAN, 1.0f));
    CHECK(nexttowardf(1.0f, 2.0) == nextafterf(1.0f, 2.0f) && nextafterf(1.0f, 0.0f) < 1.0f);

    TEST_SECTION("erf, erfc, tgamma, lgamma");
    CHECK_ZERO(erff(0.0f), 0);
    CHECK_ZERO(erff(-0.0f), 1);
    CHECK(erff(INFINITY) == 1.0f && erff(-INFINITY) == -1.0f && erff(10.0f) == 1.0f);
    CHECK(erfcf(INFINITY) == 0.0f && erfcf(-INFINITY) == 2.0f);
    CHECK_NAN(erff(NAN));
    CHECK_NAN(erfcf(NAN));
    for (int i = -40; i <= 40; i++)
    {
        float x = (float)i * 0.1f;                                 // they add up to 1, far from the tail
        CHECK(fabsf(erff(x) + erfcf(x) - 1.0f) < 2.5e-7f);
        CHECK(erff(-x) == -erff(x));
    }
    CHECK(erfcf(9.5f) > 0.0f && erfcf(9.5f) < 1e-40f);             // subnormal, but there
    CHECK(tgammaf(1.0f) == 1.0f && tgammaf(2.0f) == 1.0f && tgammaf(5.0f) == 24.0f && tgammaf(11.0f) == 3628800.0f);
    CHECK(tgammaf(13.0f) == 479001600.0f);                         // whole numbers are exact factorials
    CHECK_REL(tgammaf(0.5f), 1.77245385f, 3e-7f);                  // sqrt(pi)
    errno = 0;
    CHECK_INF(tgammaf(0.0f), 0);
    CHECK_EQ(errno, ERANGE);
    CHECK_INF(tgammaf(-0.0f), 1);
    errno = 0;
    CHECK_NAN(tgammaf(-2.0f));
    CHECK_EQ(errno, EDOM);
    CHECK_NAN(tgammaf(-INFINITY));
    CHECK_INF(tgammaf(INFINITY), 0);
    errno = 0;
    CHECK_INF(tgammaf(36.0f), 0);
    CHECK_EQ(errno, ERANGE);
    CHECK(tgammaf(-50.5f) == 0.0f);                                // underflows
    CHECK(tgammaf(-2.5f) < 0.0f && tgammaf(-3.5f) > 0.0f);          // the sign alternates between the poles
    CHECK_ZERO(lgammaf(1.0f), 0);
    CHECK_ZERO(lgammaf(2.0f), 0);
    (void)lgammaf(-2.5f);
    CHECK_EQ(signgam, -1);
    (void)lgammaf(-3.5f);
    CHECK_EQ(signgam, 1);
    (void)lgammaf(-40.5f);                                         // by reflection
    CHECK_EQ(signgam, -1);
    CHECK_REL(lgammaf(-40.5f), -111.029647f, 3e-6f);
    errno = 0;
    CHECK_INF(lgammaf(0.0f), 0);
    CHECK_EQ(errno, ERANGE);
    CHECK_INF(lgammaf(-3.0f), 0);
    CHECK_INF(lgammaf(-INFINITY), 0);
    CHECK(erff(0.5f) == erff(0.5f) && tgammaf(3.0f) == 2.0f && lgammaf(3.0f) == lgammaf(3.0f));
}

static void section_inverse_trigonometric_quadrants_and_specials(void)
{
    TEST_SECTION("inverse trigonometric: quadrants and specials");
    CHECK_REL(atan2f(1.0f, 1.0f), M_PI_4, 1e-7f);
    CHECK_REL(atan2f(1.0f, -1.0f), 3.0f * M_PI_4, 3e-7f);
    CHECK_REL(atan2f(-1.0f, -1.0f), -3.0f * M_PI_4, 3e-7f);
    CHECK_REL(atan2f(-1.0f, 1.0f), -M_PI_4, 1e-7f);
    CHECK_ZERO(atan2f(0.0f, 1.0f), 0);
    CHECK_ZERO(atan2f(-0.0f, 1.0f), 1);
    CHECK_REL(atan2f(0.0f, -1.0f), M_PI, 1e-7f);
    CHECK_REL(atan2f(-0.0f, -1.0f), -M_PI, 1e-7f);
    CHECK_REL(atan2f(0.0f, -0.0f), M_PI, 1e-7f);
    CHECK_ZERO(atan2f(0.0f, 0.0f), 0);
    CHECK_REL(atan2f(1.0f, 0.0f), M_PI_2, 1e-7f);
    CHECK_REL(atan2f(-1.0f, 0.0f), -M_PI_2, 1e-7f);
    CHECK_REL(atan2f(INFINITY, INFINITY), M_PI_4, 1e-7f);
    CHECK_REL(atan2f(INFINITY, -INFINITY), 3.0f * M_PI_4, 3e-7f);
    CHECK_ZERO(atan2f(1.0f, INFINITY), 0);
    CHECK_REL(atan2f(1.0f, -INFINITY), M_PI, 1e-7f);
    CHECK_REL(atan2f(INFINITY, 5.0f), M_PI_2, 1e-7f);
    CHECK_NAN(atan2f(NAN, 1.0f));
    CHECK_NAN(atan2f(1.0f, NAN));
    CHECK_REL(atanf(INFINITY), M_PI_2, 1e-7f);
    CHECK_REL(atanf(-INFINITY), -M_PI_2, 1e-7f);
    CHECK_NAN(atanf(NAN));
    CHECK_ZERO(atanf(0.0f), 0);
    CHECK_REL(asinf(1.0f), M_PI_2, 1e-7f);
    CHECK_REL(asinf(-1.0f), -M_PI_2, 1e-7f);
    CHECK_ZERO(acosf(1.0f), 0);
    CHECK_REL(acosf(-1.0f), M_PI, 1e-7f);
    CHECK_REL(acosf(0.0f), M_PI_2, 1e-7f);
}

static void section_domain_and_range_errors(void)
{
    TEST_SECTION("domain and range errors");
    CHECK_ERRNO(fmodf(5.0f, 0.0f), EDOM);            // the instruction traps and would give back 5
    CHECK_NAN(fmodf(5.0f, 0.0f));
    CHECK_NAN(fmodf(5.0f, -0.0f));
    CHECK_ERRNO(fmodf(INFINITY, 2.0f), EDOM);
    CHECK_NAN(fmodf(-INFINITY, 2.0f));
    CHECK(fmodf(5.5f, INFINITY) == 5.5f && fmodf(-5.5f, -INFINITY) == -5.5f);   // an infinite y leaves x
    CHECK_NAN(fmodf(NAN, 2.0f));
    CHECK_NAN(fmodf(2.0f, NAN));
    CHECK_NAN(fmodf(NAN, 0.0f));
    CHECK_ERRNO(fmodf(NAN, 0.0f), 0);                // NaN in is not a domain error of its own
    CHECK_ERRNO(logf(-1.0f), EDOM);
    CHECK_NAN(logf(-1.0f));
    CHECK_ERRNO(logf(0.0f), ERANGE);
    CHECK_INF(logf(0.0f), 1);
    CHECK_INF(logf(INFINITY), 0);
    CHECK_NAN(logf(NAN));
    CHECK_ERRNO(log2f(-4.0f), EDOM);
    CHECK_ERRNO(log10f(0.0f), ERANGE);
    CHECK_ERRNO(log1pf(-2.0f), EDOM);
    CHECK_ERRNO(log1pf(-1.0f), ERANGE);
    CHECK_INF(log1pf(-1.0f), 1);
    CHECK_ERRNO(asinf(2.0f), EDOM);
    CHECK_NAN(asinf(-1.5f));
    CHECK_ERRNO(acosf(-2.0f), EDOM);
    CHECK_ERRNO(acoshf(0.5f), EDOM);
    CHECK_ERRNO(atanhf(2.0f), EDOM);
    CHECK_ERRNO(atanhf(1.0f), ERANGE);
    CHECK_INF(atanhf(1.0f), 0);
    CHECK_INF(atanhf(-1.0f), 1);
    CHECK_ERRNO(sinf(INFINITY), EDOM);
    CHECK_NAN(cosf(-INFINITY));
    CHECK_NAN(tanf(INFINITY));
    CHECK_NAN(sinf(NAN));
    CHECK_ERRNO(expf(1000.0f), ERANGE);
    CHECK_INF(expf(1000.0f), 0);
    CHECK_ERRNO(expf(-1000.0f), ERANGE);
    CHECK_ZERO(expf(-1000.0f), 0);
    CHECK_ERRNO(expf(INFINITY), 0);                       // exp(inf) = inf is not an error
    CHECK_INF(expf(INFINITY), 0);
    CHECK_ZERO(expf(-INFINITY), 0);
    CHECK_ERRNO(exp2f(1000.0f), ERANGE);
    CHECK_ERRNO(exp2f(-1000.0f), ERANGE);
    CHECK_ERRNO(expf(1.0f), 0);                           // and a good call leaves errno alone
    CHECK_ERRNO(sinf(1.0f), 0);
    CHECK_ERRNO(logf(2.0f), 0);
    CHECK_ERRNO(powf(2.0f, 10.0f), 0);
    CHECK_ERRNO(powf(-2.0f, 0.5f), EDOM);
    CHECK_NAN(powf(-2.0f, 0.5f));
    CHECK_ERRNO(powf(0.0f, -1.0f), ERANGE);
    CHECK_INF(powf(0.0f, -1.0f), 0);
    CHECK_ERRNO(powf(10.0f, 39.0f), ERANGE);
    CHECK_INF(powf(10.0f, 39.0f), 0);
    CHECK_ERRNO(powf(10.0f, -50.0f), ERANGE);
    CHECK_ZERO(powf(10.0f, -50.0f), 0);
    CHECK_ERRNO(powf(2.0f, 1000.0f), ERANGE);
    CHECK_ERRNO(sinhf(100.0f), ERANGE);
    CHECK_INF(sinhf(100.0f), 0);
    CHECK_INF(sinhf(-100.0f), 1);
    CHECK_INF(coshf(100.0f), 0);
    CHECK_ERRNO(ldexpf(1.0f, 200), ERANGE);
    CHECK_ERRNO(hypotf(3.0e38f, 2.5e38f), ERANGE);
    CHECK_INF(hypotf(3.0e38f, 2.5e38f), 0);
    CHECK_ERRNO(remainderf(1.0f, 0.0f), EDOM);
    CHECK_ERRNO(lroundf(3.0e9f), ERANGE);
    CHECK_ERRNO(lrintf(NAN), EDOM);
}

static void section_exp_and_log(void)
{
    TEST_SECTION("exp and log");
    CHECK(expf(0.0f) == 1.0f);
    CHECK_REL(expf(1.0f), M_E, 1e-7f);
    CHECK(logf(1.0f) == 0.0f);
    CHECK_REL(logf(M_E), 1.0f, 3e-7f);
    CHECK(log2f(8.0f) == 3.0f);
    CHECK(log2f(0.25f) == -2.0f);
    CHECK(log2f(1.0f) == 0.0f);
    CHECK_REL(log10f(1000.0f), 3.0f, 3e-7f);
    CHECK_REL(log10f(0.001f), -3.0f, 3e-7f);
    CHECK(exp2f(0.0f) == 1.0f && exp2f(10.0f) == 1024.0f && exp2f(-1.0f) == 0.5f && exp2f(3.0f) == 8.0f);
    CHECK(exp2f(-149.0f) == FLT_TRUE_MIN);
    CHECK(exp2f(127.0f) == 1.70141183e38f);
    CHECK(expm1f(0.0f) == 0.0f);
    CHECK(log1pf(0.0f) == 0.0f);
    CHECK_REL(expm1f(1e-7f), 1e-7f, 1e-6f);               // no cancellation for tiny arguments
    CHECK_REL(log1pf(1e-7f), 1e-7f, 1e-6f);
    CHECK(expf(-104.0f) >= 0.0f);                         // deep in the subnormals: no crash, no negative
    CHECK(logf(FLT_MIN) < -87.0f && logf(FLT_MIN) > -88.0f);
    CHECK(logf(FLT_TRUE_MIN) < -103.0f && logf(FLT_TRUE_MIN) > -104.0f);   // a subnormal argument works
    CHECK_REL(logf(FLT_MAX), 88.7228394f, 3e-7f);
    {
        float worst = 0.0f;
        for (int i = 1; i <= 400; i++)
        {
            float x = (float)i * 0.05f;
            float e1 = rel_err(expf(logf(x)), x);
            float e2 = rel_err(logf(expf(1.0f + x * 0.5f)), 1.0f + x * 0.5f);   // exp of 1..11 keeps log's argument away from 1
            if (e2 > worst) worst = e2;
            if (e1 > worst) worst = e1;
        }
        CHECK(worst < 3e-6f);
    }
}

static void section_pow(void)
{
    TEST_SECTION("pow");
    CHECK(powf(0.0f, 0.0f) == 1.0f);
    CHECK(powf(2.0f, 0.0f) == 1.0f);
    CHECK(powf(NAN, 0.0f) == 1.0f);
    CHECK(powf(1.0f, NAN) == 1.0f);
    CHECK(powf(1.0f, INFINITY) == 1.0f);
    CHECK(powf(-1.0f, INFINITY) == 1.0f);
    CHECK_NAN(powf(NAN, 1.0f));
    CHECK_NAN(powf(2.0f, NAN));
    CHECK_INF(powf(2.0f, INFINITY), 0);
    CHECK_ZERO(powf(0.5f, INFINITY), 0);
    CHECK_ZERO(powf(2.0f, -INFINITY), 0);
    CHECK_INF(powf(0.5f, -INFINITY), 0);
    CHECK_ZERO(powf(INFINITY, -1.0f), 0);
    CHECK_INF(powf(INFINITY, 2.0f), 0);
    CHECK_INF(powf(-INFINITY, 3.0f), 1);
    CHECK_INF(powf(-INFINITY, 2.0f), 0);
    CHECK_ZERO(powf(0.0f, 3.0f), 0);
    CHECK_ZERO(powf(-0.0f, 3.0f), 1);
    CHECK_ZERO(powf(-0.0f, 2.0f), 0);
    CHECK_INF(powf(-0.0f, -3.0f), 1);
    CHECK_INF(powf(-0.0f, -2.0f), 0);
    CHECK(powf(-8.0f, 3.0f) == -512.0f);
    CHECK(powf(-2.0f, 4.0f) == 16.0f);
    CHECK(powf(2.0f, 10.0f) == 1024.0f);
    CHECK(powf(2.0f, -1.0f) == 0.5f);
    CHECK(powf(3.0f, 3.0f) == 27.0f);
    CHECK(powf(10.0f, 2.0f) == 100.0f);
    CHECK_REL(powf(2.0f, 0.5f), M_SQRT2, 3e-7f);
    CHECK_REL(powf(-2.0f, -3.0f), -0.125f, 3e-7f);
}

static void section_hyperbolic_roots_and_hypot(void)
{
    TEST_SECTION("hyperbolic, roots and hypot");
    CHECK_ZERO(sinhf(0.0f), 0);
    CHECK_ZERO(sinhf(-0.0f), 1);
    CHECK(coshf(0.0f) == 1.0f);
    CHECK_ZERO(tanhf(0.0f), 0);
    CHECK(tanhf(20.0f) == 1.0f && tanhf(-20.0f) == -1.0f);
    CHECK_INF(sinhf(INFINITY), 0);
    CHECK_INF(coshf(-INFINITY), 0);
    CHECK(tanhf(INFINITY) == 1.0f);
    CHECK_INF(asinhf(INFINITY), 0);
    CHECK_INF(acoshf(INFINITY), 0);
    CHECK_ZERO(asinhf(0.0f), 0);
    CHECK_ZERO(acoshf(1.0f), 0);
    CHECK_ZERO(atanhf(0.0f), 0);
    CHECK_ZERO(cbrtf(0.0f), 0);
    CHECK_ZERO(cbrtf(-0.0f), 1);
    CHECK_REL(cbrtf(-8.0f), -2.0f, 1e-7f);
    CHECK_REL(cbrtf(27.0f), 3.0f, 1e-7f);
    CHECK_INF(cbrtf(-INFINITY), 1);
    CHECK_NAN(cbrtf(NAN));
    CHECK(hypotf(3.0f, 4.0f) == 5.0f);
    CHECK(hypotf(0.0f, -7.0f) == 7.0f);
    CHECK_INF(hypotf(INFINITY, NAN), 0);
    CHECK_NAN(hypotf(NAN, 1.0f));
    CHECK_REL(hypotf(1.0e30f, 1.0e30f), 1.41421356e30f, 3e-7f);   // x*x alone would overflow
    {
        float worst = 0.0f;
        for (int i = 1; i <= 200; i++)
        {
            float x = (float)i * 1.37f;
            float c = cbrtf(x);
            float e = rel_err(c * c * c, x);
            float e2 = rel_err(sinhf(x * 0.01f) / coshf(x * 0.01f), tanhf(x * 0.01f));
            if (e > worst) worst = e;
            if (e2 > worst) worst = e2;
        }
        CHECK(worst < 1e-6f);
    }
}

static void section_ldexp_frexp_modf(void)
{
    TEST_SECTION("ldexp, frexp, modf");
    CHECK(ldexpf(1.0f, 10) == 1024.0f);
    CHECK(ldexpf(3.0f, -1) == 1.5f);
    CHECK(ldexpf(-1.5f, 4) == -24.0f);
    CHECK(ldexpf(1.0f, -149) == FLT_TRUE_MIN);
    CHECK(ldexpf(0.0f, 100) == 0.0f);
    CHECK_INF(ldexpf(1.0f, 200), 0);
    CHECK_INF(ldexpf(INFINITY, -5), 0);
    CHECK(scalbnf(1.0f, 3) == 8.0f);
    int e = 99;
    CHECK(frexpf(8.0f, &e) == 0.5f && e == 4);
    CHECK(frexpf(1.0f, &e) == 0.5f && e == 1);
    CHECK(frexpf(-0.75f, &e) == -0.75f && e == 0);
    CHECK(frexpf(0.0f, &e) == 0.0f && e == 0);
    CHECK(frexpf(0.1f, &e) > 0.5f && e == -3);
    CHECK(frexpf(FLT_MAX, &e) < 1.0f && e == 128);
    {
        float sub = FLT_TRUE_MIN * 12345.0f;              // a subnormal
        float m = frexpf(sub, &e);
        CHECK(m >= 0.5f && m < 1.0f);
        CHECK(ldexpf(m, e) == sub);
        int bad = 0;
        for (int i = 1; i <= 120; i++)                    // frexp then ldexp gives the number back
        {
            float x = (float)i * 3.7f - 200.0f;
            float mm = frexpf(x, &e);
            if (ldexpf(mm, e) != x) bad++;
            if (x != 0.0f && (fabsf(mm) < 0.5f || fabsf(mm) >= 1.0f)) bad++;
        }
        CHECK_EQ(bad, 0);
    }
    float ip;
    CHECK(modff(3.75f, &ip) == 0.75f && ip == 3.0f);
    CHECK(modff(-3.75f, &ip) == -0.75f && ip == -3.0f);
    CHECK(modff(5.0f, &ip) == 0.0f && ip == 5.0f);
    CHECK_ZERO(modff(-5.0f, &ip), 1);
    CHECK(ip == -5.0f);
    CHECK_ZERO(modff(INFINITY, &ip), 0);
    CHECK(isinf(ip));
    CHECK(isnan(modff(NAN, &ip)) && isnan(ip));
    CHECK(modff(0.25f, &ip) == 0.25f && ip == 0.0f);
}

static void section_rounding(void)
{
    TEST_SECTION("rounding");
    CHECK_ZERO(nearbyintf(0.5f), 0);                       // halves go to even
    CHECK(nearbyintf(1.5f) == 2.0f && nearbyintf(2.5f) == 2.0f && nearbyintf(3.5f) == 4.0f);
    CHECK_ZERO(nearbyintf(-0.5f), 1);
    CHECK(nearbyintf(-1.5f) == -2.0f && nearbyintf(-2.5f) == -2.0f && nearbyintf(-3.5f) == -4.0f);
    CHECK(nearbyintf(2.4f) == 2.0f && nearbyintf(2.6f) == 3.0f && nearbyintf(-2.6f) == -3.0f);
    CHECK(nearbyintf(8388608.0f) == 8388608.0f && nearbyintf(1.0e10f) == 1.0e10f);
    CHECK(rintf(2.5f) == 2.0f && rintf(3.5f) == 4.0f);
    CHECK(isinf(nearbyintf(INFINITY)) && isnan(nearbyintf(NAN)));
    CHECK_EQ(lroundf(0.5f), 1);                            // halves go away from zero
    CHECK_EQ(lroundf(1.5f), 2);
    CHECK_EQ(lroundf(2.5f), 3);
    CHECK_EQ(lroundf(-0.5f), -1);
    CHECK_EQ(lroundf(-2.5f), -3);
    CHECK_EQ(lroundf(2.4f), 2);
    CHECK_EQ(lroundf(2147483000.0f), 2147483008);
    CHECK_EQ(lroundf(3.0e9f), INT_MAX);
    CHECK_EQ(lroundf(-3.0e9f), INT_MIN);
    CHECK_EQ(lrintf(2.5f), 2);
    CHECK_EQ(lrintf(3.5f), 4);
    CHECK_EQ(lrintf(-2.5f), -2);
    CHECK_EQ(lrintf(NAN), 0);
}

static void section_remainder_fdim_nan(void)
{
    TEST_SECTION("llround and llrint: 64 bits");
    CHECK_EQ64(llroundf(3.0e9f), 3000000000LL);                  // past what an int holds
    CHECK_EQ64(llroundf(-3.0e9f), -3000000000LL);
    CHECK_EQ64(llroundf(2.5f), 3LL);                             // halves away from zero
    CHECK_EQ64(llroundf(-2.5f), -3LL);
    CHECK_EQ64(llrintf(2.5f), 2LL);                              // halves to even
    CHECK_EQ64(llrintf(3.5f), 4LL);
    CHECK_EQ64(llrintf(1.0e18f), (long long)1.0e18f);
    CHECK_EQ64(llroundf(-9223372036854775808.0f), -9223372036854775807LL - 1);   // the bottom edge fits
    CHECK_ERRNO(llroundf(-9223372036854775808.0f), 0);             // ... and is not out of range
    CHECK_EQ64(llrintf(-9223372036854775808.0f), -9223372036854775807LL - 1);
    CHECK_ERRNO(llrintf(-9223372036854775808.0f), 0);
    CHECK_ERRNO(llroundf(9223372036854775808.0f), ERANGE);         // 2^63 does not
    CHECK_EQ64(llroundf(9223372036854775808.0f), 9223372036854775807LL);
    CHECK_EQ64(llrintf(-1.0e19f), -9223372036854775807LL - 1);
    CHECK_ERRNO(llrintf(-1.0e19f), ERANGE);
    CHECK_ERRNO(llroundf(NAN), EDOM);
    CHECK_ERRNO(llrintf(3.0e9f), 0);

    TEST_SECTION("remainder, fdim, nan");
    CHECK(remainderf(5.0f, 3.0f) == -1.0f);                // 5/3 rounds to 2
    CHECK(remainderf(7.0f, 2.0f) == -1.0f);                // 3.5 rounds to the even 4
    CHECK(remainderf(5.0f, 2.0f) == 1.0f);                 // 2.5 rounds to the even 2
    CHECK(remainderf(-5.0f, 3.0f) == 1.0f);
    CHECK(remainderf(10.0f, 5.0f) == 0.0f);
    CHECK(remainderf(1.5f, 1.0f) == -0.5f);
    CHECK(remainderf(2.5f, 1.0f) == 0.5f);
    CHECK(remainderf(0.5f, 1.0f) == 0.5f);
    CHECK(remainderf(3.0f, INFINITY) == 3.0f);
    CHECK_NAN(remainderf(INFINITY, 2.0f));
    CHECK_NAN(remainderf(NAN, 2.0f));
    CHECK(fdimf(5.0f, 3.0f) == 2.0f && fdimf(3.0f, 5.0f) == 0.0f && fdimf(3.0f, 3.0f) == 0.0f);
    CHECK_NAN(fdimf(NAN, 1.0f));
    CHECK(isnan(nanf("")));
    CHECK(isnan(nanf("123")));
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
    TABLE_1(sin, 44, 3e-7f);
    TABLE_1(cos, 44, 3e-7f);
    TABLE_1(tan, 26, 5e-7f);
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

    TEST_SECTION("tables: error and gamma functions");
    TABLE_1(erf, 20, 3e-7f);
    TABLE_1(erfc, 18, 5e-7f);
    TABLE_1(tgamma, 23, 2e-6f);
    TABLE_1(lgamma, 21, 3e-6f);

#ifndef MATH_REPORT
    section_classification();
    section_the_one_instruction_functions();
    section_macros_and_functions_agree();
    section_trigonometric_identities();
    section_exponent_next_error_and_gamma();
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
