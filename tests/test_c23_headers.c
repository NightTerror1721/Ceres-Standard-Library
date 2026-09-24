// The C11/C23 headers: <stdnoreturn.h>, <stdalign.h>, <stdbit.h>, <stdckdint.h> and <uchar.h>. <stdnoreturn.h> comes
// first on purpose: it makes `noreturn` a macro, and the library headers after it must not mind.
#include <stdnoreturn.h>
#include <stdalign.h>
#include <stdbit.h>
#include <stdckdint.h>
#include <uchar.h>
#include <stddef.h>
#include "ceres/test.h"
#include "stdlib.h"
#include "limits.h"
#include "stdint.h"

noreturn void never_back(int status);
void never_back(int status) { exit(status); }

_Static_assert(__alignof_is_defined && __alignas_is_defined, "stdalign.h defines its macros");
_Static_assert(__STDC_VERSION_STDBIT_H__ == 202311L && __STDC_VERSION_STDCKDINT_H__ == 202311L, "the C23 versions");

// Each prefixed literal has the type its header names.
_Static_assert(_Generic(L'a', wchar_t: 1, default: 0) && _Generic(u'a', char16_t: 1, default: 0) &&
               _Generic(U'a', char32_t: 1, default: 0) && _Generic(u8'a', char8_t: 1, default: 0), "the character types");
_Static_assert(sizeof(char16_t) == 2 && sizeof(char32_t) == 4 && WCHAR_MAX == 2147483647, "their sizes");
_Static_assert(__STDC_UTF_16__ && __STDC_UTF_32__, "uchar.h says its encodings");

// References a bit at a time, on a value of `w` bits.
static unsigned int ref_leading_zeros(unsigned long long x, unsigned int w)
{
    unsigned int n = 0;
    for (int i = (int)w - 1; i >= 0 && ((x >> i) & 1ull) == 0ull; i--) n++;
    return n;
}
static unsigned int ref_trailing_zeros(unsigned long long x, unsigned int w)
{
    unsigned int n = 0;
    for (unsigned int i = 0; i < w && ((x >> i) & 1ull) == 0ull; i++) n++;
    return n;
}
static unsigned int ref_ones(unsigned long long x, unsigned int w)
{
    unsigned int n = 0;
    for (unsigned int i = 0; i < w; i++) n += (unsigned int)((x >> i) & 1ull);
    return n;
}
static unsigned long long mask_of(unsigned int w) { return w == 64u ? ~0ull : (1ull << w) - 1ull; }

// Every function of stdbit.h at width w, through the generic macro, against the references.
static int check_width(unsigned long long x, unsigned int w)
{
    unsigned long long inv = ~x & mask_of(w);
    unsigned int lz = ref_leading_zeros(x, w);
    unsigned int lo = ref_leading_zeros(inv, w);
    unsigned int tz = ref_trailing_zeros(x, w);
    unsigned int to = ref_trailing_zeros(inv, w);
    unsigned int ones = ref_ones(x, w);
    unsigned int width = w - lz;
    unsigned long long floor = x == 0ull ? 0ull : 1ull << (width - 1u);
    unsigned long long ceil = 1ull;
    while (ceil < x && ceil != 0ull) ceil = (ceil << 1) & mask_of(w);
    unsigned int got[12];
    unsigned long long gfloor = 0, gceil = 0;
    bool single = false;
    if (w == 8)
    {
        unsigned char v = (unsigned char)x;
        got[0] = stdc_leading_zeros(v); got[1] = stdc_leading_ones(v); got[2] = stdc_trailing_zeros(v);
        got[3] = stdc_trailing_ones(v); got[4] = stdc_first_leading_zero(v); got[5] = stdc_first_leading_one(v);
        got[6] = stdc_first_trailing_zero(v); got[7] = stdc_first_trailing_one(v); got[8] = stdc_count_zeros(v);
        got[9] = stdc_count_ones(v); got[10] = stdc_bit_width(v);
        single = stdc_has_single_bit(v); gfloor = stdc_bit_floor(v); gceil = stdc_bit_ceil(v);
    }
    else if (w == 16)
    {
        unsigned short v = (unsigned short)x;
        got[0] = stdc_leading_zeros(v); got[1] = stdc_leading_ones(v); got[2] = stdc_trailing_zeros(v);
        got[3] = stdc_trailing_ones(v); got[4] = stdc_first_leading_zero(v); got[5] = stdc_first_leading_one(v);
        got[6] = stdc_first_trailing_zero(v); got[7] = stdc_first_trailing_one(v); got[8] = stdc_count_zeros(v);
        got[9] = stdc_count_ones(v); got[10] = stdc_bit_width(v);
        single = stdc_has_single_bit(v); gfloor = stdc_bit_floor(v); gceil = stdc_bit_ceil(v);
    }
    else if (w == 32)
    {
        unsigned int v = (unsigned int)x;
        got[0] = stdc_leading_zeros(v); got[1] = stdc_leading_ones(v); got[2] = stdc_trailing_zeros(v);
        got[3] = stdc_trailing_ones(v); got[4] = stdc_first_leading_zero(v); got[5] = stdc_first_leading_one(v);
        got[6] = stdc_first_trailing_zero(v); got[7] = stdc_first_trailing_one(v); got[8] = stdc_count_zeros(v);
        got[9] = stdc_count_ones(v); got[10] = stdc_bit_width(v);
        single = stdc_has_single_bit(v); gfloor = stdc_bit_floor(v); gceil = stdc_bit_ceil(v);
        unsigned long l = (unsigned long)x;                           // unsigned long is 32 bits too
        if (stdc_leading_zeros(l) != got[0] || stdc_bit_ceil(l) != (unsigned long)gceil) return 0;
    }
    else
    {
        unsigned long long v = x;
        got[0] = stdc_leading_zeros(v); got[1] = stdc_leading_ones(v); got[2] = stdc_trailing_zeros(v);
        got[3] = stdc_trailing_ones(v); got[4] = stdc_first_leading_zero(v); got[5] = stdc_first_leading_one(v);
        got[6] = stdc_first_trailing_zero(v); got[7] = stdc_first_trailing_one(v); got[8] = stdc_count_zeros(v);
        got[9] = stdc_count_ones(v); got[10] = stdc_bit_width(v);
        single = stdc_has_single_bit(v); gfloor = stdc_bit_floor(v); gceil = stdc_bit_ceil(v);
    }
    unsigned int want[11];
    want[0] = lz; want[1] = lo; want[2] = tz; want[3] = to;
    want[4] = lo == w ? 0u : lo + 1u;
    want[5] = x == 0ull ? 0u : lz + 1u;
    want[6] = to == w ? 0u : to + 1u;
    want[7] = x == 0ull ? 0u : tz + 1u;
    want[8] = w - ones; want[9] = ones; want[10] = width;
    for (int i = 0; i < 11; i++)
        if (got[i] != want[i]) return 0;
    if (single != (ones == 1u) || gfloor != floor)
        return 0;
    if (x <= 1ull ? gceil != 1ull : gceil != ceil)                    // 0 when the ceiling does not fit
        return 0;
    return 1;
}

static void stdbit(void)
{
    TEST_SECTION("stdbit.h: every function at every width");
    static const unsigned long long samples[] = {
        0ull, 1ull, 2ull, 3ull, 0x40ull, 0x7Full, 0x80ull, 0x81ull, 0xFFull, 0x100ull, 0x7FFFull, 0x8000ull,
        0xFFFFull, 0x12345ull, 0x7FFFFFFFull, 0x80000000ull, 0x80000001ull, 0xFFFFFFFFull, 0x100000000ull,
        0x123456789ABCull, 0x7FFFFFFFFFFFFFFFull, 0x8000000000000000ull, 0x8000000000000001ull,
        0xFFFFFFFFFFFFFFFFull, 0xF0F0F0F0F0F0F0F0ull,
    };
    int ok = 1;
    for (unsigned int i = 0; i < sizeof(samples) / sizeof(samples[0]); i++)
    {
        unsigned long long x = samples[i];
        if (!check_width(x & 0xFFull, 8) || !check_width(x & 0xFFFFull, 16) ||
            !check_width(x & 0xFFFFFFFFull, 32) || !check_width(x, 64))
            ok = 0;
    }
    CHECK(ok);
    CHECK_EQ((int)stdc_leading_zeros((unsigned char)1), 7);           // the generic macro picks the width
    CHECK_EQ((int)stdc_leading_zeros((unsigned short)1), 15);
    CHECK_EQ((int)stdc_leading_zeros(1u), 31);
    CHECK_EQ((int)stdc_leading_zeros(1ull), 63);
    CHECK(stdc_bit_ceil(200u) == 256u && stdc_bit_ceil((unsigned char)200) == 0);
    CHECK(stdc_bit_floor(0x123456789ull) == 0x100000000ull);
    unsigned int word = 0x01020304u;
    CHECK(__STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_LITTLE__ && *(unsigned char*)&word == 4);
}

static void stdckdint(void)
{
    TEST_SECTION("stdckdint.h: exact results, wrapped stores");
    int i = 0;
    unsigned int u = 0;
    long long ll = 0;
    unsigned long long ull = 0;
    signed char sc = 0;
    unsigned short us = 0;

    CHECK(ckd_add(&i, INT_MAX, 1) && i == INT_MIN);                     // over, and the wrapped value
    CHECK(!ckd_add(&i, INT_MAX, 0) && i == INT_MAX);
    CHECK(!ckd_sub(&i, INT_MIN + 1, 1) && i == INT_MIN);
    CHECK(ckd_sub(&i, INT_MIN, 1) && i == INT_MAX);
    CHECK(ckd_sub(&u, 0, 1) && u == UINT_MAX);                         // below zero for an unsigned result
    CHECK(!ckd_add(&u, -1, 5) && u == 4u);                              // mixed signs: exact, not converted first
    CHECK(!ckd_sub(&i, 0u, 5u) && i == -5);                             // unsigned operands, a signed result
    CHECK(ckd_add(&u, UINT_MAX, 1u) && u == 0u);
    CHECK(ckd_mul(&i, 65536, 65536) && i == 0);
    CHECK(!ckd_mul(&ll, 65536, 65536) && ll == 4294967296LL);           // the result's type decides
    CHECK(!ckd_mul(&ll, 3000000000u, 4u) && ll == 12000000000LL);
    CHECK(!ckd_mul(&i, -5, 0) && i == 0);
    CHECK(!ckd_mul(&i, -46341, 46340) && i == -2147441940);
    CHECK(ckd_mul(&i, -46341, 46341) && i == 2147479015);               // |product| > 2^31: over, and wrapped
    CHECK(!ckd_mul(&i, -65536, 32768) && i == INT_MIN);                 // exactly INT_MIN fits
    CHECK(ckd_mul(&i, 65536, 32768) && i == INT_MIN);                   // but +2^31 does not
    CHECK(!ckd_add(&sc, 100, 27) && sc == 127);
    CHECK(ckd_add(&sc, 100, 28) && sc == -128);
    CHECK(!ckd_mul(&us, 255, 257) && us == 65535);
    CHECK(ckd_mul(&us, 256, 256) && us == 0);
    CHECK(ckd_sub(&ll, LLONG_MIN, 1) && ll == LLONG_MAX);
    CHECK(!ckd_sub(&ll, -1, LLONG_MAX) && ll == LLONG_MIN);
    CHECK(ckd_mul(&ll, -3037000500LL, 3037000500LL));                   // just past 2^63
    CHECK(!ckd_mul(&ll, -3037000499LL, 3037000499LL) && ll == -9223372030926249001LL);
    CHECK(ckd_mul(&ull, ULLONG_MAX, 2) && ull == ULLONG_MAX - 1ull);    // 2^65 - 2, wrapped
    CHECK(!ckd_mul(&ull, 0xFFFFFFFFull, 0x100000001ull) && ull == ULLONG_MAX);   // exactly 2^64 - 1
    CHECK(ckd_mul(&ull, 0x100000000ull, 0x100000000ull) && ull == 0ull);         // exactly 2^64
    CHECK(ckd_add(&ull, ULLONG_MAX, 1) && ull == 0ull);
    CHECK(ckd_add(&ll, ULLONG_MAX, -1) && ll == -2);                    // 2^64 - 2: exact, too big, wrapped
    CHECK(!ckd_sub(&ll, ULLONG_MAX, ULLONG_MAX - 5ull) && ll == 5);
    CHECK(ckd_add(&ll, (unsigned long long)LLONG_MAX + 1ull, 0) && ll == LLONG_MIN);
}

static void stdalign_and_noreturn(void)
{
    TEST_SECTION("stdalign.h and stdnoreturn.h");
    alignas(4) int aligned_int = 3;
    CHECK_EQ((int)alignof(int), 4);
    CHECK_EQ((int)_Alignof(long long), (int)alignof(long long));
    CHECK_EQ(((unsigned int)&aligned_int & 3u), 0u);
    CHECK(aligned_int == 3);
    void (*stop)(int) = never_back;                                   // declared noreturn, not called here
    CHECK(stop != 0);
}

int main(void)
{
    stdbit();
    stdckdint();
    stdalign_and_noreturn();
    return test_summary();
}
