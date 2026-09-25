# `<ceres/config.h>`

Compile-time configuration. Each setting has a default here and can be changed for a whole build with a definition on the compiler's command line, which applies to every file the build compiles, the library's included:

```c
ceresc main.c <the library sources> -DCERES_ATEXIT_SLOTS=8 -DFS_MAX_OPEN=4 ... -o app.cres
```

Nothing else reads the environment: there is no file to edit, and a setting that is not defined is its default. Sizes are numbers (a literal, not an expression: some become array sizes).

```c
// How many functions atexit() can hold. exit() runs them last registered first. (src/exit.c)
#ifndef CERES_ATEXIT_SLOTS
#define CERES_ATEXIT_SLOTS 32
#endif

// Bytes malloc keeps free between the top of the heap and the stack pointer, so that the heap never grows
// into the stack. A program can change it at run time with heap_set_stack_reserve(). (src/malloc.c)
#ifndef CERES_HEAP_STACK_RESERVE
#define CERES_HEAP_STACK_RESERVE 16384
#endif

// 1 makes malloc check itself as it goes: a canary after every block, freed blocks filled with 0xDD, and
// heap_check() looking at both, so an overrun, a write after free, a double free or a bad free is reported
// through heap_set_error_handler() (by default: a message and abort). Costs 8 bytes a block and a pass over
// every freed block. (src/malloc.c)
#ifndef CERES_HEAP_DEBUG
#define CERES_HEAP_DEBUG 0
#endif

// How many CeresFS files can be open at once, and so how many fopen() streams. (ceres/fs.h)
#ifndef FS_MAX_OPEN
#define FS_MAX_OPEN 8
#endif

// How many tasks ceres/task.h can hold at once, main included, and the stack a task gets when it asks for 0.
#ifndef TASK_MAX
#define TASK_MAX 16
#endif
#ifndef TASK_STACK_DEFAULT
#define TASK_STACK_DEFAULT 8192
#endif

// How many software timers timer_after() and timer_every() can hold at once. (ceres/timer.h)
#ifndef TIMER_MAX_TASKS
#define TIMER_MAX_TASKS 8
#endif
```
