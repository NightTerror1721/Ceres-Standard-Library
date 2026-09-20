#pragma once

#include "stddef.h"

// BSD string utilities (src/string.c).
int    strcasecmp(const char* a, const char* b);
int    strncasecmp(const char* a, const char* b, size_t n);
void   bzero(void* p, size_t n);
void   bcopy(const void* src, void* dst, size_t n);
char*  index(const char* s, int c);
char*  rindex(const char* s, int c);
