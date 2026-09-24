#pragma once

#include "stddef.h"

// C11/C23 <uchar.h>: the character types of the prefixed literals. Ceres-C gives u8'x' the type
// unsigned char, u'x' and u"x" unsigned short, U'x' and U"x" unsigned int (0f79da7), so these name them:
//
//   char8_t   u8'a'    a UTF-8 code unit     (a u8"..." STRING keeps char elements, as in C17)
//   char16_t  u"..."   a UTF-16 code unit    (U+10000 and up take a surrogate pair)
//   char32_t  U"..."   a Unicode code point
//
// The conversion functions of the standard header (mbrtoc8, c8rtomb, mbrtoc16, c16rtomb, mbrtoc32,
// c32rtomb) and mbstate_t are not provided: this library's multibyte encoding is one byte per character
// (MB_CUR_MAX is 1), so there is no multibyte state for them to carry.

typedef unsigned char  char8_t;
typedef unsigned short char16_t;
typedef unsigned int   char32_t;

#define __STDC_UTF_16__ 1   // char16_t values are UTF-16
#define __STDC_UTF_32__ 1   // char32_t values are UTF-32
