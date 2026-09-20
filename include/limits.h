#pragma once

// Ceres is a 32-bit machine: `long` and `long long` are `int` (the compiler caps them, with W2001).

#define CHAR_BIT    8
#define SCHAR_MIN   (-128)
#define SCHAR_MAX   127
#define UCHAR_MAX   255
#define CHAR_MIN    SCHAR_MIN       // plain char is signed (it is loaded with ldrsb)
#define CHAR_MAX    SCHAR_MAX

#define SHRT_MIN    (-32768)
#define SHRT_MAX    32767
#define USHRT_MAX   65535

// INT_MIN is spelled (-INT_MAX - 1): 2147483648 does not fit an int, so the literal form only
// works by accident. UINT_MAX needs its `u` - without it the constant is -1 and `UINT_MAX > 0`
// is false.
#define INT_MAX     2147483647
#define INT_MIN     (-2147483647 - 1)
#define UINT_MAX    4294967295u

#define LONG_MAX    INT_MAX
#define LONG_MIN    INT_MIN
#define ULONG_MAX   UINT_MAX

#define LLONG_MAX   INT_MAX
#define LLONG_MIN   INT_MIN
#define ULLONG_MAX  UINT_MAX

#define SIZE_MAX    UINT_MAX
#define MB_LEN_MAX  1
