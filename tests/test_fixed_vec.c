// 16.16 fixed point and the vector helpers. Fixed-point results are exact integers (checked against
// big-integer arithmetic done outside the VM); the float helpers are compared with a tolerance.
#include "ceres/test.h"
#include "ceres/fixed.h"
#include "ceres/vecmath.h"

static void fixed_basic(void)
{
    TEST_SECTION("construction");
    CHECK_EQ(FIX_INT(3), 196608);
    CHECK_EQ(FIX_INT(-2), -131072);
    CHECK_EQ(FIX(0.5f), 32768);
    CHECK_EQ(FIX(1.0f), FIX_ONE);
    CHECK_EQ(FIX_TO_INT(FIX_INT(7)), 7);
    CHECK_EQ(FIX_TO_INT(FIX(2.75f)), 2);
    CHECK_EQ(FIX_TO_INT(FIX(-2.75f)), -3);                     // toward minus infinity
    CHECK_EQ(FIX_FRAC(FIX(2.75f)), 49152);
    CHECK_EQ(fx_add(FIX(1.5f), FIX(2.25f)), FIX(3.75f));
    CHECK_EQ(fx_sub(FIX(1.5f), FIX(2.25f)), FIX(-0.75f));
    CHECK_EQ(fx_abs(FIX(-3.5f)), FIX(3.5f));
    CHECK_EQ(fx_abs(FIX(3.5f)), FIX(3.5f));
    CHECK_EQ(fx_floor(FIX(2.75f)), FIX_INT(2));
    CHECK_EQ(fx_floor(FIX(-2.75f)), FIX_INT(-3));
    CHECK_EQ(fx_round(FIX(2.5f)), FIX_INT(3));
    CHECK_EQ(fx_round(FIX(2.49f)), FIX_INT(2));
    CHECK_EQ(fx_round(FIX(-2.75f)), FIX_INT(-3));

    TEST_SECTION("mul");
    CHECK_EQ(fx_mul(FIX_INT(3), FIX(0.5f)), 98304);            // 1.5
    CHECK_EQ(fx_mul(FIX_INT(3), FIX_INT(4)), FIX_INT(12));
    CHECK_EQ(fx_mul(FIX_INT(-3), FIX_INT(4)), FIX_INT(-12));
    CHECK_EQ(fx_mul(FIX_INT(-3), FIX_INT(-4)), FIX_INT(12));
    CHECK_EQ(fx_mul(FIX(0.5f), FIX(0.5f)), FIX(0.25f));
    CHECK_EQ(fx_mul(0, FIX_INT(1000)), 0);
    CHECK_EQ(fx_mul(FIX_ONE, 12345), 12345);                   // one is the identity
    CHECK_EQ(fx_mul(1, 1), 0);                                 // 2^-32 truncates to zero
    CHECK_EQ(fx_mul(FIX_INT(181), FIX_INT(181)), FIX_INT(32761));   // the intermediate product needs 48 bits

    TEST_SECTION("div");
    CHECK_EQ(fx_div(FIX_INT(1), FIX_INT(3)), 21845);
    CHECK_EQ(fx_div(FIX_INT(-1), FIX_INT(3)), -21845);
    CHECK_EQ(fx_div(FIX_INT(1), FIX_INT(-3)), -21845);
    CHECK_EQ(fx_div(FIX_INT(-1), FIX_INT(-3)), 21845);
    CHECK_EQ(fx_div(FIX_INT(7), FIX_INT(2)), 229376);          // 3.5
    CHECK_EQ(fx_div(FIX_INT(-7), FIX_INT(2)), -229376);
    CHECK_EQ(fx_div(FIX(0.5f), FIX(0.25f)), FIX_INT(2));
    CHECK_EQ(fx_div(0, FIX_INT(5)), 0);
    CHECK_EQ(fx_div(FIX_INT(6), FIX_ONE), FIX_INT(6));
    CHECK_EQ(fx_div(FIX_INT(1), 0x7FFFFFFF), 2);               // the smallest divisors stay exact
    CHECK_EQ(fx_div(FIX_INT(1), FIX_INT(30000)), 2);
    CHECK_EQ(fx_div(FIX_INT(30000), 66), FIX_MAX);             // 30000 / 0.001 does not fit: saturate
    CHECK_EQ(fx_div(FIX_INT(-30000), 66), FIX_MIN);
    CHECK_EQ(fx_div(FIX_INT(100), 655), 655720197);            // 100 / 0.01, and it fits
    CHECK_EQ(fx_div(FIX_INT(5), 0), FIX_MAX);                  // no trap on a zero divisor
    CHECK_EQ(fx_div(FIX_INT(-5), 0), FIX_MIN);
    CHECK_EQ(fx_div(FIX_INT(30000), FIX_INT(1)), FIX_INT(30000));
    fixed_t big = fx_div(FIX_INT(-32768), FIX_ONE);            // exactly the most negative value
    CHECK_EQ(big, FIX_MIN);
    CHECK_EQ(fx_mul(fx_div(FIX_INT(10), FIX_INT(3)), FIX_INT(3)), 655359);   // (10/3)*3 loses the last step, as expected

    TEST_SECTION("sqrt");
    CHECK_EQ(fx_sqrt(FIX_INT(4)), FIX_INT(2));
    CHECK_EQ(fx_sqrt(FIX_INT(9)), FIX_INT(3));
    CHECK_EQ(fx_sqrt(FIX_INT(1)), FIX_ONE);
    CHECK_EQ(fx_sqrt(FIX_INT(2)), 92681);
    CHECK_EQ(fx_sqrt(FIX_INT(3)), 113511);
    CHECK_EQ(fx_sqrt(FIX(0.5f)), 46340);
    CHECK_EQ(fx_sqrt(1), 256);                                 // sqrt(2^-16) = 2^-8
    CHECK_EQ(fx_sqrt(FIX_INT(10000)), FIX_INT(100));
    CHECK_EQ(fx_sqrt(0x7FFFFFFF), 11863283);
    CHECK_EQ(fx_sqrt(0), 0);
    CHECK_EQ(fx_sqrt(-FIX_INT(4)), 0);
    int exact = 1;                                             // floor(sqrt) for perfect squares, all the way up
    for (int i = 1; i < 181; i++)
        if (fx_sqrt(FIX_INT(i * i)) != FIX_INT(i)) exact = 0;
    CHECK(exact);

    TEST_SECTION("lerp and conversion");
    CHECK_EQ(fx_lerp(FIX_INT(10), FIX_INT(20), 0), FIX_INT(10));
    CHECK_EQ(fx_lerp(FIX_INT(10), FIX_INT(20), FIX_ONE), FIX_INT(20));
    CHECK_EQ(fx_lerp(FIX_INT(10), FIX_INT(20), FIX_HALF), FIX_INT(15));
    CHECK_EQ(fx_lerp(FIX_INT(20), FIX_INT(10), FIX_HALF), FIX_INT(15));
    CHECK_NEAR(fx_to_float(FIX(1.5f)), 1.5f, 0.00001f);
    CHECK_NEAR(fx_to_float(FIX_INT(-100)), -100.0f, 0.00001f);
    CHECK_EQ(fx_from_float(2.5f), FIX(2.5f));
    CHECK_EQ(fx_from_float(-2.5f), FIX(-2.5f));
    CHECK_EQ(fx_from_float(1000000.0f), FIX_MAX);              // outside the range: saturate
    CHECK_EQ(fx_from_float(-1000000.0f), FIX_MIN);
    CHECK_EQ(fx_from_float(0.0f), 0);
}

static void fixed_trig(void)
{
    TEST_SECTION("sin and cos at the quarter turns");
    CHECK_EQ(fx_sin(0), 0);
    CHECK_EQ(fx_sin(64), FIX_ONE);
    CHECK_EQ(fx_sin(128), 0);
    CHECK_EQ(fx_sin(192), -FIX_ONE);
    CHECK_EQ(fx_cos(0), FIX_ONE);
    CHECK_EQ(fx_cos(64), 0);
    CHECK_EQ(fx_cos(128), -FIX_ONE);
    CHECK_EQ(fx_cos(192), 0);
    CHECK_EQ(fx_sin(32), 46341);                               // sin 45 degrees
    CHECK_EQ(fx_sin(16), 25080);

    TEST_SECTION("symmetry and wrapping");
    int sym = 1, wrap = 1, neg = 1, cosine = 1;
    for (int a = 0; a < 256; a++)
    {
        if (fx_sin(a + 128) != -fx_sin(a)) sym = 0;            // half a turn flips the sign
        if (fx_sin(a + 256) != fx_sin(a)) wrap = 0;
        if (fx_sin(a - 256) != fx_sin(a)) wrap = 0;
        if (fx_sin(-a) != -fx_sin(a)) neg = 0;
        if (fx_cos(a) != fx_sin(a + 64)) cosine = 0;
    }
    CHECK(sym);
    CHECK(wrap);
    CHECK(neg);
    CHECK(cosine);
    CHECK_EQ(fx_sin(1000), fx_sin(1000 & 255));

    TEST_SECTION("against the float sine");
    int close = 1;
    for (int a = 0; a < 256; a++)
    {
        float want = sin(6.28318531f * (float)a / 256.0f);
        float have = fx_to_float(fx_sin(a));
        float d = have - want;
        if (d < 0.0f) d = -d;
        if (d > 0.00003f) close = 0;
    }
    CHECK(close);
    int unit = 1;                                              // sin^2 + cos^2 stays within a few steps of 1
    for (int a = 0; a < 256; a++)
    {
        fixed_t s = fx_sin(a), c = fx_cos(a);
        fixed_t sum = fx_mul(s, s) + fx_mul(c, c);
        if (sum < FIX_ONE - 4 || sum > FIX_ONE + 4) unit = 0;
    }
    CHECK(unit);
}

static void vectors(void)
{
    TEST_SECTION("vec2");
    struct vec2 a = vec2_make(3.0f, 4.0f);
    struct vec2 b = vec2_make(-1.0f, 2.0f);
    struct vec2 s = vec2_add(a, b);
    CHECK_NEAR(s.x, 2.0f, 0.00001f);
    CHECK_NEAR(s.y, 6.0f, 0.00001f);
    struct vec2 d = vec2_sub(a, b);
    CHECK_NEAR(d.x, 4.0f, 0.00001f);
    CHECK_NEAR(d.y, 2.0f, 0.00001f);
    struct vec2 k = vec2_scale(a, 0.5f);
    CHECK_NEAR(k.x, 1.5f, 0.00001f);
    CHECK_NEAR(k.y, 2.0f, 0.00001f);
    CHECK_NEAR(vec2_dot(a, b), 5.0f, 0.00001f);
    CHECK_NEAR(vec2_len2(a), 25.0f, 0.00001f);
    CHECK_NEAR(vec2_len(a), 5.0f, 0.00001f);
    struct vec2 n = vec2_normalize(a);
    CHECK_NEAR(n.x, 0.6f, 0.00001f);
    CHECK_NEAR(n.y, 0.8f, 0.00001f);
    struct vec2 z = vec2_normalize(vec2_make(0.0f, 0.0f));
    CHECK_NEAR(z.x, 0.0f, 0.0f);
    CHECK_NEAR(z.y, 0.0f, 0.0f);
    struct vec2 r = vec2_rotate(vec2_make(1.0f, 0.0f), M_PI_2);
    CHECK_NEAR(r.x, 0.0f, 0.00001f);
    CHECK_NEAR(r.y, 1.0f, 0.00001f);
    struct vec2 r2 = vec2_rotate(vec2_make(2.0f, 1.0f), M_PI);
    CHECK_NEAR(r2.x, -2.0f, 0.00001f);
    CHECK_NEAR(r2.y, -1.0f, 0.00001f);

    TEST_SECTION("vec3");
    struct vec3 i = vec3_make(1.0f, 0.0f, 0.0f);
    struct vec3 j = vec3_make(0.0f, 1.0f, 0.0f);
    struct vec3 c = vec3_cross(i, j);
    CHECK_NEAR(c.x, 0.0f, 0.00001f);
    CHECK_NEAR(c.y, 0.0f, 0.00001f);
    CHECK_NEAR(c.z, 1.0f, 0.00001f);
    struct vec3 c2 = vec3_cross(j, i);
    CHECK_NEAR(c2.z, -1.0f, 0.00001f);
    struct vec3 v = vec3_make(2.0f, 3.0f, 6.0f);
    CHECK_NEAR(vec3_len(v), 7.0f, 0.0001f);
    CHECK_NEAR(vec3_dot(v, i), 2.0f, 0.00001f);
    struct vec3 vn = vec3_normalize(v);
    CHECK_NEAR(vec3_len(vn), 1.0f, 0.00001f);
    struct vec3 sum = vec3_add(v, vec3_scale(i, 3.0f));
    CHECK_NEAR(sum.x, 5.0f, 0.00001f);
    struct vec3 dif = vec3_sub(v, v);
    CHECK_NEAR(dif.z, 0.0f, 0.0f);

    TEST_SECTION("scalar helpers");
    CHECK_NEAR(clampf(5.0f, 0.0f, 3.0f), 3.0f, 0.0f);
    CHECK_NEAR(clampf(-5.0f, 0.0f, 3.0f), 0.0f, 0.0f);
    CHECK_NEAR(clampf(1.5f, 0.0f, 3.0f), 1.5f, 0.0f);
    CHECK_EQ(clampi(9, 0, 5), 5);
    CHECK_EQ(clampi(-9, 0, 5), 0);
    CHECK_EQ(clampi(3, 0, 5), 3);
    CHECK_NEAR(lerpf(10.0f, 20.0f, 0.25f), 12.5f, 0.00001f);
    CHECK_NEAR(signf(-4.0f), -1.0f, 0.0f);
    CHECK_NEAR(signf(4.0f), 1.0f, 0.0f);
    CHECK_NEAR(signf(0.0f), 0.0f, 0.0f);
    CHECK_NEAR(deg2rad(180.0f), M_PI, 0.00001f);
    CHECK_NEAR(rad2deg(M_PI_2), 90.0f, 0.0001f);
    CHECK_NEAR(smoothstep(0.0f, 1.0f, 0.5f), 0.5f, 0.00001f);
    CHECK_NEAR(smoothstep(0.0f, 1.0f, -3.0f), 0.0f, 0.0f);
    CHECK_NEAR(smoothstep(0.0f, 1.0f, 3.0f), 1.0f, 0.0f);
    CHECK_NEAR(smoothstep(0.0f, 1.0f, 0.25f), 0.15625f, 0.00001f);
    CHECK_NEAR(smoothstep(2.0f, 2.0f, 1.0f), 0.0f, 0.0f);      // an empty edge is a step, not a division by zero
    CHECK_NEAR(smoothstep(2.0f, 2.0f, 3.0f), 1.0f, 0.0f);
    CHECK_NEAR(wrapf(370.0f, 0.0f, 360.0f), 10.0f, 0.001f);
    CHECK_NEAR(wrapf(-10.0f, 0.0f, 360.0f), 350.0f, 0.001f);
    CHECK_NEAR(wrapf(360.0f, 0.0f, 360.0f), 0.0f, 0.0f);
    CHECK_NEAR(wrapf(5.0f, 0.0f, 360.0f), 5.0f, 0.0f);
    CHECK_NEAR(wrapf(0.5f, -1.0f, 1.0f), 0.5f, 0.00001f);
    CHECK_NEAR(wrapf(1.5f, -1.0f, 1.0f), -0.5f, 0.00001f);
    CHECK_NEAR(wrapf(3.0f, 2.0f, 2.0f), 2.0f, 0.0f);           // an empty period gives the low edge
}

int main(void)
{
    fixed_basic();
    fixed_trig();
    vectors();
    return test_summary();
}
