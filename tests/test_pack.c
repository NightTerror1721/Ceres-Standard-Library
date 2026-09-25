// Resource packs (ceres/pack.h): one built here in memory (stored and compressed entries), the same written to a
// host file and opened from there, and tests/data/assets.cart - built by tools/mkpack.js, with its own LZ4
// compressor - plugged into port 3 as a cartridge and read a sector at a time. Broken packs and entries refused.
#include "ceres/test.h"
#include "ceres/pack.h"
#include "ceres/image.h"
#include "ceres/lz.h"
#include "ceres/hash.h"
#include "ceres/color.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
#include "errno.h"

static unsigned char pack[2048];
static unsigned int used;

static void put32(unsigned char* p, unsigned int v)
{
    for (int i = 0; i < 4; i++)
        p[i] = (unsigned char)(v >> (8 * i));
}

// A pack of these entries, compressing each when that helps.
static void build(const char* const* names, const char* const* texts, int count)
{
    memset(pack, 0, sizeof pack);
    memcpy(pack, "CPAK", 4);
    pack[4] = 1;
    pack[6] = (unsigned char)count;
    put32(pack + 8, 16);
    used = 16 + 48u * (unsigned int)count;
    for (int i = 0; i < count; i++)
    {
        unsigned char* e = pack + 16 + 48 * i;
        size_t size = strlen(texts[i]);
        strcpy((char*)e, names[i]);
        put32(e + 32, used);
        put32(e + 36, (unsigned int)size);
        put32(e + 44, hash_crc32(texts[i], size));
        int packed = lz4_compress(texts[i], size, pack + used, sizeof pack - used);
        if (packed < 0 || (size_t)packed >= size)
        {
            memcpy(pack + used, texts[i], size);
            packed = (int)size;
        }
        put32(e + 40, (unsigned int)packed);
        used += (unsigned int)packed;
    }
}

int main(void)
{
    static const char* const names[] = { "hello.txt", "levels/1.txt", "empty" };
    static const char* const texts[] = {
        "hi",
        "##########\n#........#\n#........#\n#........#\n#........#\n##########\n",
        "" };
    build(names, texts, 3);

    TEST_SECTION("in memory");
    struct pack p;
    CHECK_EQ(pack_open_memory(&p, pack, used), 0);
    CHECK_EQ(pack_count(&p), 3);
    CHECK_STR(pack_entry_at(&p, 1)->name, "levels/1.txt");
    CHECK(pack_entry_at(&p, 3) == 0);
    const struct pack_entry* level = pack_find(&p, "levels/1.txt");
    CHECK(level != 0 && level->stored < level->size);            // compressed
    CHECK(pack_find(&p, "hello.txt")->stored == 2);              // too short to gain: stored
    char text[128];
    CHECK_EQ((int)pack_read(&p, level, text, sizeof text), (int)strlen(texts[1]));
    CHECK(memcmp(text, texts[1], strlen(texts[1])) == 0);
    errno = 0;
    CHECK_EQ((int)pack_read(&p, level, text, 10), -1);
    CHECK_EQ(errno, ENOSPC);
    size_t size = 99;
    char* hello = pack_load(&p, "hello.txt", &size);
    CHECK(hello != 0 && size == 2);
    CHECK_STR(hello, "hi");                                      // with its NUL
    free(hello);
    char* nothing = pack_load(&p, "empty", &size);
    CHECK(nothing != 0 && size == 0 && nothing[0] == 0);
    free(nothing);
    errno = 0;
    CHECK(pack_load(&p, "missing", &size) == 0);
    CHECK_EQ(errno, ENOENT);
    pack_close(&p);

    TEST_SECTION("broken");
    pack[16 + 48 + 44] ^= 1;                                     // the level's CRC
    CHECK_EQ(pack_open_memory(&p, pack, used), 0);
    errno = 0;
    CHECK_EQ((int)pack_read(&p, pack_find(&p, "levels/1.txt"), text, sizeof text), -1);
    CHECK_EQ(errno, EIO);
    pack_close(&p);
    pack[16 + 48 + 44] ^= 1;
    put32(pack + 16 + 32, 5000);                                 // hello.txt's data far past the end
    CHECK_EQ(pack_open_memory(&p, pack, used), 0);
    errno = 0;
    CHECK_EQ((int)pack_read(&p, pack_find(&p, "hello.txt"), text, sizeof text), -1);
    CHECK_EQ(errno, EINVAL);
    pack_close(&p);
    errno = 0;
    CHECK_EQ(pack_open_memory(&p, pack, 30), -1);                // the directory cut short
    CHECK_EQ(errno, EINVAL);
    CHECK_EQ(pack_open_memory(&p, "CPAQ", 4), -1);                 // too short
    pack[0] = 'Q';
    errno = EIO;                                                 // a stale errno must not leak through
    CHECK_EQ(pack_open_memory(&p, pack, used), -1);              // long enough, but not a pack
    CHECK_EQ(errno, EINVAL);
    pack[0] = 'C';
    put32(pack + 8, 0xFFFFFFF0u);                                // a directory near the top of the address range
    CHECK_EQ(pack_open_memory(&p, pack, used), -1);
    CHECK_EQ(errno, EINVAL);
    build(names, texts, 3);

    TEST_SECTION("in a file");
    FILE* f = fopen("host:test.pack", "wb");
    CHECK(f != 0 && fwrite(pack, 1, used, f) == used);
    fclose(f);
    CHECK_EQ(pack_open_file(&p, "host:test.pack"), 0);
    CHECK_EQ((int)pack_read(&p, pack_find(&p, "levels/1.txt"), text, sizeof text), (int)strlen(texts[1]));
    CHECK(memcmp(text, texts[1], strlen(texts[1])) == 0);
    pack_close(&p);
    errno = 0;
    CHECK_EQ(pack_open_file(&p, "host:nope.pack"), -1);
    CHECK_EQ(errno, ENOENT);

    TEST_SECTION("a cartridge made by mkpack.js");
    CHECK_EQ(pack_open_port(&p, 3), 0);
    CHECK_EQ(pack_count(&p), 4);
    for (int i = 0; i < pack_count(&p); i++)
    {
        const struct pack_entry* e = pack_entry_at(&p, i);
        printf("%-26s %5u %5u\n", e->name, e->size, e->stored);
    }
    char* story = pack_load(&p, "pack/story.txt", &size);
    CHECK(story != 0 && size == 2831);                           // 271 bytes on the cartridge, over a sector
    CHECK(strncmp(story, "Line 1: the quick brown fox", 27) == 0);
    CHECK(strstr(story, "Line 40: the quick brown fox") != 0);
    free(story);
    struct gfx_surface* s = pack_image(&p, "host/images/dotnet24.bmp", RGB(255, 0, 255));
    CHECK(s != 0 && s->w == 5 && s->h == 3);
    CHECK(s != 0 && s->px[2 * 5 + 4] == RGB(200, 200, 13));
    image_free(s);
    pack_close(&p);
    errno = 0;
    CHECK_EQ(pack_open_port(&p, 1), -1);                         // nothing plugged in there
    CHECK_EQ(errno, ENODEV);
    CHECK_EQ(pack_open_port(&p, 2), -1);                         // a cartridge, but not a pack
    CHECK_EQ(errno, EINVAL);
    return test_summary();
}
