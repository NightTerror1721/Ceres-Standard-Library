// assert(expr) stops the program with "file:line: function: assertion 'expr' failed" when expr is 0.
// Defining NDEBUG before including this header removes every assert.
//
// `assert` is a reserved word in CASM, which is why it can only ever be a macro: a C function of
// that name does not assemble (E4004). The function behind it is __assert_fail.

void __assert_fail(const char* expr, const char* file, int line, const char* func) __attribute__((__noreturn__));   // prints, then stops the machine

// static_assert(condition, "message") is checked when the program is compiled and costs nothing when it
// holds: a false condition is a compile error that carries the message. The condition is a constant
// expression (sizeof, enumerators, arithmetic), and the statement may stand at file scope, in a block, or in
// a struct. The message is optional.
#ifndef static_assert
#define static_assert _Static_assert
#endif

// No include guard on purpose: the standard says assert.h can be included again after NDEBUG changes.
#undef assert
#ifdef NDEBUG
#define assert(e) ((void)0)
#else
#define assert(e) ((e) ? (void)0 : __assert_fail(#e, __FILE__, __LINE__, __func__))
#endif
