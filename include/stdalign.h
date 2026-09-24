#pragma once

// C11 <stdalign.h>. alignof is a keyword of Ceres-C, and _Alignof is predefined as it (Ceres-C 558335b).
//
// alignas takes a NUMBER here, not a type: it is the aligned attribute, whose argument must be a constant
// power of two. Up to 4 it is what every scalar has already; above 4 ceresc ignores it with a warning
// (W2002), because the linker places every section on a 4-byte boundary and cannot keep more.
#define __alignof_is_defined 1
#define __alignas_is_defined 1
#define alignas(n)  __attribute__((__aligned__(n)))
#define _Alignas(n) __attribute__((__aligned__(n)))
