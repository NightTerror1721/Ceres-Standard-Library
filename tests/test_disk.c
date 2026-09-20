// The disk: sector reads and writes, the size it reports, and what happens past the end. Without
// `--disk` the machine has a 64-sector disk that lives as long as the run does.
#include "ceres/test.h"
#include "ceres/disk.h"
#include "string.h"

static unsigned char a[DISK_SECTOR_SIZE];
static unsigned char b[DISK_SECTOR_SIZE];
static unsigned char big[4 * DISK_SECTOR_SIZE];
static unsigned char back[4 * DISK_SECTOR_SIZE];

static void pattern(unsigned char* buf, int len, int seed)
{
    for (int i = 0; i < len; i++)
        buf[i] = (unsigned char)(seed + i * 5 + (i >> 8));
}

int main(void)
{
    TEST_SECTION("size");
    CHECK_EQ((int)disk_sectors(), 64);                    // 64 sectors: 32 KiB
    CHECK_EQ((int)disk_sectors(), 64);                    // the second answer comes from the cache
    CHECK_EQ(DISK_SECTOR_SIZE, 512);

    TEST_SECTION("one sector");
    memset(b, 0xCC, sizeof(b));
    CHECK_EQ(disk_read(0, b), 0);
    CHECK(b[0] == 0 && b[511] == 0);                      // a fresh disk is zeros
    pattern(a, sizeof(a), 1);
    CHECK_EQ(disk_write(0, a), 0);
    memset(b, 0xCC, sizeof(b));
    CHECK_EQ(disk_read(0, b), 0);
    CHECK(memcmp(a, b, sizeof(a)) == 0);
    pattern(a, sizeof(a), 77);
    CHECK_EQ(disk_write(7, a), 0);
    CHECK_EQ(disk_read(7, b), 0);
    CHECK(memcmp(a, b, sizeof(a)) == 0);
    CHECK_EQ(disk_read(0, b), 0);                         // sector 0 is still what was written there
    pattern(a, sizeof(a), 1);
    CHECK(memcmp(a, b, sizeof(a)) == 0);
    CHECK_EQ(disk_read(6, b), 0);
    CHECK(b[0] == 0 && b[100] == 0);                      // and its neighbours were not touched
    CHECK_EQ(disk_read(8, b), 0);
    CHECK(b[0] == 0 && b[100] == 0);

    TEST_SECTION("the edges of the disk");
    pattern(a, sizeof(a), 200);
    CHECK_EQ(disk_write(63, a), 0);                       // the last sector
    CHECK_EQ(disk_read(63, b), 0);
    CHECK(memcmp(a, b, sizeof(a)) == 0);
    CHECK_EQ(disk_read(64, b), -1);                       // one past the end
    CHECK_EQ(disk_write(64, a), -1);
    CHECK_EQ(disk_read(100000, b), -1);
    CHECK_EQ(disk_read(0xFFFFFFFFu, b), -1);
    CHECK_EQ(disk_write(0xFFFFFFFFu, a), -1);
    CHECK_EQ(disk_read(63, b), 0);                        // an error does not wedge the device
    CHECK(memcmp(a, b, sizeof(a)) == 0);
    CHECK_EQ(disk_read(3, 0), -1);                        // no buffer

    TEST_SECTION("a buffer that is not aligned");
    static unsigned char odd[DISK_SECTOR_SIZE + 3];
    pattern(odd + 1, DISK_SECTOR_SIZE, 9);
    CHECK_EQ(disk_write(20, odd + 1), 0);
    memset(odd, 0xAA, sizeof(odd));
    CHECK_EQ(disk_read(20, odd + 3), 0);
    unsigned char expect[DISK_SECTOR_SIZE];
    pattern(expect, DISK_SECTOR_SIZE, 9);
    CHECK(memcmp(odd + 3, expect, sizeof(expect)) == 0);
    CHECK(odd[0] == 0xAA && odd[2] == 0xAA);              // and nothing before the buffer was written

    TEST_SECTION("several sectors");
    pattern(big, sizeof(big), 33);
    CHECK_EQ(disk_write_n(10, big, 4), 0);
    memset(back, 0, sizeof(back));
    CHECK_EQ(disk_read_n(10, back, 4), 0);
    CHECK(memcmp(big, back, sizeof(big)) == 0);
    CHECK_EQ(disk_read(9, b), 0);                         // the sectors around the run are untouched
    CHECK(b[0] == 0 && b[300] == 0);
    CHECK_EQ(disk_read(14, b), 0);
    CHECK(b[0] == 0 && b[300] == 0);
    CHECK_EQ(disk_read(12, b), 0);                        // and each sector holds ITS quarter
    CHECK(memcmp(b, big + 2 * DISK_SECTOR_SIZE, DISK_SECTOR_SIZE) == 0);
    CHECK_EQ(disk_read_n(10, back, 0), 0);                // zero sectors is not an error

    TEST_SECTION("a run that leaves the disk");
    pattern(big, sizeof(big), 90);
    memset(back, 0x5A, sizeof(back));
    CHECK_EQ(disk_read_n(62, back, 4), -1);               // 62 and 63 are read, 64 is not
    CHECK_EQ(disk_read(62, b), 0);
    CHECK(memcmp(back, b, DISK_SECTOR_SIZE) == 0);
    CHECK(back[2 * DISK_SECTOR_SIZE] == 0x5A);            // the third sector of the buffer was never filled
    CHECK_EQ(disk_write_n(63, big, 2), -1);               // stops at the first error ...
    CHECK_EQ(disk_read(63, b), 0);
    CHECK(memcmp(big, b, DISK_SECTOR_SIZE) == 0);         // ... after the sector that did fit

    TEST_SECTION("flush");
    CHECK_EQ(disk_flush(), 0);                            // nothing behind it, and that is fine
    CHECK_EQ(disk_read(0, b), 0);
    pattern(a, sizeof(a), 1);
    CHECK(memcmp(a, b, sizeof(a)) == 0);
    return test_summary();
}
