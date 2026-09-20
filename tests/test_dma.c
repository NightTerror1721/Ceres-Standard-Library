// The DMA controller: copies of many lengths and alignments, what it reports, and that it stays inside the range.
#include "ceres/test.h"
#include "ceres/dma.h"
#include "string.h"

#define BUF 4200
static unsigned char src[BUF + 8];
static unsigned char dst[BUF + 8];

static void fill_source(void)
{
    for (int i = 0; i < BUF + 8; i++)
        src[i] = (unsigned char)(i * 7 + 3);
}

// Copies `len` bytes from src+so to dst+do_ and checks every byte of the result, and the guards around it.
static int copy_and_check(int so, int do_, unsigned int len)
{
    memset(dst, 0xEE, sizeof(dst));
    unsigned int moved = dma_copy(dst + do_, src + so, len);
    int ok = 1;
    if (moved != len) ok = 0;
    if (dma_transferred() != len) ok = 0;
    if (!dma_done() || dma_busy()) ok = 0;
    if (memcmp(dst + do_, src + so, len) != 0) ok = 0;                   // the copy is right ...
    for (int i = 0; i < do_; i++) if (dst[i] != 0xEE) ok = 0;            // ... nothing before it changed ...
    for (unsigned int i = do_ + len; i < sizeof(dst); i++) if (dst[i] != 0xEE) ok = 0;   // ... nor after
    return ok;
}

int main(void)
{
    fill_source();
    unsigned char source_copy[BUF + 8];
    memcpy(source_copy, src, sizeof(src));

    TEST_SECTION("lengths");
    unsigned int lengths[6] = { 1, 3, 4, 255, 256, 4096 };
    for (int i = 0; i < 6; i++)
        CHECK(copy_and_check(0, 0, lengths[i]));

    TEST_SECTION("unaligned source and destination");
    int offsets[4][2] = { { 1, 3 }, { 3, 1 }, { 2, 2 }, { 1, 0 } };
    for (int i = 0; i < 4; i++)
    {
        CHECK(copy_and_check(offsets[i][0], offsets[i][1], 1));
        CHECK(copy_and_check(offsets[i][0], offsets[i][1], 255));
        CHECK(copy_and_check(offsets[i][0], offsets[i][1], 1024));
        CHECK(copy_and_check(offsets[i][0], offsets[i][1], 4096));
    }

    TEST_SECTION("zero length and the source");
    memset(dst, 0xEE, sizeof(dst));
    CHECK_EQ((int)dma_copy(dst, src, 0), 0);                             // nothing to do, nothing waited for
    CHECK_EQ(dst[0], 0xEE);
    CHECK(memcmp(src, source_copy, sizeof(src)) == 0);                   // the source is never modified

    TEST_SECTION("asynchronous");
    memset(dst, 0x11, sizeof(dst));
    dma_copy_async(dst, src, 2000);
    dma_wait();                                                          // spins until it is not busy
    CHECK(dma_done() && !dma_busy());
    CHECK_EQ((int)dma_transferred(), 2000);
    CHECK(memcmp(dst, src, 2000) == 0);
    CHECK_EQ(dst[2000], 0x11);
    memset(dst, 0x22, sizeof(dst));
    dma_copy_async(dst + 10, src + 5, 300);
    int spins = 0;
    while (!dma_done() && spins < 1000000)
        spins++;
    CHECK(dma_done());
    CHECK(memcmp(dst + 10, src + 5, 300) == 0);
    CHECK_EQ(dst[9], 0x22);
    CHECK_EQ(dst[310], 0x22);

    TEST_SECTION("back to back");
    memset(dst, 0, sizeof(dst));
    unsigned int total = 0;
    for (int chunk = 0; chunk < 8; chunk++)                              // eight copies into eight slices of dst
        total += dma_copy(dst + chunk * 512, src + chunk * 512, 512);
    CHECK_EQ((int)total, 4096);
    CHECK(memcmp(dst, src, 4096) == 0);

    TEST_SECTION("words");
    static unsigned int words_in[64], words_out[64];
    for (int i = 0; i < 64; i++) words_in[i] = 0xA5000000u + (unsigned int)i * 0x01010101u;
    CHECK_EQ((int)dma_copy(words_out, words_in, sizeof(words_in)), 256);
    CHECK(memcmp(words_in, words_out, sizeof(words_in)) == 0);
    return test_summary();
}
