#pragma once

#include "stdbool.h"

// C23 <stdbit.h>: counting and finding bits, on the machine's own clz, ctz and popcnt instructions
// (through the compiler's builtins, so a call is a few instructions and no library code).
//
// Every operation comes once per unsigned type - _uc, _us, _ui, _ul, _ull - and once as a type-generic
// macro that picks the one for its argument: stdc_leading_zeros((unsigned char)1) is 7, and
// stdc_leading_zeros(1u) is 31. Only the unsigned types are accepted, as C23 says. The counts are
// unsigned int; has_single_bit is bool; bit_floor and bit_ceil give back the argument's own type, and
// bit_ceil of a value whose ceiling does not fit that type is 0.
//
// "first" positions count from 1 (1 is the most significant bit for leading, the least for trailing),
// and are 0 when there is no such bit.
//
// (This file is generated; the helpers take a width so that one body serves 8, 16 and 32 bits, and
// the 64-bit ones work on the two halves.)

#define __STDC_VERSION_STDBIT_H__ 202311      // C writes 202311L; ceresc has no l suffix, and long is int here
#define __STDC_ENDIAN_LITTLE__ 1234
#define __STDC_ENDIAN_BIG__    4321
#define __STDC_ENDIAN_NATIVE__ __STDC_ENDIAN_LITTLE__     // the machine is little-endian

// ---- on a value of w bits (8, 16 or 32), zero-extended into an unsigned int ----
static inline unsigned int __stdc_mask(unsigned int w) { return w >= 32u ? 0xFFFFFFFFu : (1u << w) - 1u; }
static inline unsigned int __stdc_lz(unsigned int x, unsigned int w) { return __builtin_clz(x) - (32u - w); }   // clz(0) is 32
static inline unsigned int __stdc_lo(unsigned int x, unsigned int w) { return __stdc_lz(~x & __stdc_mask(w), w); }
static inline unsigned int __stdc_tz(unsigned int x, unsigned int w) { return __builtin_umin(__builtin_ctz(x), w); }
static inline unsigned int __stdc_to(unsigned int x, unsigned int w) { return __stdc_tz(~x & __stdc_mask(w), w); }
static inline unsigned int __stdc_flz(unsigned int x, unsigned int w) { unsigned int n = __stdc_lo(x, w); return n == w ? 0u : n + 1u; }
static inline unsigned int __stdc_flo(unsigned int x, unsigned int w) { return x == 0u ? 0u : __stdc_lz(x, w) + 1u; }
static inline unsigned int __stdc_ftz(unsigned int x, unsigned int w) { unsigned int n = __stdc_to(x, w); return n == w ? 0u : n + 1u; }
static inline unsigned int __stdc_fto(unsigned int x, unsigned int w) { return x == 0u ? 0u : __stdc_tz(x, w) + 1u; }
static inline unsigned int __stdc_cz(unsigned int x, unsigned int w) { return w - __builtin_popcount(x); }
static inline unsigned int __stdc_co(unsigned int x, unsigned int w) { (void)w; return __builtin_popcount(x); }
static inline bool __stdc_single(unsigned int x, unsigned int w) { (void)w; return __builtin_popcount(x) == 1u; }
static inline unsigned int __stdc_width(unsigned int x, unsigned int w) { return w - __stdc_lz(x, w); }
static inline unsigned int __stdc_floor(unsigned int x, unsigned int w) { return x == 0u ? 0u : 1u << (__stdc_width(x, w) - 1u); }
static inline unsigned int __stdc_ceil(unsigned int x, unsigned int w)
{
    if (x <= 1u)
        return 1u;
    unsigned int bits = __stdc_width(x - 1u, w);
    return bits >= w ? 0u : 1u << bits;
}

// ---- on 64 bits, from the two halves ----
static inline unsigned int __stdc_lz64(unsigned long long x)
{
    unsigned int hi = (unsigned int)(x >> 32);
    return hi != 0u ? __builtin_clz(hi) : 32u + __builtin_clz((unsigned int)x);
}
static inline unsigned int __stdc_tz64(unsigned long long x)
{
    unsigned int lo = (unsigned int)x;
    return lo != 0u ? __builtin_ctz(lo) : 32u + __builtin_ctz((unsigned int)(x >> 32));
}
static inline unsigned int __stdc_co64(unsigned long long x) { return __builtin_popcount((unsigned int)x) + __builtin_popcount((unsigned int)(x >> 32)); }
static inline unsigned int __stdc_lo64(unsigned long long x) { return __stdc_lz64(~x); }
static inline unsigned int __stdc_to64(unsigned long long x) { return __stdc_tz64(~x); }
static inline unsigned int __stdc_flz64(unsigned long long x) { unsigned int n = __stdc_lo64(x); return n == 64u ? 0u : n + 1u; }
static inline unsigned int __stdc_flo64(unsigned long long x) { return x == 0ull ? 0u : __stdc_lz64(x) + 1u; }
static inline unsigned int __stdc_ftz64(unsigned long long x) { unsigned int n = __stdc_to64(x); return n == 64u ? 0u : n + 1u; }
static inline unsigned int __stdc_fto64(unsigned long long x) { return x == 0ull ? 0u : __stdc_tz64(x) + 1u; }
static inline unsigned int __stdc_cz64(unsigned long long x) { return 64u - __stdc_co64(x); }
static inline bool __stdc_single64(unsigned long long x) { return __stdc_co64(x) == 1u; }
static inline unsigned int __stdc_width64(unsigned long long x) { return 64u - __stdc_lz64(x); }
static inline unsigned long long __stdc_floor64(unsigned long long x) { return x == 0ull ? 0ull : 1ull << (__stdc_width64(x) - 1u); }
static inline unsigned long long __stdc_ceil64(unsigned long long x)
{
    if (x <= 1ull)
        return 1ull;
    unsigned int bits = __stdc_width64(x - 1ull);
    return bits >= 64u ? 0ull : 1ull << bits;
}

// ---- stdc_leading_zeros ----
static inline unsigned int stdc_leading_zeros_uc(unsigned char x) { return __stdc_lz((unsigned int)x, 8u); }
static inline unsigned int stdc_leading_zeros_us(unsigned short x) { return __stdc_lz((unsigned int)x, 16u); }
static inline unsigned int stdc_leading_zeros_ui(unsigned int x) { return __stdc_lz((unsigned int)x, 32u); }
static inline unsigned int stdc_leading_zeros_ul(unsigned long x) { return __stdc_lz((unsigned int)x, 32u); }
static inline unsigned int stdc_leading_zeros_ull(unsigned long long x) { return __stdc_lz64(x); }
#define stdc_leading_zeros(x) _Generic((x), unsigned char: stdc_leading_zeros_uc, unsigned short: stdc_leading_zeros_us, unsigned int: stdc_leading_zeros_ui, unsigned long: stdc_leading_zeros_ul, unsigned long long: stdc_leading_zeros_ull)(x)

// ---- stdc_leading_ones ----
static inline unsigned int stdc_leading_ones_uc(unsigned char x) { return __stdc_lo((unsigned int)x, 8u); }
static inline unsigned int stdc_leading_ones_us(unsigned short x) { return __stdc_lo((unsigned int)x, 16u); }
static inline unsigned int stdc_leading_ones_ui(unsigned int x) { return __stdc_lo((unsigned int)x, 32u); }
static inline unsigned int stdc_leading_ones_ul(unsigned long x) { return __stdc_lo((unsigned int)x, 32u); }
static inline unsigned int stdc_leading_ones_ull(unsigned long long x) { return __stdc_lo64(x); }
#define stdc_leading_ones(x) _Generic((x), unsigned char: stdc_leading_ones_uc, unsigned short: stdc_leading_ones_us, unsigned int: stdc_leading_ones_ui, unsigned long: stdc_leading_ones_ul, unsigned long long: stdc_leading_ones_ull)(x)

// ---- stdc_trailing_zeros ----
static inline unsigned int stdc_trailing_zeros_uc(unsigned char x) { return __stdc_tz((unsigned int)x, 8u); }
static inline unsigned int stdc_trailing_zeros_us(unsigned short x) { return __stdc_tz((unsigned int)x, 16u); }
static inline unsigned int stdc_trailing_zeros_ui(unsigned int x) { return __stdc_tz((unsigned int)x, 32u); }
static inline unsigned int stdc_trailing_zeros_ul(unsigned long x) { return __stdc_tz((unsigned int)x, 32u); }
static inline unsigned int stdc_trailing_zeros_ull(unsigned long long x) { return __stdc_tz64(x); }
#define stdc_trailing_zeros(x) _Generic((x), unsigned char: stdc_trailing_zeros_uc, unsigned short: stdc_trailing_zeros_us, unsigned int: stdc_trailing_zeros_ui, unsigned long: stdc_trailing_zeros_ul, unsigned long long: stdc_trailing_zeros_ull)(x)

// ---- stdc_trailing_ones ----
static inline unsigned int stdc_trailing_ones_uc(unsigned char x) { return __stdc_to((unsigned int)x, 8u); }
static inline unsigned int stdc_trailing_ones_us(unsigned short x) { return __stdc_to((unsigned int)x, 16u); }
static inline unsigned int stdc_trailing_ones_ui(unsigned int x) { return __stdc_to((unsigned int)x, 32u); }
static inline unsigned int stdc_trailing_ones_ul(unsigned long x) { return __stdc_to((unsigned int)x, 32u); }
static inline unsigned int stdc_trailing_ones_ull(unsigned long long x) { return __stdc_to64(x); }
#define stdc_trailing_ones(x) _Generic((x), unsigned char: stdc_trailing_ones_uc, unsigned short: stdc_trailing_ones_us, unsigned int: stdc_trailing_ones_ui, unsigned long: stdc_trailing_ones_ul, unsigned long long: stdc_trailing_ones_ull)(x)

// ---- stdc_first_leading_zero ----
static inline unsigned int stdc_first_leading_zero_uc(unsigned char x) { return __stdc_flz((unsigned int)x, 8u); }
static inline unsigned int stdc_first_leading_zero_us(unsigned short x) { return __stdc_flz((unsigned int)x, 16u); }
static inline unsigned int stdc_first_leading_zero_ui(unsigned int x) { return __stdc_flz((unsigned int)x, 32u); }
static inline unsigned int stdc_first_leading_zero_ul(unsigned long x) { return __stdc_flz((unsigned int)x, 32u); }
static inline unsigned int stdc_first_leading_zero_ull(unsigned long long x) { return __stdc_flz64(x); }
#define stdc_first_leading_zero(x) _Generic((x), unsigned char: stdc_first_leading_zero_uc, unsigned short: stdc_first_leading_zero_us, unsigned int: stdc_first_leading_zero_ui, unsigned long: stdc_first_leading_zero_ul, unsigned long long: stdc_first_leading_zero_ull)(x)

// ---- stdc_first_leading_one ----
static inline unsigned int stdc_first_leading_one_uc(unsigned char x) { return __stdc_flo((unsigned int)x, 8u); }
static inline unsigned int stdc_first_leading_one_us(unsigned short x) { return __stdc_flo((unsigned int)x, 16u); }
static inline unsigned int stdc_first_leading_one_ui(unsigned int x) { return __stdc_flo((unsigned int)x, 32u); }
static inline unsigned int stdc_first_leading_one_ul(unsigned long x) { return __stdc_flo((unsigned int)x, 32u); }
static inline unsigned int stdc_first_leading_one_ull(unsigned long long x) { return __stdc_flo64(x); }
#define stdc_first_leading_one(x) _Generic((x), unsigned char: stdc_first_leading_one_uc, unsigned short: stdc_first_leading_one_us, unsigned int: stdc_first_leading_one_ui, unsigned long: stdc_first_leading_one_ul, unsigned long long: stdc_first_leading_one_ull)(x)

// ---- stdc_first_trailing_zero ----
static inline unsigned int stdc_first_trailing_zero_uc(unsigned char x) { return __stdc_ftz((unsigned int)x, 8u); }
static inline unsigned int stdc_first_trailing_zero_us(unsigned short x) { return __stdc_ftz((unsigned int)x, 16u); }
static inline unsigned int stdc_first_trailing_zero_ui(unsigned int x) { return __stdc_ftz((unsigned int)x, 32u); }
static inline unsigned int stdc_first_trailing_zero_ul(unsigned long x) { return __stdc_ftz((unsigned int)x, 32u); }
static inline unsigned int stdc_first_trailing_zero_ull(unsigned long long x) { return __stdc_ftz64(x); }
#define stdc_first_trailing_zero(x) _Generic((x), unsigned char: stdc_first_trailing_zero_uc, unsigned short: stdc_first_trailing_zero_us, unsigned int: stdc_first_trailing_zero_ui, unsigned long: stdc_first_trailing_zero_ul, unsigned long long: stdc_first_trailing_zero_ull)(x)

// ---- stdc_first_trailing_one ----
static inline unsigned int stdc_first_trailing_one_uc(unsigned char x) { return __stdc_fto((unsigned int)x, 8u); }
static inline unsigned int stdc_first_trailing_one_us(unsigned short x) { return __stdc_fto((unsigned int)x, 16u); }
static inline unsigned int stdc_first_trailing_one_ui(unsigned int x) { return __stdc_fto((unsigned int)x, 32u); }
static inline unsigned int stdc_first_trailing_one_ul(unsigned long x) { return __stdc_fto((unsigned int)x, 32u); }
static inline unsigned int stdc_first_trailing_one_ull(unsigned long long x) { return __stdc_fto64(x); }
#define stdc_first_trailing_one(x) _Generic((x), unsigned char: stdc_first_trailing_one_uc, unsigned short: stdc_first_trailing_one_us, unsigned int: stdc_first_trailing_one_ui, unsigned long: stdc_first_trailing_one_ul, unsigned long long: stdc_first_trailing_one_ull)(x)

// ---- stdc_count_zeros ----
static inline unsigned int stdc_count_zeros_uc(unsigned char x) { return __stdc_cz((unsigned int)x, 8u); }
static inline unsigned int stdc_count_zeros_us(unsigned short x) { return __stdc_cz((unsigned int)x, 16u); }
static inline unsigned int stdc_count_zeros_ui(unsigned int x) { return __stdc_cz((unsigned int)x, 32u); }
static inline unsigned int stdc_count_zeros_ul(unsigned long x) { return __stdc_cz((unsigned int)x, 32u); }
static inline unsigned int stdc_count_zeros_ull(unsigned long long x) { return __stdc_cz64(x); }
#define stdc_count_zeros(x) _Generic((x), unsigned char: stdc_count_zeros_uc, unsigned short: stdc_count_zeros_us, unsigned int: stdc_count_zeros_ui, unsigned long: stdc_count_zeros_ul, unsigned long long: stdc_count_zeros_ull)(x)

// ---- stdc_count_ones ----
static inline unsigned int stdc_count_ones_uc(unsigned char x) { return __stdc_co((unsigned int)x, 8u); }
static inline unsigned int stdc_count_ones_us(unsigned short x) { return __stdc_co((unsigned int)x, 16u); }
static inline unsigned int stdc_count_ones_ui(unsigned int x) { return __stdc_co((unsigned int)x, 32u); }
static inline unsigned int stdc_count_ones_ul(unsigned long x) { return __stdc_co((unsigned int)x, 32u); }
static inline unsigned int stdc_count_ones_ull(unsigned long long x) { return __stdc_co64(x); }
#define stdc_count_ones(x) _Generic((x), unsigned char: stdc_count_ones_uc, unsigned short: stdc_count_ones_us, unsigned int: stdc_count_ones_ui, unsigned long: stdc_count_ones_ul, unsigned long long: stdc_count_ones_ull)(x)

// ---- stdc_has_single_bit ----
static inline bool stdc_has_single_bit_uc(unsigned char x) { return __stdc_single((unsigned int)x, 8u); }
static inline bool stdc_has_single_bit_us(unsigned short x) { return __stdc_single((unsigned int)x, 16u); }
static inline bool stdc_has_single_bit_ui(unsigned int x) { return __stdc_single((unsigned int)x, 32u); }
static inline bool stdc_has_single_bit_ul(unsigned long x) { return __stdc_single((unsigned int)x, 32u); }
static inline bool stdc_has_single_bit_ull(unsigned long long x) { return __stdc_single64(x); }
#define stdc_has_single_bit(x) _Generic((x), unsigned char: stdc_has_single_bit_uc, unsigned short: stdc_has_single_bit_us, unsigned int: stdc_has_single_bit_ui, unsigned long: stdc_has_single_bit_ul, unsigned long long: stdc_has_single_bit_ull)(x)

// ---- stdc_bit_width ----
static inline unsigned int stdc_bit_width_uc(unsigned char x) { return __stdc_width((unsigned int)x, 8u); }
static inline unsigned int stdc_bit_width_us(unsigned short x) { return __stdc_width((unsigned int)x, 16u); }
static inline unsigned int stdc_bit_width_ui(unsigned int x) { return __stdc_width((unsigned int)x, 32u); }
static inline unsigned int stdc_bit_width_ul(unsigned long x) { return __stdc_width((unsigned int)x, 32u); }
static inline unsigned int stdc_bit_width_ull(unsigned long long x) { return __stdc_width64(x); }
#define stdc_bit_width(x) _Generic((x), unsigned char: stdc_bit_width_uc, unsigned short: stdc_bit_width_us, unsigned int: stdc_bit_width_ui, unsigned long: stdc_bit_width_ul, unsigned long long: stdc_bit_width_ull)(x)

// ---- stdc_bit_floor ----
static inline unsigned char stdc_bit_floor_uc(unsigned char x) { return (unsigned char)__stdc_floor((unsigned int)x, 8u); }
static inline unsigned short stdc_bit_floor_us(unsigned short x) { return (unsigned short)__stdc_floor((unsigned int)x, 16u); }
static inline unsigned int stdc_bit_floor_ui(unsigned int x) { return (unsigned int)__stdc_floor((unsigned int)x, 32u); }
static inline unsigned long stdc_bit_floor_ul(unsigned long x) { return (unsigned long)__stdc_floor((unsigned int)x, 32u); }
static inline unsigned long long stdc_bit_floor_ull(unsigned long long x) { return __stdc_floor64(x); }
#define stdc_bit_floor(x) _Generic((x), unsigned char: stdc_bit_floor_uc, unsigned short: stdc_bit_floor_us, unsigned int: stdc_bit_floor_ui, unsigned long: stdc_bit_floor_ul, unsigned long long: stdc_bit_floor_ull)(x)

// ---- stdc_bit_ceil ----
static inline unsigned char stdc_bit_ceil_uc(unsigned char x) { return (unsigned char)__stdc_ceil((unsigned int)x, 8u); }
static inline unsigned short stdc_bit_ceil_us(unsigned short x) { return (unsigned short)__stdc_ceil((unsigned int)x, 16u); }
static inline unsigned int stdc_bit_ceil_ui(unsigned int x) { return (unsigned int)__stdc_ceil((unsigned int)x, 32u); }
static inline unsigned long stdc_bit_ceil_ul(unsigned long x) { return (unsigned long)__stdc_ceil((unsigned int)x, 32u); }
static inline unsigned long long stdc_bit_ceil_ull(unsigned long long x) { return __stdc_ceil64(x); }
#define stdc_bit_ceil(x) _Generic((x), unsigned char: stdc_bit_ceil_uc, unsigned short: stdc_bit_ceil_us, unsigned int: stdc_bit_ceil_ui, unsigned long: stdc_bit_ceil_ul, unsigned long long: stdc_bit_ceil_ull)(x)
