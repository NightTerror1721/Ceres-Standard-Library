#pragma once

// Fixed-width integers for a 32-bit machine. Nothing 64-bit is defined: `long long` is 32 bits here,
// so an int64_t that quietly held 32 would be a trap; not having one is honest.

typedef signed char             int8_t;
typedef unsigned char           uint8_t;
typedef signed short int        int16_t;
typedef unsigned short int      uint16_t;
typedef signed int              int32_t;
typedef unsigned int            uint32_t;

typedef int8_t    int_least8_t;    typedef uint8_t   uint_least8_t;
typedef int16_t   int_least16_t;   typedef uint16_t  uint_least16_t;
typedef int32_t   int_least32_t;   typedef uint32_t  uint_least32_t;

// A register is 32 bits wide, so nothing narrower is faster.
typedef int32_t   int_fast8_t;     typedef uint32_t  uint_fast8_t;
typedef int32_t   int_fast16_t;    typedef uint32_t  uint_fast16_t;
typedef int32_t   int_fast32_t;    typedef uint32_t  uint_fast32_t;

typedef signed int              intmax_t;
typedef unsigned int            uintmax_t;

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

#define INT_LEAST8_MIN    INT8_MIN
#define INT_LEAST8_MAX    INT8_MAX
#define UINT_LEAST8_MAX   UINT8_MAX
#define INT_LEAST16_MIN   INT16_MIN
#define INT_LEAST16_MAX   INT16_MAX
#define UINT_LEAST16_MAX  UINT16_MAX
#define INT_LEAST32_MIN   INT32_MIN
#define INT_LEAST32_MAX   INT32_MAX
#define UINT_LEAST32_MAX  UINT32_MAX

#define INT_FAST8_MIN     INT32_MIN
#define INT_FAST8_MAX     INT32_MAX
#define UINT_FAST8_MAX    UINT32_MAX
#define INT_FAST16_MIN    INT32_MIN
#define INT_FAST16_MAX    INT32_MAX
#define UINT_FAST16_MAX   UINT32_MAX
#define INT_FAST32_MIN    INT32_MIN
#define INT_FAST32_MAX    INT32_MAX
#define UINT_FAST32_MAX   UINT32_MAX

#define INTPTR_MIN    INT32_MIN
#define INTPTR_MAX    INT32_MAX
#define UINTPTR_MAX   UINT32_MAX
#define INTMAX_MIN    INT32_MIN
#define INTMAX_MAX    INT32_MAX
#define UINTMAX_MAX   UINT32_MAX
#define PTRDIFF_MIN   INT32_MIN
#define PTRDIFF_MAX   INT32_MAX

#ifndef SIZE_MAX
#define SIZE_MAX      UINT32_MAX     // <limits.h> spells it UINT_MAX; whichever comes first wins
#endif

// Integer constants of a given width. The unsigned ones carry the `u`: a bare 4000000000 would
// not fit an int.
#define INT8_C(v)     (v)
#define INT16_C(v)    (v)
#define INT32_C(v)    (v)
#define UINT8_C(v)    (v##u)
#define UINT16_C(v)   (v##u)
#define UINT32_C(v)   (v##u)
#define INTMAX_C(v)   (v)
#define UINTMAX_C(v)  (v##u)
