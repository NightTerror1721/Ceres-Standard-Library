#pragma once

// Non-local jumps (asm/setjmp.casm).
//
//   jmp_buf env;
//   if (setjmp(env) == 0) { ...work that may call longjmp(env, code)... }
//   else                  { ...arrive here with code... }
//
// As in C: a local variable that changes between setjmp and longjmp has an INDETERMINATE value
// afterwards unless it is volatile (the compiler keeps locals in the registers longjmp restores from
// the moment of setjmp). longjmp must not be called once the function that called setjmp has
// returned. Do not jump out of an __interrupt handler: it never executes `iret`, so the flags it
// masked stay masked.

typedef unsigned int jmp_buf[16];       // r8-r11, fp, sp, pc, f8-f15 (15 words used)

int  setjmp(jmp_buf env);               // 0 when called directly, else the value given to longjmp
void longjmp(jmp_buf env, int val) __attribute__((__noreturn__));   // longjmp(env, 0) makes setjmp return 1
