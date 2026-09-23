#pragma once

#include "stdint.h"

// printf/scanf conversion specifiers for the fixed-width types, e.g.
//     printf("%" PRIu32 " bytes\n", n);
// `long` is 32 bits here, so the 32-bit specifiers carry no length modifier; the 64-bit ones carry
// `ll`, which `%lld`/`%llu`/`%llx` and the `ll` scanf forms understand.

#define PRId8    "d"
#define PRId16   "d"
#define PRId32   "d"
#define PRId64   "lld"
#define PRIi8    "i"
#define PRIi16   "i"
#define PRIi32   "i"
#define PRIi64   "lli"
#define PRIu8    "u"
#define PRIu16   "u"
#define PRIu32   "u"
#define PRIu64   "llu"
#define PRIo8    "o"
#define PRIo16   "o"
#define PRIo32   "o"
#define PRIo64   "llo"
#define PRIx8    "x"
#define PRIx16   "x"
#define PRIx32   "x"
#define PRIx64   "llx"
#define PRIX8    "X"
#define PRIX16   "X"
#define PRIX32   "X"
#define PRIX64   "llX"

#define PRIdLEAST32  "d"
#define PRIuLEAST32  "u"
#define PRIdLEAST64  "lld"
#define PRIuLEAST64  "llu"
#define PRIdFAST32   "d"
#define PRIuFAST32   "u"
#define PRIdFAST64   "lld"
#define PRIuFAST64   "llu"
#define PRIdPTR      "d"
#define PRIiPTR      "i"
#define PRIuPTR      "u"
#define PRIxPTR      "x"
#define PRIXPTR      "X"
#define PRIdMAX      "lld"
#define PRIiMAX      "lli"
#define PRIuMAX      "llu"
#define PRIxMAX      "llx"
#define PRIXMAX      "llX"

#define SCNd8    "hhd"
#define SCNd16   "hd"
#define SCNd32   "d"
#define SCNd64   "lld"
#define SCNi32   "i"
#define SCNi64   "lli"
#define SCNu8    "hhu"
#define SCNu16   "hu"
#define SCNu32   "u"
#define SCNu64   "llu"
#define SCNo32   "o"
#define SCNo64   "llo"
#define SCNx32   "x"
#define SCNx64   "llx"
#define SCNdPTR  "d"
#define SCNuPTR  "u"
#define SCNxPTR  "x"
#define SCNdMAX  "lld"
#define SCNuMAX  "llu"
#define SCNxMAX  "llx"

intmax_t imaxabs(intmax_t v);
struct __imaxdiv_s { intmax_t quot; intmax_t rem; };
typedef struct __imaxdiv_s imaxdiv_t;
imaxdiv_t imaxdiv(intmax_t num, intmax_t den);
