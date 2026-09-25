# `<stdnoreturn.h>`

C11 <stdnoreturn.h>: `noreturn` for the _Noreturn function specifier (which Ceres-C predefines as __attribute__((__noreturn__)), since 558335b). The library's own headers spell the attribute __noreturn__, so including this first does not disturb them.

```c
#define noreturn _Noreturn
```
