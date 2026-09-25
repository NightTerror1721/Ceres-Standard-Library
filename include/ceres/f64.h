#pragma once

// IEEE 754 binary64 - a real double - in software, on the bits of one in an unsigned long long. The machine's only
// floating point is binary32 (and C's double is float); these do the arithmetic a double needs, rounded to nearest
// with ties to even like a hardware FPU, with subnormals, infinities, signed zeros and NaN. Ceres-C calls them for
// every double operation when a program is compiled with -fsoft-double; a program can also call them itself.
//
//   f64 third = __f64_div(__f64_from_i32(1), __f64_from_i32(3));   // 0x3FD5555555555555
//
// A NaN result is the quiet NaN 0x7FF8000000000000 unless an operand was a NaN, whose payload it keeps (quieted).
// Converting to an integer truncates toward zero; a value out of the integer's range, or a NaN, gives the nearest
// end of the range (0 for a NaN). There are no exception flags.

typedef unsigned long long f64;

#define F64_ZERO      0x0000000000000000ull
#define F64_ONE       0x3FF0000000000000ull
#define F64_INFINITY  0x7FF0000000000000ull
#define F64_NAN       0x7FF8000000000000ull

f64 __f64_add(f64 a, f64 b);
f64 __f64_sub(f64 a, f64 b);
f64 __f64_mul(f64 a, f64 b);
f64 __f64_div(f64 a, f64 b);
f64 __f64_sqrt(f64 a);
f64 __f64_neg(f64 a);
int __f64_cmp(f64 a, f64 b);                   // -1, 0 or 1; 2 when either is a NaN (-0 equals +0)

f64 __f64_from_f32(float x);                  // exact
float __f64_to_f32(f64 a);                    // rounded to nearest
f64 __f64_from_i32(int x);                    // exact
f64 __f64_from_u32(unsigned int x);
f64 __f64_from_i64(long long x);              // rounded when it needs more than 53 bits
f64 __f64_from_u64(unsigned long long x);
int __f64_to_i32(f64 a);
unsigned int __f64_to_u32(f64 a);
long long __f64_to_i64(f64 a);
unsigned long long __f64_to_u64(f64 a);
