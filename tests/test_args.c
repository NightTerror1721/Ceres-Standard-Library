// main's arguments and the environment: the test runs as `ceres run ... --env HOME=/save --env LANG=es -- one two`
// (tests/expected/test_args.run).
#include "ceres/test.h"
#include "ceres/sys.h"
#include "stdlib.h"
#include "string.h"
#include "errno.h"

int main(int argc, char** argv)
{
    TEST_SECTION("argv");
    CHECK_EQ(argc, 3);
    CHECK(strstr(argv[0], "test_args") != 0);          // the program's path: it names the level, so not printed
    CHECK_STR(argv[1], "one");
    CHECK_STR(argv[2], "two");
    CHECK(argv[3] == 0);
    CHECK_EQ(sys_argc(), argc);
    CHECK(sys_argv() == argv);                          // the same array, reached without main

    TEST_SECTION("getenv");
    CHECK_STR(getenv("HOME"), "/save");
    CHECK_STR(getenv("LANG"), "es");
    CHECK(getenv("HOM") == 0);                          // a prefix is not a name
    CHECK(getenv("PATH") == 0);                         // nothing of the host's comes through
    char** envp = sys_envp();
    CHECK_STR(envp[0], "HOME=/save");
    CHECK(envp[2] == 0);

    TEST_SECTION("setenv and unsetenv");
    CHECK_EQ(setenv("HOME", "/other", 0), 0);
    CHECK_STR(getenv("HOME"), "/save");                 // not overwritten
    CHECK_EQ(setenv("HOME", "/other", 1), 0);
    CHECK_STR(getenv("HOME"), "/other");
    CHECK_EQ(setenv("LEVEL", "3", 0), 0);
    CHECK_STR(getenv("LEVEL"), "3");
    CHECK_EQ(setenv("EMPTY", "", 1), 0);
    CHECK_STR(getenv("EMPTY"), "");
    CHECK_EQ(unsetenv("LANG"), 0);
    CHECK(getenv("LANG") == 0);
    CHECK_STR(getenv("LEVEL"), "3");                    // the others stay
    CHECK_EQ(unsetenv("LANG"), 0);                      // not there: nothing to do
    for (int i = 0; i < 40; i++)                        // the copy grows
    {
        char name[8] = { 'V', (char)('A' + i / 26), (char)('A' + i % 26), 0 };
        CHECK_EQ(setenv(name, "x", 1), 0);
    }
    CHECK_STR(getenv("VBN"), "x");
    errno = 0;
    CHECK_EQ(setenv("A=B", "x", 1), -1);
    CHECK_EQ(errno, EINVAL);
    CHECK_EQ(setenv("", "x", 1), -1);
    CHECK_EQ(unsetenv(0), -1);
    return test_summary();
}
