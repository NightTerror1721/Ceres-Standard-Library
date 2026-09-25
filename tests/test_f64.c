// The soft double (ceres/f64.h) against node's binary64, bit for bit: add, subtract, multiply, divide, square root,
// compare and the conversions, on operands of every kind - normal, subnormal, near 1, huge, cancelling, special
// (tests/f64_vectors.inc, made by tools/gen_f64_vectors.js). A NaN result only has to be a NaN.
#include "ceres/test.h"
#include "ceres/f64.h"
#include "stdio.h"
#include "math.h"

#include "f64_vectors.inc"

static int is_nan64(f64 a)
{
    return (a & 0x7FFFFFFFFFFFFFFFull) > F64_INFINITY;
}

static int shown;

static int same(f64 got, f64 want, const char* op, f64 a, f64 b)
{
    if (got == want || (is_nan64(got) && is_nan64(want)))
        return 1;
    if (shown++ < 8)
        printf("  %s %016llx %016llx: got %016llx, want %016llx\n", op, a, b, got, want);
    return 0;
}

static int check_binary(const char* op, f64 (*fn)(f64, f64), const unsigned long long (*v)[3], int count)
{
    int wrong = 0;
    for (int i = 0; i < count; i++)
        wrong += !same(fn(v[i][0], v[i][1]), v[i][2], op, v[i][0], v[i][1]);
    return wrong;
}

int main(void)
{
    TEST_SECTION("arithmetic");
    CHECK_EQ(check_binary("add", __f64_add, v_add, V_ADD_COUNT), 0);
    CHECK_EQ(check_binary("sub", __f64_sub, v_sub, V_SUB_COUNT), 0);
    CHECK_EQ(check_binary("mul", __f64_mul, v_mul, V_MUL_COUNT), 0);
    CHECK_EQ(check_binary("div", __f64_div, v_div, V_DIV_COUNT), 0);
    int wrong = 0;
    for (int i = 0; i < V_SQRT_COUNT; i++)
        wrong += !same(__f64_sqrt(v_sqrt[i][0]), v_sqrt[i][1], "sqrt", v_sqrt[i][0], 0);
    CHECK_EQ(wrong, 0);
    CHECK(__f64_div(__f64_from_i32(1), __f64_from_i32(3)) == 0x3FD5555555555555ull);
    CHECK(__f64_neg(F64_ONE) == 0xBFF0000000000000ull);
    CHECK(__f64_add(0x8000000000000000ull, 0x8000000000000000ull) == 0x8000000000000000ull);   // -0 + -0
    CHECK(__f64_sub(F64_ONE, F64_ONE) == 0);                                           // +0

    TEST_SECTION("compare");
    wrong = 0;
    for (int i = 0; i < V_CMP_COUNT; i++)
        wrong += __f64_cmp(v_cmp[i][0], v_cmp[i][1]) != (int)v_cmp[i][2];
    CHECK_EQ(wrong, 0);
    CHECK_EQ(__f64_cmp(0, 0x8000000000000000ull), 0);                                  // +0 == -0
    CHECK_EQ(__f64_cmp(F64_NAN, F64_NAN), 2);

    TEST_SECTION("conversions");
    wrong = 0;
    for (int i = 0; i < V_TO_F32_COUNT; i++)
    {
        unsigned int got = float_bits(__f64_to_f32(v_to_f32[i][0]));
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
        wrong += !same(__f64_from_f32(float_from_bits((unsigned int)v_from_f32[i][0])), v_from_f32[i][1], "from_f32", v_from_f32[i][0], 0);
    CHECK_EQ(wrong, 0);
    wrong = 0;
    for (int i = 0; i < V_FROM_INT_COUNT; i++)
    {
        wrong += !same(__f64_from_i64((long long)v_from_int[i][0]), v_from_int[i][1], "from_i64", v_from_int[i][0], 0);
        wrong += !same(__f64_from_u64(v_from_int[i][0]), v_from_int[i][2], "from_u64", v_from_int[i][0], 0);
    }
    CHECK_EQ(wrong, 0);
    wrong = 0;
    for (int i = 0; i < V_TO_INT_COUNT; i++)
    {
        if ((unsigned long long)__f64_to_i64(v_to_int[i][0]) != v_to_int[i][1] || __f64_to_u64(v_to_int[i][0]) != v_to_int[i][2])
        {
            wrong++;
            if (shown++ < 8)
                printf("  to_int %016llx: got %016llx %016llx\n", v_to_int[i][0], (unsigned long long)__f64_to_i64(v_to_int[i][0]), __f64_to_u64(v_to_int[i][0]));
        }
    }
    CHECK_EQ(wrong, 0);
    CHECK_EQ(__f64_to_i32(__f64_from_i32(-123456)), -123456);
    CHECK_EQ(__f64_to_i32(0x41F0000000000000ull), 2147483647);                          // 2^32: clamped
    CHECK_EQ((int)__f64_to_u32(0xC000000000000000ull), 0);                              // -2
    CHECK(__f64_from_u32(4000000000u) == 0x41EDCD6500000000ull);
    CHECK(__f64_to_f32(__f64_from_f32(0.1f)) == 0.1f);
    return test_summary();
}
