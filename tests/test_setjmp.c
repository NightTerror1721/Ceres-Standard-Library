// setjmp / longjmp: a jump out of deep recursion, the value it delivers, and what survives.
#include "ceres/test.h"
#include "setjmp.h"

static jmp_buf env;
static jmp_buf outer_env;

static void deep(int n)
{
    if (n == 0)
        longjmp(env, 7);
    deep(n - 1);
    __t_failed++;                                    // never reached: the frames above n are abandoned
}

static void jump_with(int value) { longjmp(env, value); }

// Uses every callee-saved register it can, so that after a longjmp they hold garbage unless restored.
static int clobber(int seed)
{
    int a = seed * 3, b = seed * 5, c = seed * 7, d = seed * 11;
    float f = (float)seed * 1.5f, g = (float)seed * 2.5f;
    int sum = a + b + c + d + (int)f + (int)g;
    if (sum > 0)
        jump_with(9);
    return sum;
}

static int run_once(jmp_buf buf, int code)
{
    int r = setjmp(buf);
    if (r == 0)
        longjmp(buf, code);
    return r;
}

static int retry_counter;
static void flaky(void)
{
    retry_counter++;
    if (retry_counter < 4)
        longjmp(env, retry_counter);                 // fails three times, then works
}

int main(void)
{
    TEST_SECTION("a jump out of deep recursion");
    int r = setjmp(env);
    CHECK(r == 0 || r == 7);
    if (r == 0)
        deep(5);
    CHECK_EQ(r, 7);                                  // we came back through setjmp with the value

    TEST_SECTION("the value delivered");
    volatile int stage = 0;
    r = setjmp(env);
    if (r == 0)
    {
        stage = 1;
        jump_with(42);
    }
    CHECK_EQ(r, 42);
    CHECK_EQ(stage, 1);
    r = setjmp(env);
    if (r == 0)
        jump_with(0);                                // longjmp(env, 0) must make setjmp return 1
    CHECK_EQ(r, 1);
    r = setjmp(env);
    if (r == 0)
        jump_with(-5);
    CHECK_EQ(r, -5);
    CHECK_EQ(run_once(env, 3), 3);                   // setjmp and longjmp in one helper's frame

    TEST_SECTION("registers survive");
    int a = 11, b = 22, c = 33, d = 44;              // not touched after setjmp: they must be intact
    float f = 1.5f, g = 2.5f, h = 3.5f;
    r = setjmp(env);
    if (r == 0)
        clobber(100);
    CHECK_EQ(r, 9);
    CHECK_EQ(a, 11);
    CHECK_EQ(b, 22);
    CHECK_EQ(c, 33);
    CHECK_EQ(d, 44);
    CHECK(f == 1.5f && g == 2.5f && h == 3.5f);

    TEST_SECTION("volatile locals keep their latest value");
    volatile int counter = 0;
    r = setjmp(env);
    counter = counter + 1;
    if (r == 0)
        jump_with(2);
    CHECK_EQ(counter, 2);                            // the assignment ran once before and once after
    CHECK_EQ(r, 2);

    TEST_SECTION("a retry loop");
    retry_counter = 0;
    volatile int failures = 0;
    int attempt = setjmp(env);
    if (attempt != 0)
        failures = failures + 1;
    flaky();                                         // longjmps back to the setjmp above until it succeeds
    CHECK_EQ(failures, 3);
    CHECK_EQ(retry_counter, 4);

    TEST_SECTION("two buffers");
    volatile int path = 0;
    if (setjmp(outer_env) == 0)
    {
        if (setjmp(env) == 0)
        {
            path = 1;
            longjmp(outer_env, 5);                   // skips over the inner buffer entirely
        }
        path = 2;                                    // not reached
    }
    CHECK_EQ(path, 1);
    return test_summary();
}
