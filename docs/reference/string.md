# `<string.h>`

Memory (asm/memory.casm): word-at-a-time when both pointers are aligned, bytes otherwise. memmove picks its direction, so overlapping ranges are fine; memcpy's are not.

```c
void*  memcpy(void* restrict dst, const void* restrict src, size_t n);
void*  memmove(void* dst, const void* src, size_t n);
void*  memset(void* dst, int c, size_t n);
int    memcmp(const void* a, const void* b, size_t n) __attribute__((__pure__));   // unsigned bytes, like the standard says
size_t strlen(const char* s) __attribute__((__pure__));
void*  memchr(const void* p, int c, size_t n) __attribute__((__pure__));

// Strings (src/string.c).
size_t strnlen(const char* s, size_t max) __attribute__((__pure__));
char*  strcpy(char* restrict dst, const char* restrict src);
char*  strncpy(char* restrict dst, const char* restrict src, size_t n);   // pads with NULs, as C does
char*  strcat(char* restrict dst, const char* restrict src);
char*  strncat(char* restrict dst, const char* restrict src, size_t n);
int    strcmp(const char* a, const char* b) __attribute__((__pure__));             // unsigned comparison
int    strncmp(const char* a, const char* b, size_t n) __attribute__((__pure__));
int    strcoll(const char* a, const char* b) __attribute__((__pure__));            // the "C" locale: same as strcmp
size_t strxfrm(char* dst, const char* src, size_t n);
char*  strchr(const char* s, int c) __attribute__((__pure__));
char*  strrchr(const char* s, int c) __attribute__((__pure__));
char*  strstr(const char* hay, const char* needle) __attribute__((__pure__));
size_t strspn(const char* s, const char* accept) __attribute__((__pure__));
size_t strcspn(const char* s, const char* reject) __attribute__((__pure__));
char*  strpbrk(const char* s, const char* accept) __attribute__((__pure__));
char*  strtok(char* s, const char* delim);
char*  strtok_r(char* s, const char* delim, char** save);
char*  strerror(int errnum);

// BSD / POSIX / Ceres extensions.
char*  strdup(const char* s);                            // malloc'd: free() it
char*  strndup(const char* s, size_t n);
size_t strlcpy(char* dst, const char* src, size_t size); // returns strlen(src): truncation is n >= size
size_t strlcat(char* dst, const char* src, size_t size);
char*  strsep(char** s, const char* delim);
void*  memrchr(const void* p, int c, size_t n) __attribute__((__pure__));
void*  memmem(const void* hay, size_t hl, const void* needle, size_t nl) __attribute__((__pure__));
char*  strlwr(char* s);
char*  strupr(char* s);
char*  strrev(char* s);
void   memset32(unsigned int* dst, unsigned int value, size_t words);   // asm/memory.casm
```
