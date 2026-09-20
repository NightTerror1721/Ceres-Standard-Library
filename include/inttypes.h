#pragma once

#include "stdint.h"

// printf/scanf conversion specifiers for the fixed-width types, e.g.
//     printf("%" PRIu32 " bytes\n", n);
// Nothing 64-bit exists (see <stdint.h>), and everything here is at most 32 bits, so the
// specifiers carry no length modifier.

#define PRId8    "d"
#define PRId16   "d"
#define PRId32   "d"
#define PRIi8    "i"
#define PRIi16   "i"
#define PRIi32   "i"
#define PRIu8    "u"
#define PRIu16   "u"
#define PRIu32   "u"
#define PRIo8    "o"
#define PRIo16   "o"
#define PRIo32   "o"
#define PRIx8    "x"
#define PRIx16   "x"
#define PRIx32   "x"
#define PRIX8    "X"
#define PRIX16   "X"
#define PRIX32   "X"

#define PRIdLEAST32  "d"
#define PRIuLEAST32  "u"
#define PRIdFAST32   "d"
#define PRIuFAST32   "u"
#define PRIdPTR      "d"
#define PRIiPTR      "i"
#define PRIuPTR      "u"
#define PRIxPTR      "x"
#define PRIXPTR      "X"
#define PRIdMAX      "d"
#define PRIuMAX      "u"
#define PRIxMAX      "x"

#define SCNd8    "hhd"
#define SCNd16   "hd"
#define SCNd32   "d"
#define SCNi32   "i"
#define SCNu8    "hhu"
#define SCNu16   "hu"
#define SCNu32   "u"
#define SCNo32   "o"
#define SCNx32   "x"
#define SCNdPTR  "d"
#define SCNuPTR  "u"
#define SCNxPTR  "x"

int imaxabs(int v);
