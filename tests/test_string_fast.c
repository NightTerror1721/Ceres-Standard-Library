// The assembly string routines against byte-at-a-time references written here (the C loops the library used
// before they became the standard functions): the same answers for every alignment and length, and fewer
// cycles on a long string (the timer's ticks are CPU cycles, the same on every run).
#include "ceres/test.h"
#include "ceres/string_fast.h"
#include "ceres/timer.h"

static unsigned int src_words[300];
static unsigned int dst_words[300];
static unsigned int ref_words[300];

// A string of `len` letters at `s`, ending in a NUL.
static void make(char* s, int len)
{
    for (int i = 0; i < len; i++)
        s[i] = (char)('a' + i % 26);
    s[len] = 0;
}

static char* ref_strcpy(char* dst, const char* src)
{
    size_t i = 0;
    while (src[i] != 0) { dst[i] = src[i]; i++; }
    dst[i] = 0;
    return dst;
}

static int ref_strcmp(const char* a, const char* b)
{
    size_t i = 0;
    while (a[i] != 0 && a[i] == b[i]) i++;
    return (int)((unsigned char)a[i]) - (int)((unsigned char)b[i]);
}

static char* ref_strchr(const char* s, int c)
{
    for (size_t i = 0; ; i++)
    {
        if (s[i] == (char)c) return (char*)(s + i);
        if (s[i] == 0) return 0;
    }
}

static void* ref_memchr(const void* p, int c, size_t n)
{
    const unsigned char* s = (const unsigned char*)p;
    for (size_t i = 0; i < n; i++)
        if (s[i] == (unsigned char)c) return (void*)(s + i);
    return 0;
}

int main(void)
{
    char* src_base = (char*)src_words;
    char* dst_base = (char*)dst_words;
    char* ref_base = (char*)ref_words;

    TEST_SECTION("strcpy_fast: every alignment and length");
    int copy_ok = 1;
    for (int so = 0; so < 4; so++)
        for (int d = 0; d < 4; d++)
            for (int len = 0; len <= 40; len++)
            {
                char* s = src_base + so;
                make(s, len);
                memset(dst_base, '#', 64);
                memset(ref_base, '#', 64);
                char* got = strcpy_fast(dst_base + d, s);
                ref_strcpy(ref_base + d, s);
                if (got != dst_base + d || memcmp(dst_base, ref_base, 64) != 0)
                    copy_ok = 0;                          // the return value, the copy and the bytes around it
            }
    CHECK(copy_ok);

    TEST_SECTION("strcmp_fast: the same difference as strcmp, at every alignment");
    int cmp_ok = 1;
    for (int ao = 0; ao < 4; ao++)
        for (int bo = 0; bo < 4; bo++)
        {
            char* a = src_base + ao;
            char* b = dst_base + bo;
            for (int len = 0; len <= 20; len++)
            {
                make(a, len);
                make(b, len);
                if (strcmp_fast(a, b) != ref_strcmp(a, b)) cmp_ok = 0;              // equal
                for (int at = 0; at < len; at++)
                {
                    make(b, len);
                    b[at] = (char)(b[at] + 3);
                    if (strcmp_fast(a, b) != ref_strcmp(a, b)) cmp_ok = 0;          // differing at every position
                    if (strcmp_fast(b, a) != ref_strcmp(b, a)) cmp_ok = 0;
                }
                make(b, len + 1);
                if (strcmp_fast(a, b) != ref_strcmp(a, b)) cmp_ok = 0;              // one is a prefix of the other
                if (strcmp_fast(b, a) != ref_strcmp(b, a)) cmp_ok = 0;
            }
        }
    CHECK(cmp_ok);
    char* a = src_base;
    char* b = dst_base;
    a[0] = (char)0xC8;                                    // a byte with the high bit set is a large unsigned value
    a[1] = 0;
    b[0] = 'A';
    b[1] = 0;
    CHECK(strcmp_fast(a, b) > 0);
    CHECK_EQ(strcmp_fast(a, b), ref_strcmp(a, b));
    CHECK_EQ(strcmp_fast("", ""), 0);
    CHECK(strcmp_fast("", "x") < 0);

    TEST_SECTION("strchr_fast and memchr_fast, at every alignment");
    int find_ok = 1;
    for (int off = 0; off < 4; off++)
    {
        char* s = src_base + off;
        make(s, 30);
        for (int c = 0; c < 256; c++)
        {
            if (strchr_fast(s, c) != ref_strchr(s, c)) find_ok = 0;
            if (memchr_fast(s, c, 30) != ref_memchr(s, c, 30)) find_ok = 0;
            if (memchr_fast(s, c, 31) != ref_memchr(s, c, 31)) find_ok = 0;     // the terminator is inside this one
            if (memchr_fast(s, c, 7) != ref_memchr(s, c, 7)) find_ok = 0;       // and this one ends inside a word
        }
    }
    CHECK(find_ok);
    make(a, 30);
    CHECK(strchr_fast(a, 0) == a + 30);                   // the terminator
    CHECK(strchr_fast(a, 'a' + 256) == a);                // only the low byte of c counts, as for strchr
    CHECK(strchr_fast(a, 'z') == a + 25);                 // the first of several
    CHECK(strchr_fast(a, '#') == 0);
    CHECK(memchr_fast(a, 'a', 0) == 0);                   // nothing to look at
    CHECK(memchr_fast(a, 'e', 4) == 0);                   // it is at 4, past the four bytes
    CHECK(memchr_fast(a, 'e', 5) == a + 4);
    a[10] = (char)0xE9;
    CHECK(memchr_fast(a, 0xE9, 30) == a + 10);            // a byte with the high bit set
    CHECK(memchr_fast(a, -23, 30) == a + 10);             // ... asked for as a negative int

    TEST_SECTION("fewer cycles on a long string");
    make(src_base, 1000);
    unsigned int t = timer_ticks();
    ref_strcpy(ref_base, src_base);
    unsigned int slow_copy = timer_elapsed(t);
    t = timer_ticks();
    strcpy_fast(dst_base, src_base);
    unsigned int fast_copy = timer_elapsed(t);
    CHECK(fast_copy < slow_copy);
    CHECK(memcmp(dst_base, ref_base, 1001) == 0);

    make(a, 1000);
    make(b, 1000);
    t = timer_ticks();
    int r1 = ref_strcmp(a, b);
    unsigned int slow_cmp = timer_elapsed(t);
    t = timer_ticks();
    int r2 = strcmp_fast(a, b);
    unsigned int fast_cmp = timer_elapsed(t);
    CHECK(fast_cmp < slow_cmp);
    CHECK_EQ(r1, r2);

    t = timer_ticks();
    char* p1 = ref_strchr(a, '#');
    unsigned int slow_chr = timer_elapsed(t);
    t = timer_ticks();
    char* p2 = strchr_fast(a, '#');
    unsigned int fast_chr = timer_elapsed(t);
    CHECK(fast_chr < slow_chr);
    CHECK(p1 == p2);

    t = timer_ticks();
    void* q1 = ref_memchr(a, '#', 1000);
    unsigned int slow_mem = timer_elapsed(t);
    t = timer_ticks();
    void* q2 = memchr_fast(a, '#', 1000);
    unsigned int fast_mem = timer_elapsed(t);
    CHECK(fast_mem < slow_mem);
    CHECK(q1 == q2);
    return test_summary();
}
