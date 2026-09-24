// The host's files (ceres/hostfs.h): the test runs with --host-dir build/host, a fresh directory holding a copy of
// tests/data/host (tests/expected/test_hostfs.run).
#include "ceres/test.h"
#include "ceres/hostfs.h"
#include "stdio.h"
#include "string.h"
#include "errno.h"

int main(void)
{
    TEST_SECTION("a directory is attached");
    CHECK_EQ(host_available(), 1);

    TEST_SECTION("reading what the host put there");
    int h = host_open("levels/one.txt", HOST_READ);
    CHECK(h >= 0);
    char buf[64];
    int n = host_read(h, buf, sizeof buf - 1);
    CHECK(n > 0);
    buf[n > 0 ? n : 0] = 0;
    printf("%s", buf);
    CHECK_EQ(host_size(h), n);
    CHECK_EQ(host_read(h, buf, 8), 0);                       // at the end
    CHECK_EQ(host_seek(h, 6, SEEK_SET), 6);
    CHECK_EQ(host_read(h, buf, 3), 3);
    CHECK(memcmp(buf, "one", 3) == 0);
    CHECK_EQ(host_close(h), 0);
    CHECK_EQ(host_close(h), -EBADF);

    TEST_SECTION("writing, appending, renaming, removing");
    h = host_open("save.dat", HOST_WRITE | HOST_CREATE | HOST_TRUNCATE);
    CHECK(h >= 0);
    CHECK_EQ(host_write(h, "score=10\n", 9), 9);
    CHECK_EQ(host_close(h), 0);
    h = host_open("save.dat", HOST_WRITE | HOST_APPEND);
    CHECK_EQ(host_write(h, "lives=3\n", 8), 8);
    CHECK_EQ(host_close(h), 0);
    CHECK_EQ(host_stat("save.dat"), 17);
    CHECK_EQ(host_open("save.dat", HOST_WRITE | HOST_CREATE | HOST_EXCLUSIVE), -EEXIST);
    CHECK_EQ(host_mkdir("saves"), 0);
    CHECK_EQ(host_rename("save.dat", "saves/slot1.dat"), 0);
    CHECK_EQ(host_stat("save.dat"), -ENOENT);
    CHECK_EQ(host_stat("saves"), -EISDIR);
    h = host_open("saves/slot1.dat", HOST_READ);
    n = host_read(h, buf, sizeof buf - 1);
    buf[n > 0 ? n : 0] = 0;
    CHECK_STR(buf, "score=10\nlives=3\n");
    host_close(h);

    TEST_SECTION("listing");
    char name[32];
    for (unsigned int i = 0; host_list("", i, name, sizeof name) > 0; i++)
        printf("  %s\n", name);
    CHECK_EQ(host_list("", 99, name, sizeof name), 0);
    CHECK_EQ(host_list("levels", 0, name, 4), -ENAMETOOLONG);   // "one.txt" does not fit in 4

    TEST_SECTION("what cannot be named");
    CHECK_EQ(host_open("../escape.txt", HOST_WRITE | HOST_CREATE), -EINVAL);
    CHECK_EQ(host_open("/abs.txt", HOST_READ), -EINVAL);
    CHECK_EQ(host_open("missing.txt", HOST_READ), -ENOENT);
    CHECK_EQ(host_remove("saves"), -ENOTEMPTY);
    CHECK_EQ(host_remove("saves/slot1.dat"), 0);
    CHECK_EQ(host_remove("saves"), 0);
    return test_summary();
}
