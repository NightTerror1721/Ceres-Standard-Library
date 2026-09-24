#pragma once

#include "../../stddef.h"

// A string that grows as text is appended, so a message can be built piece by piece without strcat's
// repeated scans or a buffer size guessed in advance. The text is always NUL-terminated. Every operation
// that can run out of memory returns -1 and leaves what was already there intact.
//
//   struct strbuf sb;  sb_init(&sb);
//   sb_appendf(&sb, "%d items:", n);  sb_append_char(&sb, ' ');  sb_append(&sb, name);
//   puts(sb_cstr(&sb));  sb_free(&sb);

struct strbuf
{
    char* data;                // NULL until something is appended
    unsigned int len;          // characters, not counting the NUL
    unsigned int cap;          // bytes allocated
};

void sb_init(struct strbuf* s);
void sb_free(struct strbuf* s);
int  sb_append(struct strbuf* s, const char* text);
int  sb_append_n(struct strbuf* s, const char* text, unsigned int n);   // at most n characters
int  sb_append_char(struct strbuf* s, char c);
int  sb_appendf(struct strbuf* s, const char* fmt, ...) __attribute__((__format__(__printf__, 2, 3)));   // printf-style
const char* sb_cstr(const struct strbuf* s);                           // never NULL: "" when empty
char* sb_take(struct strbuf* s);                                       // hands the memory to the caller (free it); the strbuf is empty again. NULL when out of memory
void sb_clear(struct strbuf* s);                                       // empties it, keeps the memory

static inline unsigned int sb_len(const struct strbuf* s) { return s->len; }
