#pragma once

// Ceres has one floating-point format: IEEE 754 binary32. `double` and `long double` are `float`
// (the compiler reduces them, with W2001), so all three families carry the same numbers - unless the program is
// compiled with -fsoft-double, and then they are a real IEEE binary64 done in software (ceres/f64.h).

#define FLT_RADIX          2
#define FLT_MANT_DIG       24
#define FLT_DIG            6
#define FLT_DECIMAL_DIG    9              // digits that round-trip any float
#define FLT_MIN_EXP        (-125)
#define FLT_MIN_10_EXP     (-37)
#define FLT_MAX_EXP        128
#define FLT_MAX_10_EXP     38
#define FLT_MIN            1.17549435e-38f
#define FLT_MAX            3.40282347e+38f
#define FLT_EPSILON        1.19209290e-7f
#define FLT_TRUE_MIN       1.40129846e-45f   // smallest subnormal
#define FLT_HAS_SUBNORM    1

#define FLT_EVAL_METHOD    0
#define FLT_ROUNDS         1              // to nearest

#ifdef __CERES_SOFT_DOUBLE__
#define DBL_MANT_DIG       53
#define DBL_DIG            15
#define DBL_DECIMAL_DIG    17
#define DBL_MIN_EXP        (-1021)
#define DBL_MIN_10_EXP     (-307)
#define DBL_MAX_EXP        1024
#define DBL_MAX_10_EXP     308
#define DBL_MIN            2.2250738585072014e-308
#define DBL_MAX            1.7976931348623157e+308
#define DBL_EPSILON        2.2204460492503131e-16
#define DBL_TRUE_MIN       4.9406564584124654e-324
#define DBL_HAS_SUBNORM    1
#define DECIMAL_DIG        17
#define LDBL_MANT_DIG      DBL_MANT_DIG
#define LDBL_DIG           DBL_DIG
#define LDBL_DECIMAL_DIG   DBL_DECIMAL_DIG
#define LDBL_MIN_EXP       DBL_MIN_EXP
#define LDBL_MIN_10_EXP    DBL_MIN_10_EXP
#define LDBL_MAX_EXP       DBL_MAX_EXP
#define LDBL_MAX_10_EXP    DBL_MAX_10_EXP
#define LDBL_MIN           DBL_MIN
#define LDBL_MAX           DBL_MAX
#define LDBL_EPSILON       DBL_EPSILON
#define LDBL_TRUE_MIN      DBL_TRUE_MIN
#define LDBL_HAS_SUBNORM   1
#else
#define DBL_MANT_DIG       FLT_MANT_DIG
#define DBL_DIG            FLT_DIG
#define DBL_DECIMAL_DIG    FLT_DECIMAL_DIG
#define DBL_MIN_EXP        FLT_MIN_EXP
#define DBL_MIN_10_EXP     FLT_MIN_10_EXP
#define DBL_MAX_EXP        FLT_MAX_EXP
#define DBL_MAX_10_EXP     FLT_MAX_10_EXP
#define DBL_MIN            FLT_MIN
#define DBL_MAX            FLT_MAX
#define DBL_EPSILON        FLT_EPSILON
#define DBL_TRUE_MIN       FLT_TRUE_MIN
#define DBL_HAS_SUBNORM    FLT_HAS_SUBNORM

#define LDBL_MANT_DIG      FLT_MANT_DIG
#define LDBL_DIG           FLT_DIG
#define LDBL_DECIMAL_DIG   FLT_DECIMAL_DIG
#define LDBL_MIN_EXP       FLT_MIN_EXP
#define LDBL_MIN_10_EXP    FLT_MIN_10_EXP
#define LDBL_MAX_EXP       FLT_MAX_EXP
#define LDBL_MAX_10_EXP    FLT_MAX_10_EXP
#define LDBL_MIN           FLT_MIN
#define LDBL_MAX           FLT_MAX
#define LDBL_EPSILON       FLT_EPSILON
#define LDBL_TRUE_MIN      FLT_TRUE_MIN
#define LDBL_HAS_SUBNORM   FLT_HAS_SUBNORM
#endif
