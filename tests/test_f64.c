// The machine's binary64 (fadd.d, fdiv.d, fsqrt.d, fcmp.d, fcvt...) against node's, bit for bit: add, subtract,
// multiply, divide, square root, compare and the conversions, on operands of every kind - normal, subnormal, near 1,
// huge, cancelling, special (tests/f64_vectors.inc, made by tools/gen_f64_vectors.js). A NaN result only has to be
// a NaN. The vectors hold bit patterns, so every value goes in and out of a double through a union.
#include "ceres/test.h"
#include "stdio.h"
#include "math.h"

#include "f64_vectors.inc"

typedef unsigned long long u64;

static double d_of(u64 bits)
{
    union { u64 u; double d; } v;
    v.u = bits;
    return v.d;
}

static u64 bits_of(double d)
{
    union { u64 u; double d; } v;
    v.d = d;
    return v.u;
}

static int is_nan64(u64 a)
{
    return (a & 0x7FFFFFFFFFFFFFFFull) > 0x7FF0000000000000ull;
}

static int shown;

static int same(double result, u64 want, const char* op, u64 a, u64 b)
{
    u64 got = bits_of(result);
    if (got == want || (is_nan64(got) && is_nan64(want)))
        return 1;
    if (shown++ < 8)
        printf("  %s %016llx %016llx: got %016llx, want %016llx\n", op, a, b, got, want);
    return 0;
}

// -1, 0 or 1 as a is below, equal to or above b; 2 when either is a NaN.
static int order(double a, double b)
{
    if (a < b) return -1;
    if (a > b) return 1;
    if (a == b) return 0;
    return 2;
}

int main(void)
{
    TEST_SECTION("arithmetic");
    int wrong = 0;
    for (int i = 0; i < V_ADD_COUNT; i++)
        wrong += !same(d_of(v_add[i][0]) + d_of(v_add[i][1]), v_add[i][2], "add", v_add[i][0], v_add[i][1]);
    CHECK_EQ(wrong, 0);
    wrong = 0;
    for (int i = 0; i < V_SUB_COUNT; i++)
        wrong += !same(d_of(v_sub[i][0]) - d_of(v_sub[i][1]), v_sub[i][2], "sub", v_sub[i][0], v_sub[i][1]);
    CHECK_EQ(wrong, 0);
    wrong = 0;
    for (int i = 0; i < V_MUL_COUNT; i++)
        wrong += !same(d_of(v_mul[i][0]) * d_of(v_mul[i][1]), v_mul[i][2], "mul", v_mul[i][0], v_mul[i][1]);
    CHECK_EQ(wrong, 0);
    wrong = 0;
    for (int i = 0; i < V_DIV_COUNT; i++)
    {
        double divisor = d_of(v_div[i][1]);
        if (divisor == 0.0)
            continue;   // fdiv.d by zero traps instead of giving an infinity (CeresASM docs/34-64-bit.md)
        wrong += !same(d_of(v_div[i][0]) / divisor, v_div[i][2], "div", v_div[i][0], v_div[i][1]);
    }
    CHECK_EQ(wrong, 0);
    wrong = 0;
    for (int i = 0; i < V_SQRT_COUNT; i++)
        wrong += !same(sqrt(d_of(v_sqrt[i][0])), v_sqrt[i][1], "sqrt", v_sqrt[i][0], 0);
    CHECK_EQ(wrong, 0);
    CHECK(bits_of(1.0 / 3.0) == 0x3FD5555555555555ull);
    CHECK(bits_of(-d_of(0x3FF0000000000000ull)) == 0xBFF0000000000000ull);
    CHECK(bits_of(-0.0 + -0.0) == 0x8000000000000000ull);
    CHECK(bits_of(1.0 - 1.0) == 0);                                                   // +0

    TEST_SECTION("compare");
    wrong = 0;
    for (int i = 0; i < V_CMP_COUNT; i++)
        wrong += order(d_of(v_cmp[i][0]), d_of(v_cmp[i][1])) != (int)v_cmp[i][2];
    CHECK_EQ(wrong, 0);
    CHECK_EQ(order(0.0, -0.0), 0);                                                    // +0 == -0
    CHECK_EQ(order(NAN, NAN), 2);

    TEST_SECTION("conversions");
    wrong = 0;
    for (int i = 0; i < V_TO_F32_COUNT; i++)
    {
        unsigned int got = float_bits((float)d_of(v_to_f32[i][0]));
        unsigned int want = (unsigned int)v_to_f32[i][1];
        int nan = (want & 0x7FFFFFFFu) > 0x7F800000u;
        if (got != want && !(nan && (got & 0x7FFFFFFFu) > 0x7F800000u))
        {
            wrong++;
            if (shown++ < 8)
                printf("  to_f32 %016llx: got %08x, want %08x\n", v_to_f32[i][0], got, want);
        }
    }
    CHECK_EQ(wrong, 0);
    wrong = 0;
    for (int i = 0; i < V_FROM_F32_COUNT; i++)
        wrong += !same((double)float_from_bits((unsigned int)v_from_f32[i][0]), v_from_f32[i][1], "from_f32", v_from_f32[i][0], 0);
    CHECK_EQ(wrong, 0);
    wrong = 0;
    for (int i = 0; i < V_FROM_INT_COUNT; i++)
    {
        wrong += !same((double)(long long)v_from_int[i][0], v_from_int[i][1], "from_i64", v_from_int[i][0], 0);
        wrong += !same((double)v_from_int[i][0], v_from_int[i][2], "from_u64", v_from_int[i][0], 0);
    }
    CHECK_EQ(wrong, 0);
    wrong = 0;
    for (int i = 0; i < V_TO_INT_COUNT; i++)
    {
        // fcvt truncates toward zero and saturates at the ends of the range, with NaN as 0 (SPEC 6.5).
        double d = d_of(v_to_int[i][0]);
        u64 as_signed = (u64)(long long)d;
        u64 as_unsigned = (u64)d;
        if (as_signed != v_to_int[i][1] || as_unsigned != v_to_int[i][2])
        {
            wrong++;
            if (shown++ < 8)
                printf("  to_int %016llx: got %016llx %016llx\n", v_to_int[i][0], as_signed, as_unsigned);
        }
    }
    CHECK_EQ(wrong, 0);
    CHECK_EQ((int)(double)-123456, -123456);
    CHECK_EQ((int)d_of(0x41F0000000000000ull), 2147483647);                          // 2^32: clamped
    CHECK_EQ((int)(unsigned int)d_of(0xC000000000000000ull), 0);                      // -2
    CHECK(bits_of((double)4000000000u) == 0x41EDCD6500000000ull);
    CHECK((float)(double)0.1f == 0.1f);
    return test_summary();
}
