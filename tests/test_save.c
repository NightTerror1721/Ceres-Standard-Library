// Saved games (ceres/save.h): the two copies taking turns, the newest good one read back, and what a save torn
// half way through or a flipped bit leaves - the save before it. On the disk (CeresFS) and on the host.
#include "ceres/test.h"
#include "ceres/save.h"
#include "ceres/fs.h"
#include "ceres/hash.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "errno.h"

struct progress
{
    int level, lives, score;
    char name[12];
};

// The bytes of a file, or -1.
static long file_bytes(const char* path, unsigned char* out, size_t cap)
{
    FILE* f = fopen(path, "rb");
    if (f == 0)
        return -1;
    long n = (long)fread(out, 1, cap, f);
    fclose(f);
    return n;
}

static int file_put(const char* path, const unsigned char* data, size_t n)
{
    FILE* f = fopen(path, "wb");
    if (f == 0)
        return -1;
    int ok = fwrite(data, 1, n, f) == n;
    return fclose(f) == 0 && ok ? 0 : -1;
}

static void run(const char* where, const char* a, const char* b)
{
    printf("-- on %s\n", where);
    struct progress me = { 3, 5, 1200, "ana" };
    struct progress got;
    unsigned int version = 0;
    CHECK_EQ(save_exists(where), 0);
    errno = 0;
    CHECK_EQ((int)save_read(where, &version, &got, sizeof got), -1);
    CHECK_EQ(errno, ENOENT);

    CHECK_EQ(save_write(where, 7, &me, sizeof me), 0);          // the first goes to .a
    CHECK_EQ(save_exists(where), 1);
    CHECK_EQ((int)save_size(where), (int)sizeof me);
    memset(&got, 0, sizeof got);
    CHECK_EQ((int)save_read(where, &version, &got, sizeof got), (int)sizeof got);
    CHECK(version == 7 && memcmp(&got, &me, sizeof me) == 0);

    me.level = 4;
    CHECK_EQ(save_write(where, 7, &me, sizeof me), 0);          // the next to .b, leaving .a as it was
    unsigned char bytes[128];
    long bn = file_bytes(b, bytes, sizeof bytes);
    CHECK_EQ((int)bn, 24 + (int)sizeof me);
    CHECK_EQ((int)save_read(where, &version, &got, sizeof got), (int)sizeof got);
    CHECK_EQ(got.level, 4);

    me.level = 5;
    CHECK_EQ(save_write(where, 8, &me, sizeof me), 0);          // over .a again
    CHECK_EQ((int)save_read(where, &version, &got, sizeof got), (int)sizeof got);
    CHECK(got.level == 5 && version == 8);

    // The machine stops half way through the next save, over .b: .a (level 5) is still the save.
    unsigned char a_bytes[128];
    long an = file_bytes(a, a_bytes, sizeof a_bytes);
    CHECK_EQ(file_put(b, bytes, (size_t)bn - 10), 0);           // .b cut short
    CHECK_EQ((int)save_read(where, &version, &got, sizeof got), (int)sizeof got);
    CHECK_EQ(got.level, 5);
    bytes[30] ^= 0x40;                                           // .b whole, one bit of its data flipped
    CHECK_EQ(file_put(b, bytes, (size_t)bn), 0);
    CHECK_EQ((int)save_read(where, &version, &got, sizeof got), (int)sizeof got);
    CHECK_EQ(got.level, 5);
    me.level = 6;
    CHECK_EQ(save_write(where, 8, &me, sizeof me), 0);          // a broken .b is the one written over
    CHECK_EQ((int)save_read(where, &version, &got, sizeof got), (int)sizeof got);
    CHECK_EQ(got.level, 6);
    CHECK(file_bytes(a, bytes, sizeof bytes) == an && memcmp(bytes, a_bytes, (size_t)an) == 0);   // .a untouched

    errno = 0;
    CHECK_EQ((int)save_read(where, &version, &got, 8), -1);      // no room
    CHECK_EQ(errno, ENOSPC);
    size_t size = 0;
    struct progress* loaded = save_load(where, &version, &size);
    CHECK(loaded != 0 && size == sizeof me && loaded->level == 6 && strcmp(loaded->name, "ana") == 0);
    free(loaded);

    a_bytes[5] ^= 1;                                             // .a's header broken too: .b is all there is
    CHECK_EQ(file_put(a, a_bytes, (size_t)an), 0);
    CHECK_EQ((int)save_read(where, &version, &got, sizeof got), (int)sizeof got);
    CHECK_EQ(got.level, 6);
    CHECK_EQ(save_write(where, 1, "", 0), 0);                    // an empty save is a save
    CHECK_EQ((int)save_size(where), 0);
    CHECK_EQ(save_erase(where), 0);
    CHECK_EQ(save_exists(where), 0);
    CHECK(file_bytes(a, bytes, sizeof bytes) < 0 && file_bytes(b, bytes, sizeof bytes) < 0);
}

int main(void)
{
    CHECK_EQ(fs_format(), 0);
    run("slot1", "slot1.a", "slot1.b");
    run("host:slot2", "host:slot2.a", "host:slot2.b");
    errno = 0;
    CHECK_EQ(save_write("a-name-far-too-long-for-any-save-file-to-have-at-all-0123456789", 1, "x", 1), -1);
    CHECK_EQ(errno, ENAMETOOLONG);
    // The sequence number wraps round: a copy numbered 0 is newer than one numbered 0xFFFFFFFF.
    CHECK_EQ(save_write("host:wrap", 1, "old", 3), 0);
    CHECK_EQ(save_write("host:wrap", 1, "new", 3), 0);
    unsigned char head[32];
    for (int which = 0; which < 2; which++)
    {
        const char* name = which == 0 ? "host:wrap.a" : "host:wrap.b";
        long n = file_bytes(name, head, sizeof head);
        unsigned int seq = which == 0 ? 0xFFFFFFFFu : 0u;
        for (int i = 0; i < 4; i++)
            head[8 + i] = (unsigned char)(seq >> (8 * i));
        unsigned int crc = hash_crc32(head, 20);
        for (int i = 0; i < 4; i++)
            head[20 + i] = (unsigned char)(crc >> (8 * i));
        CHECK_EQ(file_put(name, head, (size_t)n), 0);
    }
    char text[4] = { 0 };
    CHECK_EQ((int)save_read("host:wrap", 0, text, 3), 3);
    CHECK(memcmp(text, "new", 3) == 0);
    CHECK_EQ(save_erase("host:wrap"), 0);
    return test_summary();
}
