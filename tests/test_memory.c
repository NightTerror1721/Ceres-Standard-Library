// memcpy, memmove, memset, memcmp and strlen (asm/memory.casm) against byte-at-a-time references written
// here: the same bytes, the same return values, and nothing touched around the destination, for every
// alignment of both pointers, every length up to past a few unrolled turns, and overlaps both ways. Then
// the instruction counts on 4 KiB (the timer's ticks, the same on every run) stay under a bound per byte.
#include "ceres/test.h"
#include "ceres/timer.h"
#include "string.h"
#include "ceres/string_fast.h"

#define AREA 256
#define LEN_MAX 70

static unsigned int area_words[AREA / 4];
static unsigned int ref_words[AREA / 4];
static unsigned int big_a[1100];
static unsigned int big_b[1100];

static void fill(unsigned char* p, int n, int seed)
{
    for (int i = 0; i < n; i++)
        p[i] = (unsigned char)(seed + i * 7 + (i >> 3));
}

static void ref_move(unsigned char* d, const unsigned char* s, int n)
{
    if (d < s)
        for (int i = 0; i < n; i++) d[i] = s[i];
    else
        for (int i = n - 1; i >= 0; i--) d[i] = s[i];
}

static int ref_cmp(const unsigned char* a, const unsigned char* b, int n)
{
    for (int i = 0; i < n; i++)
        if (a[i] != b[i]) return (int)a[i] - (int)b[i];
    return 0;
}

static int same_area(void)
{
    const unsigned char* a = (const unsigned char*)area_words;
    const unsigned char* b = (const unsigned char*)ref_words;
    for (int i = 0; i < AREA; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}

static void check_memcpy(void)
{
    TEST_SECTION("memcpy: every alignment and length");
    unsigned char* area = (unsigned char*)area_words;
    unsigned char* ref = (unsigned char*)ref_words;
    int ok = 1;
    for (int so = 0; so < 4; so++)
        for (int d = 0; d < 4; d++)
            for (int n = 0; n <= LEN_MAX; n++)
            {
                fill(area, AREA, n);
                fill(ref, AREA, n);
                void* got = memcpy(area + 8 + d, area + 128 + so, (size_t)n);
                ref_move(ref + 8 + d, ref + 128 + so, n);
                if (got != area + 8 + d || !same_area())
                    ok = 0;
            }
    CHECK(ok);
}

static void check_memmove(void)
{
    TEST_SECTION("memmove: overlapping both ways, at every alignment");
    unsigned char* area = (unsigned char*)area_words;
    unsigned char* ref = (unsigned char*)ref_words;
    int ok = 1;
    for (int base = 0; base < 4; base++)
        for (int dist = -20; dist <= 20; dist++)
            for (int n = 0; n <= LEN_MAX; n += 3)
            {
                int s = 80 + base;
                fill(area, AREA, dist + n);
                fill(ref, AREA, dist + n);
                void* got = memmove(area + s + dist, area + s, (size_t)n);
                ref_move(ref + s + dist, ref + s, n);
                if (got != area + s + dist || !same_area())
                    ok = 0;
            }
    CHECK(ok);
}

static void check_memset(void)
{
    TEST_SECTION("memset: every alignment and length");
    unsigned char* area = (unsigned char*)area_words;
    unsigned char* ref = (unsigned char*)ref_words;
    int ok = 1;
    for (int d = 0; d < 4; d++)
        for (int n = 0; n <= LEN_MAX; n++)
        {
            fill(area, AREA, n);
            fill(ref, AREA, n);
            void* got = memset(area + 16 + d, 0x1A5 + n, (size_t)n);   // only the low byte of c counts
            for (int i = 0; i < n; i++)
                ref[16 + d + i] = (unsigned char)(0xA5 + n);
            if (got != area + 16 + d || !same_area())
                ok = 0;
        }
    CHECK(ok);
}

static void check_memcmp(void)
{
    TEST_SECTION("memcmp: the same difference, at every alignment and position");
    unsigned char* a = (unsigned char*)area_words;
    unsigned char* b = (unsigned char*)ref_words;
    int ok = 1;
    for (int ao = 0; ao < 4; ao++)
        for (int bo = 0; bo < 4; bo++)
            for (int n = 0; n <= 40; n++)
            {
                fill(a + ao, n + 1, 3);
                fill(b + bo, n + 1, 3);
                if (memcmp(a + ao, b + bo, (size_t)n) != 0) ok = 0;                 // equal
                for (int at = 0; at < n; at++)
                {
                    unsigned char keep = b[bo + at];
                    b[bo + at] = (unsigned char)(keep + 0x90);                   // high bit: unsigned bytes
                    int want = ref_cmp(a + ao, b + bo, n);
                    if (memcmp(a + ao, b + bo, (size_t)n) != want) ok = 0;
                    if (memcmp(b + bo, a + ao, (size_t)n) != -want) ok = 0;
                    b[bo + at] = keep;
                }
            }
    CHECK(ok);
}

static void check_strlen(void)
{
    TEST_SECTION("strlen: every alignment and length, high bytes included");
    char* s = (char*)area_words;
    int ok = 1;
    for (int off = 0; off < 4; off++)
        for (int n = 0; n <= LEN_MAX; n++)
        {
            for (int i = 0; i < n; i++)
                s[off + i] = (char)(0x80 + (i % 120) + 1);     // never zero; 0x81.. has the high bit set
            s[off + n] = 0;
            s[off + n + 1] = 'x';                              // something after the terminator, in its word
            if (strlen(s + off) != (size_t)n) ok = 0;
        }
    CHECK(ok);
}

static void check_string_standard_names(void)
{
    TEST_SECTION("strcpy, strcmp, strchr and memchr under their standard names");
    char buf[32];
    CHECK(strcpy(buf + 1, "hello, world") == buf + 1);
    CHECK_STR(buf + 1, "hello, world");
    CHECK(strcmp("abc", "abd") < 0 && strcmp("abd", "abc") > 0 && strcmp("abc", "abc") == 0);
    CHECK(strchr(buf + 1, 'w') == buf + 8);
    CHECK(strchr(buf + 1, 0) == buf + 13);
    CHECK(memchr(buf + 1, 'o', 12) == buf + 5);
    CHECK(memchr(buf + 1, 'z', 12) == 0);
    // The _fast names are the same routines (ceres/string_fast.h says so).
    CHECK((void*)strcpy == (void*)strcpy_fast && (void*)strcmp == (void*)strcmp_fast);
    CHECK((void*)strchr == (void*)strchr_fast && (void*)memchr == (void*)memchr_fast);
}

// Instructions for one call on 4 KiB, from the timer. The bounds are per byte, with room for the
// call and the head and tail bytes.
static unsigned int count(int which, int a_off, int b_off)
{
    unsigned char* a = (unsigned char*)big_a;
    unsigned char* b = (unsigned char*)big_b;
    unsigned int t = timer_ticks();
    if (which == 0) memcpy(a + a_off, b + b_off, 4096);
    if (which == 1) memmove(a + 64 + a_off, a + b_off, 4096);
    if (which == 2) memset(a + a_off, 7, 4096);
    if (which == 3) memcmp(a + a_off, b + b_off, 4096);
    if (which == 4) strlen((const char*)a + a_off);
    return timer_elapsed(t);
}

static void check_speed(void)
{
    TEST_SECTION("instructions per byte on 4 KiB");
    unsigned char* a = (unsigned char*)big_a;
    unsigned char* b = (unsigned char*)big_b;
    CHECK(count(0, 0, 0) < 4096);             // memcpy aligned: under one per byte
    CHECK(count(0, 1, 1) < 4096);             // co-aligned at +1: the same
    CHECK(count(0, 1, 2) < 4 * 4096);         // never aligned together: bytes, four per turn
    CHECK(count(1, 0, 0) < 4096);             // memmove backwards over an overlap
    CHECK(count(2, 3, 0) < 4096);             // memset
    memset(a, 'q', 4200);
    memset(b, 'q', 4200);
    CHECK(count(3, 0, 0) < 2 * 4096);         // memcmp of equal buffers
    CHECK(count(3, 2, 2) < 2 * 4096);
    a[4095] = 0;
    CHECK(count(4, 0, 0) < 2 * 4096 + 256);   // strlen (the bound leaves room for -O0 around the call)
}

int main(void)
{
    check_memcpy();
    check_memmove();
    check_memset();
    check_memcmp();
    check_strlen();
    check_string_standard_names();
    check_speed();
    return test_summary();
}
