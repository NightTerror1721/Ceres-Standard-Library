// <string.h> and <strings.h> except the memory primitives and strlen (asm/memory.casm) and strcpy,
// strcmp, strchr and memchr (asm/string_fast.casm), which go a word at a time.
#include "string.h"
#include "strings.h"
#include "ceres/heap.h"      // malloc, for strdup/strndup
#include "ctype.h"

void* memrchr(const void* p, int c, size_t n)
{
    const unsigned char* s = (const unsigned char*)p;
    while (n > 0)
    {
        n--;
        if (s[n] == (unsigned char)c) return (void*)(s + n);
    }
    return 0;
}

void* memmem(const void* hay, size_t hl, const void* needle, size_t nl)
{
    if (nl == 0) return (void*)hay;
    if (hl < nl) return 0;
    const unsigned char* h = (const unsigned char*)hay;
    const unsigned char* nd = (const unsigned char*)needle;
    for (size_t i = 0; i + nl <= hl; i++)
        if (h[i] == nd[0] && memcmp(h + i, nd, nl) == 0) return (void*)(h + i);
    return 0;
}

size_t strnlen(const char* s, size_t max)
{
    size_t n = 0;
    while (n < max && s[n] != 0) n++;
    return n;
}

char* strncpy(char* restrict dst, const char* restrict src, size_t n)
{
    size_t i = 0;
    while (i < n && src[i] != 0) { dst[i] = src[i]; i++; }
    while (i < n) { dst[i] = 0; i++; }                 // C pads the rest with zeros
    return dst;
}

char* strcat(char* restrict dst, const char* restrict src)
{
    strcpy(dst + strlen(dst), src);
    return dst;
}

char* strncat(char* restrict dst, const char* restrict src, size_t n)
{
    size_t d = strlen(dst);
    size_t i = 0;
    while (i < n && src[i] != 0) { dst[d + i] = src[i]; i++; }
    dst[d + i] = 0;
    return dst;
}

int strncmp(const char* a, const char* b, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        unsigned char x = (unsigned char)a[i];
        unsigned char y = (unsigned char)b[i];
        if (x != y) return (int)x - (int)y;
        if (x == 0) return 0;
    }
    return 0;
}

int strcoll(const char* a, const char* b) { return strcmp(a, b); }

size_t strxfrm(char* dst, const char* src, size_t n)
{
    size_t len = strlen(src);
    if (n != 0) { strncpy(dst, src, n - 1); dst[n - 1] = 0; }
    return len;
}

char* strrchr(const char* s, int c)
{
    const char* last = 0;
    for (size_t i = 0; ; i++)
    {
        if (s[i] == (char)c) last = s + i;
        if (s[i] == 0) return (char*)last;
    }
}

char* strstr(const char* hay, const char* needle)
{
    size_t nl = strlen(needle);
    if (nl == 0) return (char*)hay;
    for (size_t i = 0; hay[i] != 0; i++)
        if (hay[i] == needle[0] && strncmp(hay + i, needle, nl) == 0) return (char*)(hay + i);
    return 0;
}

size_t strspn(const char* s, const char* accept)
{
    size_t n = 0;
    while (s[n] != 0 && strchr(accept, s[n]) != 0) n++;
    return n;
}

size_t strcspn(const char* s, const char* reject)
{
    size_t n = 0;
    while (s[n] != 0 && strchr(reject, s[n]) == 0) n++;
    return n;
}

char* strpbrk(const char* s, const char* accept)
{
    for (size_t i = 0; s[i] != 0; i++)
        if (strchr(accept, s[i]) != 0) return (char*)(s + i);
    return 0;
}

char* strtok_r(char* s, const char* delim, char** save)
{
    if (s == 0) s = *save;
    if (s == 0) return 0;
    s += strspn(s, delim);                             // skip leading delimiters
    if (*s == 0) { *save = 0; return 0; }
    char* end = s + strcspn(s, delim);
    if (*end == 0) *save = 0;
    else { *end = 0; *save = end + 1; }
    return s;
}

static char* strtok_state = 0;
char* strtok(char* s, const char* delim) { return strtok_r(s, delim, &strtok_state); }

char* strsep(char** s, const char* delim)
{
    char* start = *s;
    if (start == 0) return 0;
    char* end = start + strcspn(start, delim);
    if (*end == 0) *s = 0;
    else { *end = 0; *s = end + 1; }
    return start;
}

size_t strlcpy(char* dst, const char* src, size_t size)
{
    size_t len = strlen(src);
    if (size != 0)
    {
        size_t n = len < size - 1 ? len : size - 1;
        memcpy(dst, src, n);
        dst[n] = 0;
    }
    return len;
}

size_t strlcat(char* dst, const char* src, size_t size)
{
    size_t dl = strnlen(dst, size);
    if (dl == size) return size + strlen(src);
    return dl + strlcpy(dst + dl, src, size - dl);
}

char* strdup(const char* s)
{
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (p != 0) memcpy(p, s, n);
    return p;
}

char* strndup(const char* s, size_t max)
{
    size_t n = strnlen(s, max);
    char* p = (char*)malloc(n + 1);
    if (p != 0) { memcpy(p, s, n); p[n] = 0; }
    return p;
}

char* strlwr(char* s) { for (size_t i = 0; s[i] != 0; i++) s[i] = (char)tolower(s[i]); return s; }
char* strupr(char* s) { for (size_t i = 0; s[i] != 0; i++) s[i] = (char)toupper(s[i]); return s; }

char* strrev(char* s)
{
    size_t n = strlen(s);
    for (size_t i = 0; i < n / 2; i++)
    {
        char t = s[i];
        s[i] = s[n - 1 - i];
        s[n - 1 - i] = t;
    }
    return s;
}

// ---- <strings.h> ----
int strcasecmp(const char* a, const char* b)
{
    size_t i = 0;
    while (a[i] != 0 && tolower(a[i]) == tolower(b[i])) i++;
    return tolower((unsigned char)a[i]) - tolower((unsigned char)b[i]);
}

int strncasecmp(const char* a, const char* b, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        int x = tolower((unsigned char)a[i]);
        int y = tolower((unsigned char)b[i]);
        if (x != y) return x - y;
        if (x == 0) return 0;
    }
    return 0;
}

void bzero(void* p, size_t n) { memset(p, 0, n); }
void bcopy(const void* src, void* dst, size_t n) { memmove(dst, src, n); }
char* index(const char* s, int c) { return strchr(s, c); }
char* rindex(const char* s, int c) { return strrchr(s, c); }

// strerror() lives in errno.c, next to the error codes it describes.
