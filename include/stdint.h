#pragma once

// Fixed-width integers for a 32-bit machine. `long` is 32 bits, but `long long` is a real 8-byte type
// (the compiler lowers it as a pair of words), so int64_t/uint64_t are defined here for real.

typedef signed char             int8_t;
typedef unsigned char           uint8_t;
typedef signed short int        int16_t;
typedef unsigned short int      uint16_t;
typedef signed int              int32_t;
typedef unsigned int            uint32_t;
typedef signed long long int    int64_t;
typedef unsigned long long int  uint64_t;

typedef int8_t    int_least8_t;    typedef uint8_t   uint_least8_t;
typedef int16_t   int_least16_t;   typedef uint16_t  uint_least16_t;
typedef int32_t   int_least32_t;   typedef uint32_t  uint_least32_t;
typedef int64_t   int_least64_t;   typedef uint64_t  uint_least64_t;

// A register is 32 bits wide, so nothing narrower is faster - but a 64-bit value needs the wide bank
// to be held whole, so the fast 64-bit types are the 64-bit ones.
typedef int32_t   int_fast8_t;     typedef uint32_t  uint_fast8_t;
typedef int32_t   int_fast16_t;    typedef uint32_t  uint_fast16_t;
typedef int32_t   int_fast32_t;    typedef uint32_t  uint_fast32_t;
typedef int64_t   int_fast64_t;    typedef uint64_t  uint_fast64_t;

typedef signed long long        intmax_t;
typedef unsigned long long      uintmax_t;

typedef int                     intptr_t;
typedef unsigned int            uintptr_t;

#define INT8_MIN     (-128)
#define INT8_MAX     127
#define UINT8_MAX    255
#define INT16_MIN    (-32768)
#define INT16_MAX    32767
#define UINT16_MAX   65535
#define INT32_MIN    (-2147483647 - 1)
#define INT32_MAX    2147483647
#define UINT32_MAX   4294967295u
// 2^63 does not fit a signed type and 2^64-1 does not fit any 32-bit literal, so both are spelled
// from their limits, and the unsigned ones carry the `ull` suffix.
#define INT64_MIN    (-9223372036854775807LL - 1)
#define INT64_MAX    9223372036854775807LL
#define UINT64_MAX   18446744073709551615ULL

#define INT_LEAST8_MIN    INT8_MIN
#define INT_LEAST8_MAX    INT8_MAX
#define UINT_LEAST8_MAX   UINT8_MAX
#define INT_LEAST16_MIN   INT16_MIN
#define INT_LEAST16_MAX   INT16_MAX
#define UINT_LEAST16_MAX  UINT16_MAX
#define INT_LEAST32_MIN   INT32_MIN
#define INT_LEAST32_MAX   INT32_MAX
#define UINT_LEAST32_MAX  UINT32_MAX
#define INT_LEAST64_MIN   INT64_MIN
#define INT_LEAST64_MAX   INT64_MAX
#define UINT_LEAST64_MAX  UINT64_MAX

#define INT_FAST8_MIN     INT32_MIN
#define INT_FAST8_MAX     INT32_MAX
#define UINT_FAST8_MAX    UINT32_MAX
#define INT_FAST16_MIN    INT32_MIN
#define INT_FAST16_MAX    INT32_MAX
#define UINT_FAST16_MAX   UINT32_MAX
#define INT_FAST32_MIN    INT32_MIN
#define INT_FAST32_MAX    INT32_MAX
#define UINT_FAST32_MAX   UINT32_MAX
#define INT_FAST64_MIN    INT64_MIN
#define INT_FAST64_MAX    INT64_MAX
#define UINT_FAST64_MAX   UINT64_MAX

#define INTPTR_MIN    INT32_MIN
#define INTPTR_MAX    INT32_MAX
#define UINTPTR_MAX   UINT32_MAX
#define INTMAX_MIN    INT64_MIN
#define INTMAX_MAX    INT64_MAX
#define UINTMAX_MAX   UINT64_MAX
#define PTRDIFF_MIN   INT32_MIN
#define PTRDIFF_MAX   INT32_MAX
#define WCHAR_MIN     INT32_MIN      // wchar_t is int (<stddef.h>)
#define WCHAR_MAX     INT32_MAX

#ifndef SIZE_MAX
#define SIZE_MAX      UINT32_MAX     // <limits.h> spells it UINT_MAX; whichever comes first wins
#endif

// Integer constants of a given width. The unsigned ones carry the `u`: a bare 4000000000 would
// not fit an int. The 64-bit ones carry `ull`/`ll`, which is what gives the literal its 64-bit type.
#define INT8_C(v)     (v)
#define INT16_C(v)    (v)
#define INT32_C(v)    (v)
#define INT64_C(v)    (v##ll)
#define UINT8_C(v)    (v##u)
#define UINT16_C(v)   (v##u)
#define UINT32_C(v)   (v##u)
#define UINT64_C(v)   (v##ull)
#define INTMAX_C(v)   (v##ll)
#define UINTMAX_C(v)  (v##ull)
