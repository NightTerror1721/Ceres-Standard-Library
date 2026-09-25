#pragma once

// NULL, size_t, ptrdiff_t, wchar_t and offsetof. A pointer, size_t and ptrdiff_t are all 32 bits.

#define NULL ((void*)0)

typedef unsigned int size_t;

typedef int ptrdiff_t;

// The type of a wide character, and of an L'x' literal and the elements of an L"..." string: int, as
// Ceres-C gives them. <stdlib.h> has it too, through this header.
typedef int wchar_t;

#define offsetof(T, m) ((size_t)&(((T*)0)->m))
