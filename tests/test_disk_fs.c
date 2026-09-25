// CeresFS version 1 (the flat layout; version 2 is test_fs_dirs) and the files built on it. The disk is the default one: 64 sectors, in memory, all zeros - so the
// file system starts unformatted, has 60 data clusters (30720 bytes) and room for 32 files.
#include "ceres/test.h"
#include "ceres/fs.h"
#include "ceres/disk.h"
#include "ceres/heap.h"
#include "ceres/rand.h"
#include "errno.h"
#include "stdlib.h"

#define TOTAL 30720u

static unsigned char wbuf[3200];
static unsigned char rbuf[3200];

static void fill(unsigned char* b, unsigned int n, unsigned int seed)
{
    for (unsigned int i = 0; i < n; i++)
        b[i] = (unsigned char)(seed * 31u + i * 7u + (i >> 8));
}

static int same(const unsigned char* a, const unsigned char* b, unsigned int n)
{
    return memcmp(a, b, n) == 0;
}

// Writes n bytes of pattern `seed` as a whole file; returns 0 when all of it was written.
static int make(const char* name, unsigned int n, unsigned int seed)
{
    int fd = fs_open(name, FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC);
    if (fd < 0) return -1;
    fill(wbuf, n, seed);
    int w = fs_write(fd, wbuf, n);
    fs_close(fd);
    return w == (int)n ? 0 : -1;
}

// 1 when the file holds exactly n bytes of pattern `seed`.
static int holds(const char* name, unsigned int n, unsigned int seed)
{
    int fd = fs_open(name, FS_O_RDONLY);
    if (fd < 0) return 0;
    int size = fs_size(fd);
    memset(rbuf, 0x55, sizeof rbuf);
    int r = fs_read(fd, rbuf, sizeof rbuf);
    int more = fs_read(fd, rbuf, 1);
    fs_close(fd);
    fill(wbuf, n, seed);
    return size == (int)n && r == (int)n && more == 0 && same(rbuf, wbuf, n);
}

static unsigned int clusters_for(unsigned int n) { return (n + 511u) / 512u; }

static void unformatted(void)
{
    TEST_SECTION("an unformatted disk");
    CHECK_EQ(fs_mounted(), 0);
    CHECK_EQ(fs_mount(), -1);
    CHECK_EQ(errno, ENODEV);
    CHECK_EQ((int)fs_total_bytes(), 0);
    CHECK_EQ((int)fs_free_bytes(), 0);
    CHECK_EQ(fs_open("x", FS_O_RDONLY), -1);
    CHECK_EQ(errno, ENODEV);
    CHECK_EQ(fs_remove("x"), -1);
    CHECK_EQ(fs_list(0, 0), -1);
    CHECK(fopen("x", "r") == 0);
    CHECK_EQ(errno, ENODEV);
    CHECK_EQ(fs_unmount(), 0);                          // nothing to do

    TEST_SECTION("format");
    CHECK_EQ(fs_format_version(1), 0);
    CHECK_EQ(fs_mounted(), 1);
    CHECK_EQ((int)fs_total_bytes(), (int)TOTAL);
    CHECK_EQ((int)fs_free_bytes(), (int)TOTAL);
    CHECK_EQ(fs_mount(), 0);                            // and a formatted disk mounts again
    CHECK_EQ((int)fs_free_bytes(), (int)TOTAL);
}

static int visit_count;
static int lister(const struct fs_dirent* d, void* ctx)
{
    visit_count++;
    return 0;
}

static void basic(void)
{
    TEST_SECTION("an empty file system");
    CHECK_EQ(fs_list(lister, 0), 0);

    TEST_SECTION("create, write, read");
    int fd = fs_open("hello.txt", FS_O_WRONLY | FS_O_CREAT);
    CHECK(fd >= 0);
    CHECK_EQ(fs_write(fd, "Hello, CeresFS!", 15), 15);
    CHECK_EQ(fs_tell(fd), 15);
    CHECK_EQ(fs_size(fd), 15);
    CHECK_EQ(fs_close(fd), 0);
    struct fs_stat st;
    CHECK_EQ(fs_stat("hello.txt", &st), 0);
    CHECK_EQ((int)st.size, 15);
    CHECK(st.first_sector >= 4);                        // in the data area: after the superblock, FAT and directory
    CHECK_EQ((int)fs_free_bytes(), (int)TOTAL - 512);   // a byte costs a whole sector

    fd = fs_open("hello.txt", FS_O_RDONLY);
    char text[32];
    memset(text, 0, sizeof text);
    CHECK_EQ(fs_read(fd, text, 100), 15);               // asked for more than there is
    CHECK_STR(text, "Hello, CeresFS!");
    CHECK_EQ(fs_read(fd, text, 10), 0);                 // the end
    CHECK_EQ(fs_seek(fd, 7, FS_SEEK_SET), 0);
    memset(text, 0, sizeof text);
    CHECK_EQ(fs_read(fd, text, 4), 4);
    CHECK_STR(text, "Cere");
    CHECK_EQ(fs_seek(fd, -3, FS_SEEK_END), 0);
    memset(text, 0, sizeof text);
    CHECK_EQ(fs_read(fd, text, 10), 3);
    CHECK_STR(text, "FS!");
    CHECK_EQ(fs_seek(fd, -5, FS_SEEK_CUR), 0);
    CHECK_EQ(fs_tell(fd), 10);
    CHECK_EQ(fs_seek(fd, 16, FS_SEEK_SET), -1);         // past the end: there are no sparse files
    CHECK_EQ(errno, EINVAL);
    CHECK_EQ(fs_seek(fd, -1, FS_SEEK_SET), -1);
    CHECK_EQ(fs_seek(fd, 0, 99), -1);
    CHECK_EQ(fs_tell(fd), 10);                          // a refused seek moved nothing
    CHECK_EQ(fs_seek(fd, 15, FS_SEEK_SET), 0);          // exactly the end is fine
    CHECK_EQ(fs_close(fd), 0);

    TEST_SECTION("errors");
    CHECK_EQ(fs_open("missing", FS_O_RDONLY), -1);
    CHECK_EQ(errno, ENOENT);
    CHECK_EQ(fs_open("", FS_O_RDONLY | FS_O_CREAT), -1);
    CHECK_EQ(errno, EINVAL);
    CHECK_EQ(fs_open(0, FS_O_RDONLY), -1);
    CHECK_EQ(fs_open("123456789012345678901234", FS_O_RDWR | FS_O_CREAT), -1);       // 24 characters
    CHECK_EQ(errno, ENAMETOOLONG);
    fd = fs_open("12345678901234567890123", FS_O_RDWR | FS_O_CREAT);                 // 23 is the longest
    CHECK(fd >= 0);
    fs_close(fd);
    CHECK_EQ(fs_remove("12345678901234567890123"), 0);
    fd = fs_open("dir/looks/like/a/path", FS_O_RDWR | FS_O_CREAT);                   // '/' is a character like another
    CHECK(fd >= 0);
    fs_close(fd);
    CHECK_EQ(fs_stat("dir/looks/like/a/path", &st), 0);
    CHECK_EQ(fs_remove("dir/looks/like/a/path"), 0);
    CHECK_EQ(fs_open("hello.txt", 0), -1);              // no access asked for
    CHECK_EQ(errno, EINVAL);
    CHECK_EQ(fs_open("hello.txt", FS_O_RDONLY | FS_O_TRUNC), -1);     // truncating a file you cannot write
    CHECK_EQ(fs_open("hello.txt", FS_O_RDONLY | FS_O_APPEND), -1);
    CHECK_EQ(fs_close(99), -1);
    CHECK_EQ(errno, EBADF);
    CHECK_EQ(fs_close(-1), -1);
    CHECK_EQ(fs_read(3, text, 1), -1);
    CHECK_EQ(errno, EBADF);
    fd = fs_open("hello.txt", FS_O_RDONLY);
    CHECK_EQ(fs_write(fd, "x", 1), -1);                 // a read-only descriptor
    CHECK_EQ(errno, EBADF);
    fs_close(fd);
    CHECK_EQ(fs_close(fd), -1);                         // closed twice
    fd = fs_open("hello.txt", FS_O_WRONLY);
    CHECK_EQ(fs_read(fd, text, 1), -1);                 // a write-only one
    CHECK_EQ(errno, EBADF);
    fs_close(fd);
    CHECK(strcmp(strerror(ENODEV), "No such device") == 0);

    TEST_SECTION("the name is case sensitive and exact");
    CHECK_EQ(fs_stat("HELLO.TXT", &st), -1);
    CHECK_EQ(fs_stat("hello", &st), -1);
    CHECK_EQ(fs_stat("hello.txt ", &st), -1);
    CHECK_EQ(fs_remove("hello.txt"), 0);
    CHECK_EQ((int)fs_free_bytes(), (int)TOTAL);
}

static void multi_sector(void)
{
    TEST_SECTION("a file over several sectors");
    int fd = fs_open("data", FS_O_WRONLY | FS_O_CREAT);
    fill(wbuf, 1300, 1);
    unsigned int at = 0;
    int all = 1;
    while (at < 1300)                                   // in pieces of 100, ending short
    {
        unsigned int n = 1300 - at < 100 ? 1300 - at : 100;
        if (fs_write(fd, wbuf + at, n) != (int)n) all = 0;
        at += n;
    }
    CHECK(all);
    CHECK_EQ(fs_size(fd), 1300);
    fs_close(fd);
    CHECK(holds("data", 1300, 1));
    CHECK_EQ((int)fs_free_bytes(), (int)TOTAL - 3 * 512);

    fd = fs_open("data", FS_O_RDONLY);
    int chunks = 1;
    fill(wbuf, 1300, 1);
    for (unsigned int i = 0; i < 1300; i += 37)         // the reads straddle every sector boundary
    {
        unsigned int n = 1300 - i < 37 ? 1300 - i : 37;
        if (fs_read(fd, rbuf, n) != (int)n || !same(rbuf, wbuf + i, n)) chunks = 0;
    }
    CHECK(chunks);
    CHECK_EQ(fs_seek(fd, 500, FS_SEEK_SET), 0);         // 500..549 crosses the first boundary
    CHECK_EQ(fs_read(fd, rbuf, 50), 50);
    CHECK(same(rbuf, wbuf + 500, 50));
    CHECK_EQ(fs_seek(fd, 100, FS_SEEK_SET), 0);         // going back works too
    CHECK_EQ(fs_read(fd, rbuf, 20), 20);
    CHECK(same(rbuf, wbuf + 100, 20));
    fs_close(fd);

    TEST_SECTION("overwrite in the middle, then grow");
    fd = fs_open("data", FS_O_RDWR);
    CHECK_EQ(fs_seek(fd, 510, FS_SEEK_SET), 0);
    CHECK_EQ(fs_write(fd, "XXXXXX", 6), 6);             // across a sector boundary
    CHECK_EQ(fs_size(fd), 1300);                        // still the same length
    CHECK_EQ(fs_seek(fd, 508, FS_SEEK_SET), 0);
    CHECK_EQ(fs_read(fd, rbuf, 10), 10);                // read back on the same descriptor
    CHECK(memcmp(rbuf + 2, "XXXXXX", 6) == 0);
    CHECK(rbuf[0] == wbuf[508] && rbuf[1] == wbuf[509] && rbuf[8] == wbuf[516]);
    CHECK_EQ(fs_seek(fd, 0, FS_SEEK_END), 0);
    CHECK_EQ(fs_write(fd, "tail", 4), 4);
    CHECK_EQ(fs_size(fd), 1304);
    fs_close(fd);
    fd = fs_open("data", FS_O_RDONLY);
    CHECK_EQ(fs_seek(fd, 1298, FS_SEEK_SET), 0);
    memset(rbuf, 0, 16);
    CHECK_EQ(fs_read(fd, rbuf, 16), 6);
    CHECK(rbuf[0] == wbuf[1298] && rbuf[1] == wbuf[1299] && memcmp(rbuf + 2, "tail", 4) == 0);
    fs_close(fd);
    CHECK_EQ(fs_remove("data"), 0);
    CHECK_EQ((int)fs_free_bytes(), (int)TOTAL);

    TEST_SECTION("exactly a whole sector, and two");
    CHECK_EQ(make("s512", 512, 2), 0);
    CHECK(holds("s512", 512, 2));
    CHECK_EQ((int)fs_free_bytes(), (int)TOTAL - 512);
    CHECK_EQ(make("s1024", 1024, 3), 0);
    CHECK(holds("s1024", 1024, 3));
    CHECK_EQ((int)fs_free_bytes(), (int)TOTAL - 512 - 1024);
    CHECK_EQ(make("s513", 513, 4), 0);                  // one byte into a second sector
    CHECK(holds("s513", 513, 4));
    CHECK_EQ(make("s0", 0, 5), 0);                      // an empty file takes no data at all
    CHECK(holds("s0", 0, 5));
    struct fs_stat st;
    CHECK_EQ(fs_stat("s0", &st), 0);
    CHECK_EQ((int)st.first_sector, 0);
    CHECK_EQ((int)fs_free_bytes(), (int)TOTAL - 512 - 1024 - 1024);
    fs_remove("s512"); fs_remove("s1024"); fs_remove("s513"); fs_remove("s0");
}

static void modes(void)
{
    TEST_SECTION("append");
    CHECK_EQ(make("log", 0, 0), 0);
    int fd = fs_open("log", FS_O_WRONLY);
    fs_write(fd, "ab", 2);
    fs_close(fd);
    fd = fs_open("log", FS_O_WRONLY | FS_O_APPEND);
    CHECK_EQ(fs_write(fd, "cd", 2), 2);                 // at the end though the position starts at 0
    CHECK_EQ(fs_seek(fd, 0, FS_SEEK_SET), 0);
    CHECK_EQ(fs_write(fd, "ef", 2), 2);                 // and again after a seek
    fs_close(fd);
    fd = fs_open("log", FS_O_RDONLY);
    memset(rbuf, 0, 16);
    CHECK_EQ(fs_read(fd, rbuf, 16), 6);
    CHECK(memcmp(rbuf, "abcdef", 6) == 0);
    fs_close(fd);

    TEST_SECTION("truncate gives the space back");
    CHECK_EQ(make("t", 2000, 6), 0);
    unsigned int used = fs_free_bytes();
    fd = fs_open("t", FS_O_WRONLY | FS_O_TRUNC);
    CHECK_EQ(fs_size(fd), 0);
    CHECK_EQ((int)fs_free_bytes(), (int)used + 4 * 512);
    CHECK_EQ(fs_write(fd, "new", 3), 3);
    fs_close(fd);
    CHECK(holds("t", 3, 6) == 0);                       // the pattern is gone: it now says "new"
    fd = fs_open("t", FS_O_RDONLY);
    memset(rbuf, 0, 8);
    CHECK_EQ(fs_read(fd, rbuf, 8), 3);
    CHECK(memcmp(rbuf, "new", 3) == 0);
    fs_close(fd);
    fs_remove("t");
    fs_remove("log");

    TEST_SECTION("one writer or any number of readers");
    CHECK_EQ(make("shared", 100, 7), 0);
    int r1 = fs_open("shared", FS_O_RDONLY);
    int r2 = fs_open("shared", FS_O_RDONLY);
    CHECK(r1 >= 0 && r2 >= 0 && r1 != r2);
    CHECK_EQ(fs_open("shared", FS_O_WRONLY), -1);       // a writer while it is being read
    CHECK_EQ(errno, EBUSY);
    CHECK_EQ(fs_remove("shared"), -1);
    CHECK_EQ(errno, EBUSY);
    fs_close(r1);
    fs_close(r2);
    int w = fs_open("shared", FS_O_WRONLY);
    CHECK(w >= 0);
    CHECK_EQ(fs_open("shared", FS_O_RDONLY), -1);       // a reader while it is being written
    CHECK_EQ(errno, EBUSY);
    CHECK_EQ(fs_open("shared", FS_O_WRONLY), -1);
    fs_close(w);
    CHECK_EQ(fs_remove("shared"), 0);

    TEST_SECTION("eight files at a time");
    char name[8];
    int fds[9];
    int opened = 0;
    for (int i = 0; i < 8; i++)
    {
        name[0] = 'f'; name[1] = (char)('0' + i); name[2] = 0;
        fds[i] = fs_open(name, FS_O_RDWR | FS_O_CREAT);
        if (fds[i] >= 0) opened++;
    }
    CHECK_EQ(opened, 8);
    CHECK_EQ(fs_open("ninth", FS_O_RDWR | FS_O_CREAT), -1);
    CHECK_EQ(errno, EMFILE);
    struct fs_stat none;
    CHECK_EQ(fs_stat("ninth", &none), -1);                        // and it was not created
    for (int i = 0; i < 8; i++)
    {
        fs_close(fds[i]);
        name[0] = 'f'; name[1] = (char)('0' + i); name[2] = 0;
        fs_remove(name);
    }
}

static const char* seen_names[8];
static unsigned int seen_sizes[8];
static int seen_n;
static char name_store[8][24];
static int recorder(const struct fs_dirent* d, void* ctx)
{
    if (seen_n < 8)
    {
        strcpy(name_store[seen_n], d->name);
        seen_names[seen_n] = name_store[seen_n];
        seen_sizes[seen_n] = d->size;
        seen_n++;
    }
    return ctx != 0 && seen_n >= *(int*)ctx;
}

static void naming(void)
{
    TEST_SECTION("rename");
    CHECK_EQ(make("old", 700, 8), 0);
    CHECK_EQ(fs_rename("old", "new"), 0);
    struct fs_stat st;
    CHECK_EQ(fs_stat("old", &st), -1);
    CHECK(holds("new", 700, 8));
    CHECK_EQ(make("other", 10, 9), 0);
    CHECK_EQ(fs_rename("new", "other"), -1);            // the name is taken
    CHECK_EQ(errno, EEXIST);
    CHECK(holds("new", 700, 8) && holds("other", 10, 9));
    CHECK_EQ(fs_rename("new", "new"), 0);               // to itself: nothing
    CHECK_EQ(fs_rename("nothing", "x"), -1);
    CHECK_EQ(errno, ENOENT);
    CHECK_EQ(fs_rename("new", ""), -1);
    CHECK_EQ(fs_rename("new", "123456789012345678901234"), -1);
    CHECK_EQ(errno, ENAMETOOLONG);
    CHECK_EQ(fs_rename("new", "renamed to something longer"), -1);

    TEST_SECTION("listing");
    seen_n = 0;
    int total = fs_list(recorder, 0);
    CHECK_EQ(total, 2);
    CHECK_EQ(seen_n, 2);
    int found_new = 0, found_other = 0;
    for (int i = 0; i < seen_n; i++)
    {
        if (strcmp(seen_names[i], "new") == 0 && seen_sizes[i] == 700) found_new = 1;
        if (strcmp(seen_names[i], "other") == 0 && seen_sizes[i] == 10) found_other = 1;
    }
    CHECK(found_new && found_other);
    seen_n = 0;
    int stop_at = 1;
    CHECK_EQ(fs_list(recorder, &stop_at), 1);           // the visitor said stop after the first
    CHECK_EQ(seen_n, 1);
    CHECK_EQ(fs_remove("new"), 0);
    CHECK_EQ(fs_remove("other"), 0);
    CHECK_EQ(fs_remove("new"), -1);
    CHECK_EQ(errno, ENOENT);
    CHECK_EQ(fs_remove(""), -1);
    visit_count = 0;
    CHECK_EQ(fs_list(lister, 0), 0);
    CHECK_EQ(visit_count, 0);
}

static void full_disk(void)
{
    TEST_SECTION("the disk fills up");
    CHECK_EQ(make("keep", 1500, 10), 0);                // 3 clusters that must survive it all
    unsigned int room = fs_free_bytes();
    CHECK_EQ((int)room, (int)TOTAL - 3 * 512);
    int fd = fs_open("big", FS_O_WRONLY | FS_O_CREAT);
    fill(wbuf, 3200, 11);
    unsigned int written = 0;
    for (;;)
    {
        int w = fs_write(fd, wbuf, 3200);
        if (w <= 0) break;
        written += (unsigned int)w;
        if (w < 3200) break;
    }
    CHECK_EQ((int)written, (int)room);                  // every free byte, and the last write was a short one
    CHECK_EQ((int)fs_free_bytes(), 0);
    CHECK_EQ(fs_write(fd, wbuf, 10), -1);
    CHECK_EQ(errno, ENOSPC);
    CHECK_EQ(fs_size(fd), (int)room);
    CHECK_EQ(fs_close(fd), 0);
    CHECK(holds("keep", 1500, 10));                     // the other file is untouched
    int new_fd = fs_open("empty", FS_O_WRONLY | FS_O_CREAT);    // an empty file still fits in the directory
    CHECK(new_fd >= 0);
    CHECK_EQ(fs_write(new_fd, "x", 1), -1);
    CHECK_EQ(errno, ENOSPC);
    fs_close(new_fd);
    fs_remove("empty");

    TEST_SECTION("what was written to a full disk reads back");
    fd = fs_open("big", FS_O_RDONLY);
    unsigned int at = 0;
    int good = 1;
    fill(wbuf, 3200, 11);
    while (at < room)
    {
        unsigned int n = room - at < 3200 ? room - at : 3200;
        if (fs_read(fd, rbuf, n) != (int)n) { good = 0; break; }
        if (!same(rbuf, wbuf, n)) good = 0;
        at += n;                                        // every 3200-byte block was written from the same pattern
    }
    fs_close(fd);
    CHECK(good);
    CHECK_EQ(fs_remove("big"), 0);
    CHECK_EQ((int)fs_free_bytes(), (int)room);
    CHECK_EQ(fs_remove("keep"), 0);
    CHECK_EQ((int)fs_free_bytes(), (int)TOTAL);

    TEST_SECTION("the directory fills up");
    char name[8];
    int created = 0;
    for (int i = 0; i < 40; i++)
    {
        name[0] = 'd'; name[1] = (char)('a' + i / 10); name[2] = (char)('0' + i % 10); name[3] = 0;
        int f = fs_open(name, FS_O_WRONLY | FS_O_CREAT);
        if (f < 0)
        {
            CHECK_EQ(errno, ENOSPC);
            break;
        }
        fs_close(f);
        created++;
    }
    CHECK_EQ(created, 32);                              // two directory sectors of sixteen
    visit_count = 0;
    CHECK_EQ(fs_list(lister, 0), 32);
    for (int i = 0; i < 32; i++)
    {
        name[0] = 'd'; name[1] = (char)('a' + i / 10); name[2] = (char)('0' + i % 10); name[3] = 0;
        CHECK_EQ(fs_remove(name), 0);
    }
    CHECK_EQ(make("again", 5, 12), 0);                  // room in the directory again
    fs_remove("again");
}

static void persistence(void)
{
    TEST_SECTION("unmount and mount again");
    CHECK_EQ(make("p1", 2000, 13), 0);
    CHECK_EQ(make("p2", 77, 14), 0);
    int open_fd = fs_open("p2", FS_O_WRONLY | FS_O_APPEND);       // still open (and dirty) when we unmount
    CHECK_EQ(fs_write(open_fd, "++", 2), 2);
    CHECK_EQ(fs_unmount(), 0);
    CHECK_EQ(fs_mounted(), 0);
    CHECK_EQ(fs_open("p1", FS_O_RDONLY), -1);
    CHECK_EQ(errno, ENODEV);
    CHECK_EQ(fs_mount(), 0);
    CHECK(holds("p1", 2000, 13));
    struct fs_stat st;
    CHECK_EQ(fs_stat("p2", &st), 0);
    CHECK_EQ((int)st.size, 79);                         // the append made it to the disk
    CHECK_EQ((int)fs_free_bytes(), (int)TOTAL - 4 * 512 - 512);

    TEST_SECTION("the bytes really are on the disk");
    unsigned int sb[128];
    CHECK_EQ(disk_read(0, sb), 0);
    CHECK_EQ((int)sb[0], 0x31534643);                   // the superblock says CFS1
    CHECK_EQ((int)sb[2], 64);
    CHECK_EQ((int)sb[8], 60);
    CHECK_EQ(disk_read(st.first_sector, rbuf), 0);
    fill(wbuf, 77, 14);
    CHECK(same(rbuf, wbuf, 77));
    CHECK(rbuf[77] == '+' && rbuf[78] == '+');

    TEST_SECTION("formatting starts over");
    int busy = fs_open("p1", FS_O_RDONLY);
    CHECK_EQ(fs_format_version(1), -1);                          // not while something is open
    CHECK_EQ(errno, EBUSY);
    fs_close(busy);
    CHECK_EQ(fs_format_version(1), 0);
    CHECK_EQ(fs_stat("p1", &st), -1);
    CHECK_EQ((int)fs_free_bytes(), (int)TOTAL);
}

static void churn(void)
{
    TEST_SECTION("many files, some removed, others added into the gaps");
    struct rng r;
    rng_seed(&r, 4242);
    unsigned int size[16];
    char name[8];
    for (int i = 0; i < 10; i++)
    {
        size[i] = (unsigned int)rng_range(&r, 1, 3000);
        name[0] = 'c'; name[1] = (char)('a' + i); name[2] = 0;
        CHECK_EQ(make(name, size[i], 100u + (unsigned int)i), 0);
    }
    for (int i = 1; i < 10; i += 2)                      // remove the odd ones: the free clusters are now scattered
    {
        name[0] = 'c'; name[1] = (char)('a' + i); name[2] = 0;
        CHECK_EQ(fs_remove(name), 0);
        size[i] = 0;
    }
    for (int i = 10; i < 15; i++)
    {
        size[i] = (unsigned int)rng_range(&r, 1, 2500);
        name[0] = 'c'; name[1] = (char)('a' + i); name[2] = 0;
        CHECK_EQ(make(name, size[i], 100u + (unsigned int)i), 0);   // these chains run through the holes
    }
    int intact = 1;
    unsigned int clusters_used = 0;
    for (int i = 0; i < 15; i++)
    {
        if (size[i] == 0) continue;
        name[0] = 'c'; name[1] = (char)('a' + i); name[2] = 0;
        if (!holds(name, size[i], 100u + (unsigned int)i)) intact = 0;
        clusters_used += clusters_for(size[i]);
    }
    CHECK(intact);
    CHECK_EQ((int)fs_free_bytes(), (int)(TOTAL - clusters_used * 512u));
    for (int i = 0; i < 15; i++)
    {
        name[0] = 'c'; name[1] = (char)('a' + i); name[2] = 0;
        fs_remove(name);
    }
    CHECK_EQ((int)fs_free_bytes(), (int)TOTAL);
}

static void streams(void)
{
    TEST_SECTION("fopen on a missing file");
    CHECK(fopen("nothing", "r") == 0);
    CHECK_EQ(errno, ENOENT);
    CHECK(fopen("x", "q") == 0);                        // not a mode
    CHECK_EQ(errno, EINVAL);

    TEST_SECTION("write text, read it back");
    FILE* f = fopen("notes.txt", "w");
    CHECK(f != 0);
    CHECK_EQ(fprintf(f, "line %d\n", 1), 7);
    CHECK_EQ(fputs("second line\n", f) >= 0, 1);
    fputc('#', f);
    fputc('\n', f);
    CHECK_EQ(fwrite("abc", 1, 3, f), 3u);
    CHECK_EQ(ftell(f), 7 + 12 + 2 + 3);
    CHECK_EQ(fclose(f), 0);
    f = fopen("notes.txt", "r");
    char line[64];
    CHECK(fgets(line, sizeof line, f) != 0);
    CHECK_STR(line, "line 1\n");
    CHECK(fgets(line, sizeof line, f) != 0);
    CHECK_STR(line, "second line\n");
    CHECK_EQ(fgetc(f), '#');
    CHECK_EQ(feof(f), 0);
    CHECK(fgets(line, sizeof line, f) != 0);
    CHECK_STR(line, "\n");
    CHECK(fgets(line, sizeof line, f) != 0);
    CHECK_STR(line, "abc");                             // the last line has no newline
    CHECK(fgets(line, sizeof line, f) == 0);            // and now the end
    CHECK_EQ(feof(f), 1);
    CHECK_EQ(fgetc(f), EOF);
    CHECK_EQ(ferror(f), 0);
    clearerr(f);
    CHECK_EQ(feof(f), 0);
    fclose(f);

    TEST_SECTION("seek, tell, rewind, ungetc");
    f = fopen("notes.txt", "r");
    CHECK_EQ(fseek(f, 5, SEEK_SET), 0);
    CHECK_EQ(ftell(f), 5);
    CHECK_EQ(fgetc(f), '1');
    CHECK_EQ(ungetc('1', f), '1');
    CHECK_EQ(ftell(f), 5);                              // the pushed-back character is not counted
    CHECK_EQ(fgetc(f), '1');
    CHECK_EQ(fseek(f, -3, SEEK_END), 0);
    CHECK_EQ(fgetc(f), 'a');
    CHECK_EQ(fseek(f, 0, SEEK_CUR), 0);
    CHECK_EQ(ftell(f), 22);
    rewind(f);
    CHECK_EQ(ftell(f), 0);
    CHECK_EQ(fgetc(f), 'l');
    CHECK_EQ(fseek(f, 1000, SEEK_SET), -1);
    fclose(f);

    TEST_SECTION("append and update");
    f = fopen("notes.txt", "a");
    fputs("more\n", f);
    fclose(f);
    f = fopen("notes.txt", "r+");
    CHECK(f != 0);
    CHECK_EQ(fseek(f, 0, SEEK_SET), 0);
    fputs("LINE", f);                                   // overwrite the start
    CHECK_EQ(fseek(f, 0, SEEK_SET), 0);
    CHECK(fgets(line, sizeof line, f) != 0);
    CHECK_STR(line, "LINE 1\n");
    fclose(f);
    f = fopen("notes.txt", "r");
    fseek(f, -5, SEEK_END);
    CHECK(fgets(line, sizeof line, f) != 0);
    CHECK_STR(line, "more\n");
    fclose(f);
    f = fopen("notes.txt", "w");                        // "w" empties it
    fclose(f);
    struct fs_stat st;
    CHECK_EQ(fs_stat("notes.txt", &st), 0);
    CHECK_EQ((int)st.size, 0);

    TEST_SECTION("records, numbers, lines");
    struct rec { int id; float value; };
    struct rec out[5], in[5];
    for (int i = 0; i < 5; i++) { out[i].id = i * 11; out[i].value = (float)i * 0.5f; }
    f = fopen("recs.bin", "wb");
    CHECK_EQ(fwrite(out, sizeof(struct rec), 5, f), 5u);
    fclose(f);
    f = fopen("recs.bin", "rb");
    CHECK_EQ(fread(in, sizeof(struct rec), 5, f), 5u);
    CHECK_EQ(fread(in, sizeof(struct rec), 1, f), 0u);
    fclose(f);
    int rec_ok = 1;
    for (int i = 0; i < 5; i++) if (in[i].id != out[i].id || in[i].value != out[i].value) rec_ok = 0;
    CHECK(rec_ok);
    f = fopen("nums.txt", "w");
    fprintf(f, "%d %d %d\n%s\n", 10, 20, 30, "the end of it");
    fclose(f);
    f = fopen("nums.txt", "r");
    int a = 0, b = 0, c = 0;
    CHECK_EQ(fscanf(f, "%d %d %d", &a, &b, &c), 3);
    CHECK(a == 10 && b == 20 && c == 30);
    fgetc(f);                                           // the newline
    char* text = 0;
    size_t cap = 0;
    CHECK_EQ(getline(&text, &cap, f), 14);
    CHECK_STR(text, "the end of it\n");
    CHECK_EQ(getline(&text, &cap, f), -1);
    free(text);
    fclose(f);

    TEST_SECTION("remove and rename through stdio");
    CHECK_EQ(rename("nums.txt", "numbers.txt"), 0);
    CHECK(fopen("nums.txt", "r") == 0);
    f = fopen("numbers.txt", "r");
    CHECK(f != 0);
    fclose(f);
    CHECK_EQ(remove("numbers.txt"), 0);
    CHECK_EQ(remove("numbers.txt"), -1);
    CHECK_EQ(errno, ENOENT);
    CHECK_EQ(rename("missing", "x"), -1);
    remove("recs.bin");
    remove("notes.txt");

    TEST_SECTION("tmpfile and freopen");
    unsigned int before = fs_free_bytes();
    f = tmpfile();
    CHECK(f != 0);
    fputs("scratch data", f);
    CHECK_EQ(fseek(f, 0, SEEK_SET), 0);
    CHECK(fgets(line, sizeof line, f) != 0);
    CHECK_STR(line, "scratch data");
    CHECK(fs_free_bytes() < before);                    // it takes room while it exists
    FILE* g = tmpfile();
    CHECK(g != 0 && g != f);
    fclose(g);
    fclose(f);
    CHECK_EQ((int)fs_free_bytes(), (int)before);        // and gives it back
    visit_count = 0;
    CHECK_EQ(fs_list(lister, 0), 0);
    f = fopen("one", "w");
    fputs("first", f);
    f = freopen("two", "w", f);
    CHECK(f != 0);
    fputs("second", f);
    fclose(f);
    CHECK_EQ(fs_stat("one", &st), 0);
    CHECK_EQ((int)st.size, 5);
    CHECK_EQ(fs_stat("two", &st), 0);
    CHECK_EQ((int)st.size, 6);
    CHECK(freopen("nothing", "r", stdout) == 0);        // only disk streams can be reopened
    remove("one");
    remove("two");

    TEST_SECTION("streams and the file system share the limits");
    FILE* many[8];
    int got = 0;
    for (int i = 0; i < 9; i++)
    {
        char n[8];
        n[0] = 's'; n[1] = (char)('0' + i); n[2] = 0;
        FILE* s = fopen(n, "w");
        if (s == 0) { CHECK_EQ(errno, EMFILE); break; }
        many[got++] = s;
    }
    CHECK_EQ(got, 8);
    for (int i = 0; i < 8; i++)
    {
        fclose(many[i]);
        char n[8];
        n[0] = 's'; n[1] = (char)('0' + i); n[2] = 0;
        remove(n);
    }
    f = fopen("busy", "w");
    CHECK(fopen("busy", "r") == 0);
    CHECK_EQ(errno, EBUSY);
    fflush(f);
    fclose(f);
    remove("busy");
    CHECK_EQ(fclose(0), EOF);
}

int main(void)
{
    unsigned int heap_before = heap_used();
    unformatted();
    basic();
    multi_sector();
    modes();
    naming();
    full_disk();
    persistence();
    churn();
    streams();

    TEST_SECTION("nothing left behind");
    CHECK_EQ((int)fs_free_bytes(), (int)TOTAL);
    visit_count = 0;
    CHECK_EQ(fs_list(lister, 0), 0);
    CHECK_EQ((int)heap_used(), (int)heap_before);       // every FILE was freed
    return test_summary();
}
