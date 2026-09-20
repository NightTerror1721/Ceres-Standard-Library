// Growing strings. See ceres/ds/strbuf.h.
#include "ceres/ds/strbuf.h"
#include "stdio.h"
#include "stdarg.h"
#include "stdlib.h"
#include "string.h"

void sb_init(struct strbuf* s)
{
    s->data = NULL;
    s->len = 0;
    s->cap = 0;
}

void sb_free(struct strbuf* s)
{
    free(s->data);
    sb_init(s);
}

// Room for `extra` more characters and the NUL.
static int grow(struct strbuf* s, unsigned int extra)
{
    if (extra > 0xFFFFFFFFu - s->len - 1u)
        return -1;
    unsigned int need = s->len + extra + 1u;
    if (need <= s->cap)
        return 0;
    unsigned int cap = s->cap == 0 ? 16u : s->cap;
    while (cap < need)
    {
        if (cap > 0x7FFFFFFFu)
        {
            cap = need;
            break;
        }
        cap *= 2u;
    }
    char* bigger = (char*)realloc(s->data, cap);
    if (bigger == NULL)
        return -1;
    s->data = bigger;
    s->cap = cap;
    return 0;
}

int sb_append_n(struct strbuf* s, const char* text, unsigned int n)
{
    unsigned int have = 0;
    while (have < n && text[have] != '\0')
        have++;
    if (grow(s, have) != 0)
        return -1;
    memcpy(s->data + s->len, text, have);
    s->len += have;
    s->data[s->len] = '\0';
    return 0;
}

int sb_append(struct strbuf* s, const char* text)
{
    return sb_append_n(s, text, (unsigned int)strlen(text));
}

int sb_append_char(struct strbuf* s, char c)
{
    if (grow(s, 1) != 0)
        return -1;
    s->data[s->len++] = c;
    s->data[s->len] = '\0';
    return 0;
}

int sb_appendf(struct strbuf* s, const char* fmt, ...)
{
    va_list ap, again;
    va_start(ap, fmt);
    va_copy(again, ap);
    int need = vsnprintf(NULL, 0, fmt, ap);             // pass one: how long is it?
    va_end(ap);
    if (need < 0 || grow(s, (unsigned int)need) != 0)
    {
        va_end(again);
        return -1;
    }
    vsnprintf(s->data + s->len, (size_t)need + 1, fmt, again);      // pass two: write it
    va_end(again);
    s->len += (unsigned int)need;
    return 0;
}

const char* sb_cstr(const struct strbuf* s)
{
    return s->data == NULL ? "" : s->data;
}

char* sb_take(struct strbuf* s)
{
    char* out = s->data;
    if (out == NULL)                                    // nothing was ever appended: an empty string the caller can free
    {
        out = (char*)malloc(1);
        if (out != NULL)
            out[0] = '\0';
    }
    sb_init(s);
    return out;
}

void sb_clear(struct strbuf* s)
{
    s->len = 0;
    if (s->data != NULL)
        s->data[0] = '\0';
}
