# `<limits.h>`

Ceres is a 32-bit machine: `long` is `int` (32 bits). `long long` is a real 8-byte type, so its limits below are the true 64-bit ones.

```c
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
// is false. The 64-bit spellings likewise come from their max plus a `ll`/`ull` suffix.
#define INT_MAX     2147483647
#define INT_MIN     (-2147483647 - 1)
#define UINT_MAX    4294967295u

#define LONG_MAX    INT_MAX
#define LONG_MIN    INT_MIN
#define ULONG_MAX   UINT_MAX

#define LLONG_MAX   9223372036854775807LL
#define LLONG_MIN   (-9223372036854775807LL - 1)
#define ULLONG_MAX  18446744073709551615ULL

#define SIZE_MAX    UINT_MAX
#define MB_LEN_MAX  4                  // a UTF-8 character
```
