#pragma once
// Exact conversions between float and decimal text (src/fconv.c), shared by the format engine (printf's %e %f %g),
// strtof and the shortest-digits printer. NOT installed in include/.

#define FCONV_MAX_DIGITS 120   // enough to decide every rounding of a float exactly (its halfway points have ~105)

// The digits of |v| (finite, nonzero), correctly rounded (to nearest, ties to even) on its exact value:
//   significant != 0: `count` significant digits;
//   significant == 0: as many as `count` digits after the decimal point asks for (none when it rounds to 0).
// `out` receives the digits (at most FCONV_MAX_DIGITS; a request for more gets the exact ones and the caller adds
// zeros), and *exp10 the power of ten of the first: the value is d.ddd x 10^exp10. Returns how many digits were
// written, 0 when the value rounds to zero at that position.
int __fconv_digits(float v, int significant, int count, char* out, int* exp10);

// The fewest significant digits (1..9) that read back as exactly v; same result shape as above.
int __fconv_shortest(float v, char* out, int* exp10);

// The float nearest D x 10^exp10, where D is the `n` decimal digits (characters '0'..'9', no leading zeros needed)
// and `sticky` says digits were dropped after them that were not all zero. *range is set to 1 when the result
// overflowed to infinity or underflowed (to zero, or to a subnormal that is not exact).
float __fconv_decimal(const char* digits, int n, int sticky, int exp10, int* range);

// The float nearest H x 2^exp2, H the 64-bit hexadecimal mantissa of "0x1.8p3"; `sticky` as above.
float __fconv_binary(unsigned long long h, int sticky, int exp2, int* range);
