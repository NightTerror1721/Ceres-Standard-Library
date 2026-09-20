#pragma once

// Ceres has one floating-point format: IEEE 754 binary32. `double` and `long double` are `float`
// (the compiler reduces them, with W2001), so all three families carry the same numbers.

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
