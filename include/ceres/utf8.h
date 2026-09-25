#pragma once

#include "../stddef.h"

// UTF-8, the encoding of every string in this library: source files, the terminal, the files a program writes, and
// the multibyte strings of <stdlib.h>, <wchar.h> and <uchar.h> (MB_CUR_MAX is 4). The keyboard hands over typed
// characters as code points (kbd_read_text), and the text framebuffer and the pixel font show Latin-1 - U+0000 to
// U+00FF, the letters of the western European languages - so fb_text() and font_text() take UTF-8 and draw every
// character up to U+00FF; a character above that shows as '?' in a cell and as a box in the font.
//
//   const char* p = "año";
//   unsigned int c;
//   while ((c = utf8_next(&p)) != 0) ...      // 'a', 0xF1, 'o'
//
// A well-formed sequence is the shortest one for a code point up to U+10FFFF that is not a surrogate (U+D800 to
// U+DFFF). Anything else - a stray continuation byte, an overlong form, a sequence cut short - is not UTF-8:
// utf8_decode says so with -1, and utf8_next() reads each such byte as U+FFFD, the replacement character, so text
// from outside always comes through, marked where it was broken.

#define UTF8_MAX          4               // bytes in the longest character
#define UTF8_REPLACEMENT  0xFFFDu         // what utf8_next() gives for a byte that is not UTF-8
#define UNICODE_MAX       0x10FFFFu

// The character at s, looking at no more than n bytes: its length (1 to 4) with the code point in *cp (a NUL is
// U+0000, length 1); 0 when n is 0; -1 when the bytes there are not UTF-8.
int utf8_decode(const char* s, size_t n, unsigned int* cp);

// cp as UTF-8 into out, which has room for UTF8_MAX bytes (no terminator is written): the bytes written, or 0 when
// cp is not a character (a surrogate, or above U+10FFFF).
int utf8_encode(unsigned int cp, char* out);

// The character at *s, moving *s past it. At the terminator it returns 0 and leaves *s there; a byte that is not
// UTF-8 is U+FFFD, and *s moves one byte.
unsigned int utf8_next(const char** s);

size_t utf8_length(const char* s);               // the characters in a string (a broken byte counts as one)
int    utf8_valid(const char* s, size_t n);      // 1 when all n bytes are well-formed UTF-8
const char* utf8_offset(const char* s, size_t index);   // where character `index` starts (the terminator past the end)
int    utf8_width(unsigned int cp);              // the bytes cp takes in UTF-8: 1 to 4, 0 when it is not a character

// Between UTF-8 and Latin-1 (one byte a character, U+0000 to U+00FF). Both write at most size - 1 bytes and a
// terminator, as strlcpy does, and a character is never cut: what does not fit whole is left out. They return the
// length the whole conversion takes, so a result >= size means it was cut short.
size_t utf8_to_latin1(char* dst, size_t size, const char* src, char unknown);   // a character above U+00FF is `unknown`
size_t latin1_to_utf8(char* dst, size_t size, const char* src);
