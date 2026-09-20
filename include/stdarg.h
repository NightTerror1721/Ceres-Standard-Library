#pragma once

// Variable arguments. The five operations are compiler builtins (there is no system include
// directory to find a <stdarg.h> in), so this header only gives them their standard names.
//
// Two deliberate deviations from C, both because the machine has no 64-bit float:
//   * a float is NOT promoted to double in a variadic call: it travels as the f32 it is, and
//     va_arg(ap, double) reads that f32 back;
//   * va_arg(ap, char) and va_arg(ap, short) are rejected - read an int and convert.
// Structs and unions cannot pass through `...`.
// See Ceres-C docs/09-Variadic-Convention.md.

typedef __builtin_va_list va_list;

#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, T)      __builtin_va_arg(ap, T)
#define va_end(ap)         __builtin_va_end(ap)
#define va_copy(dst, src)  __builtin_va_copy(dst, src)
