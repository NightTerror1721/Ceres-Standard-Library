// LZ4 (ceres/lz.h): blocks built by hand and read back, frames as the lz4 tool writes them (linked and independent
// blocks, stored ones, a content size, a skippable frame), whatever compresses coming back the same, and broken
// input refused without a read or write out of bounds.
#include "ceres/test.h"
#include "ceres/lz.h"
#include "string.h"
#include "errno.h"
#include "stdlib.h"

static unsigned char packed[LZ4_BOUND(4096)];
static unsigned char original[4096];
static unsigned char back[4096];

static int round_trip(size_t n)
{
    int size = lz4_compress(original, n, packed, sizeof packed);
    if (size < 0 || (size_t)size > lz4_bound(n))
        return -1;
    memset(back, 0xEE, sizeof back);
    if (lz4_decompress(packed, (size_t)size, back, n) != (int)n || memcmp(back, original, n) != 0)
        return -1;
    return size;
}

int main(void)
{
    TEST_SECTION("a block by hand");
    // 'a', then 8 bytes one back (so eight more a's); then the last literals, "hello".
    static const unsigned char block[] = { 0x14, 'a', 0x01, 0x00, 0x50, 'h', 'e', 'l', 'l', 'o' };
    char out[32];
    CHECK_EQ(lz4_decompress(block, sizeof block, out, sizeof out), 14);
    CHECK(memcmp(out, "aaaaaaaaahello", 14) == 0);
    errno = 0;
    CHECK_EQ(lz4_decompress(block, sizeof block, out, 13), -1);        // one byte short of room
    CHECK_EQ(errno, ENOSPC);
    static const unsigned char no_offset[] = { 0x14, 'a', 0x00, 0x00, 0x00 };
    errno = 0;
    CHECK_EQ(lz4_decompress(no_offset, sizeof no_offset, out, sizeof out), -1);
    CHECK_EQ(errno, EINVAL);
    static const unsigned char too_far[] = { 0x14, 'a', 0x02, 0x00, 0x00 };   // two back, one written
    CHECK_EQ(lz4_decompress(too_far, sizeof too_far, out, sizeof out), -1);
    CHECK_EQ(lz4_decompress(block, 3, out, sizeof out), -1);           // cut short
    static const unsigned char long_run[] = { 0xF0, 255, 255, 255 };   // more literals than there are bytes
    CHECK_EQ(lz4_decompress(long_run, sizeof long_run, out, sizeof out), -1);
    static const unsigned char empty[] = { 0x00 };
    CHECK_EQ(lz4_decompress(empty, 1, out, sizeof out), 0);

    TEST_SECTION("frames");
    static const unsigned char frame[] = {
        0x04, 0x22, 0x4D, 0x18, 0x60, 0x40, 0x82,                      // independent blocks of 64 KiB
        10, 0, 0, 0, 0x14, 'a', 0x01, 0x00, 0x50, 'h', 'e', 'l', 'l', 'o',
        3, 0, 0, 0x80, 'x', 'y', 'z',                                  // a stored block
        0, 0, 0, 0 };
    CHECK_EQ(lz4_frame_decompress(frame, sizeof frame, out, sizeof out), 17);
    CHECK(memcmp(out, "aaaaaaaaahelloxyz", 17) == 0);
    CHECK_EQ((int)lz4_frame_content_size(frame, sizeof frame), -1);    // it does not say
    static const unsigned char linked[] = {
        0x50, 0x2A, 0x4D, 0x18, 2, 0, 0, 0, 0xAB, 0xCD,                // a skippable frame first
        0x04, 0x22, 0x4D, 0x18, 0x4C, 0x40,                            // linked blocks, a content size and checksum
        19, 0, 0, 0, 0, 0, 0, 0, 0x99,
        10, 0, 0, 0, 0x14, 'a', 0x01, 0x00, 0x50, 'h', 'e', 'l', 'l', 'o',
        5, 0, 0, 0, 0x00, 0x0E, 0x00, 0x10, 'z',                       // four bytes from the block before, then z
        0, 0, 0, 0, 0x11, 0x22, 0x33, 0x44 };
    CHECK_EQ(lz4_frame_decompress(linked, sizeof linked, out, sizeof out), 19);
    CHECK(memcmp(out, "aaaaaaaaahelloaaaaz", 19) == 0);
    CHECK_EQ((int)lz4_frame_content_size(linked + 10, sizeof linked - 10), 19);
    static const unsigned char checked[] = {
        0x04, 0x22, 0x4D, 0x18, 0x70, 0x40, 0x11,                      // with a checksum after every block
        10, 0, 0, 0, 0x14, 'a', 0x01, 0x00, 0x50, 'h', 'e', 'l', 'l', 'o', 1, 2, 3, 4,
        2, 0, 0, 0x80, '!', '?', 5, 6, 7, 8,
        0, 0, 0, 0 };
    CHECK_EQ(lz4_frame_decompress(checked, sizeof checked, out, sizeof out), 16);
    CHECK(memcmp(out, "aaaaaaaaahello!?", 16) == 0);
    CHECK_EQ(lz4_frame_decompress(checked, sizeof checked - 6, out, sizeof out), -1);   // a block's checksum cut off
    static unsigned char two[64];                                      // one frame, and a second after it
    memcpy(two, frame, sizeof frame);
    memcpy(two + 32, frame, sizeof frame);
    static char both[64];
    CHECK_EQ(lz4_frame_decompress(two, 64, both, sizeof both), 34);
    CHECK(memcmp(both + 17, "aaaaaaaaahelloxyz", 17) == 0);
    memcpy(two + 32, "trailing bytes, no frame", 24);
    CHECK_EQ(lz4_frame_decompress(two, 56, out, sizeof out), 17);      // what follows the last frame is left
    unsigned char bad[32];                                             // (sizeof frame: not a constant to Ceres-C)
    memcpy(bad, frame, sizeof frame);
    bad[4] = 0x20;                                                     // version 00
    CHECK_EQ(lz4_frame_decompress(bad, sizeof bad, out, sizeof out), -1);
    CHECK_EQ(lz4_frame_decompress(frame, 20, out, sizeof out), -1);    // cut in a block
    CHECK_EQ(lz4_frame_decompress(frame, sizeof frame, out, 16), -1);

    TEST_SECTION("round trips");
    CHECK_EQ(round_trip(0), 1);                                        // one token of nothing
    memcpy(original, "short", 5);
    CHECK_EQ(round_trip(5), 6);                                        // too short to look for matches
    for (int i = 0; i < 4096; i++)
        original[i] = (unsigned char)(i % 7 == 0 ? 'x' : 'a' + i % 3);
    int size = round_trip(4096);
    CHECK(size > 0 && size < 100);                                     // repetitive: far smaller
    const char* text = "It was the best of times, it was the worst of times, it was the age of wisdom, "
                       "it was the age of foolishness, it was the epoch of belief, it was the epoch of incredulity.";
    size_t length = strlen(text);
    memcpy(original, text, length);
    size = round_trip(length);
    CHECK(size > 0 && (size_t)size < length);
    unsigned int seed = 12345;
    for (int i = 0; i < 4096; i++)
    {
        seed = seed * 1103515245u + 12345u;
        original[i] = (unsigned char)(seed >> 16);
    }
    size = round_trip(4096);
    CHECK(size > 4096 && (size_t)size <= lz4_bound(4096));             // noise grows, but within the bound
    memset(original, 0, 4096);
    original[4000] = 1;
    CHECK(round_trip(4096) > 0);                                       // a run longer than 255 and more
    errno = 0;
    CHECK_EQ(lz4_compress(original, 4096, packed, 10), -1);            // no room
    CHECK_EQ(errno, ENOSPC);
    return test_summary();
}
