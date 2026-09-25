# `<uchar.h>`

C11/C23 <uchar.h>: the character types of the prefixed literals, and the conversions between them and the multibyte strings of the rest of the library, which are UTF-8 (see ceres/utf8.h). Ceres-C gives u8'x' the type unsigned char, u'x' and u"x" unsigned short, U'x' and U"x" unsigned int (0f79da7), so these name them:

```c
char8_t   u8'a'    a UTF-8 code unit     (a u8"..." STRING keeps char elements, as in C17)
char16_t  u"..."   a UTF-16 code unit    (U+10000 and up take a surrogate pair)
char32_t  U"..."   a Unicode code point
```

mbrtoc32(&c, s, n, &state) reads one character from at most n bytes of s and returns the bytes it took, 0 for the NUL character, (size_t)-2 when the n bytes are the start of a character that goes on (the state keeps them: call again with the rest), or (size_t)-1 with errno EILSEQ when they are not UTF-8. mbrtoc16 gives a character above U+FFFF as two calls: the first returns the lead surrogate and the bytes, the next (size_t)-3 and the trail, having read nothing. mbrtoc8 hands out the UTF-8 units one by one the same way. The c...rtomb functions go the other way, writing at most MB_LEN_MAX bytes to s: c16rtomb keeps a lead surrogate in the state and writes the character when the trail comes (returning 0 for the lead), and c8rtomb writes nothing until the last unit of a character. A NULL state is each function's own, and a NULL s resets the state (c...rtomb then returns 1).

```c
typedef unsigned char  char8_t;
typedef unsigned short char16_t;
typedef unsigned int   char32_t;

#include "__mbstate.h"

#define __STDC_UTF_16__ 1   // char16_t values are UTF-16
#define __STDC_UTF_32__ 1   // char32_t values are UTF-32

size_t mbrtoc8(char8_t* pc8, const char* s, size_t n, mbstate_t* ps);
size_t c8rtomb(char* s, char8_t c8, mbstate_t* ps);
size_t mbrtoc16(char16_t* pc16, const char* s, size_t n, mbstate_t* ps);
size_t c16rtomb(char* s, char16_t c16, mbstate_t* ps);
size_t mbrtoc32(char32_t* pc32, const char* s, size_t n, mbstate_t* ps);
size_t c32rtomb(char* s, char32_t c32, mbstate_t* ps);
```
