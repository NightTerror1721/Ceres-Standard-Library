#pragma once

#include "stdio.h"
#include "string.h"

// A minimal test framework. `ceres run` exits 0 on a clean halt and never with a value the program
// chose, so a test's verdict is its TEXT: the runner (tools/runtests.ps1) compares the program's
// output with tests/expected/<name>.expected. Every check that fails prints one FAIL line (file:line and the
// function it is in), and
// test_summary() prints either "ALL PASSED n/n" or "FAILED k of n".
//
// The macros use do { ... } while (0) and no comma operator (the C subset has none).
//
//   int main(void) { TEST_SECTION("bits"); CHECK_EQ(bit_clz(1), 31); return test_summary(); }

extern int __t_total;
extern int __t_failed;

#define CHECK(cond) \
    do { __t_total++; if (!(cond)) { __t_failed++; printf("FAIL %s:%d (%s): %s\n", __FILE__, __LINE__, __func__, #cond); } } while (0)

#define CHECK_EQ(a, b) \
    do { __t_total++; if ((a) != (b)) { __t_failed++; \
        printf("FAIL %s:%d (%s): %s == %s (%d vs %d)\n", __FILE__, __LINE__, __func__, #a, #b, (int)(a), (int)(b)); } } while (0)

#define CHECK_STR(a, b) \
    do { __t_total++; if (strcmp((a), (b)) != 0) { __t_failed++; \
        printf("FAIL %s:%d (%s): \"%s\" != \"%s\"\n", __FILE__, __LINE__, __func__, (a), (b)); } } while (0)

#define CHECK_NEAR(a, b, eps) \
    do { __t_total++; float __d = (a) - (b); if (__d < 0.0f) __d = -__d; if (__d > (eps)) { __t_failed++; \
        printf("FAIL %s:%d (%s): %s ~ %s\n", __FILE__, __LINE__, __func__, #a, #b); } } while (0)

#define TEST_SECTION(name) printf("-- %s\n", name)

int test_summary(void);   // prints the verdict; returns 0 when everything passed
