# `<stdarg.h>`

Variable arguments. The five operations are compiler builtins (there is no system include directory to find a <stdarg.h> in), so this header only gives them their standard names.

As in C, a float is promoted to double in a variadic call - read it with va_arg(ap, double) - and a char or a short to int: va_arg(ap, float), va_arg(ap, char) and va_arg(ap, short) are rejected. Structs and unions cannot pass through `...`. See Ceres-C docs/09-Variadic-Convention.md.

```c
typedef __builtin_va_list va_list;

#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, T)      __builtin_va_arg(ap, T)
#define va_end(ap)         __builtin_va_end(ap)
#define va_copy(dst, src)  __builtin_va_copy(dst, src)
```
