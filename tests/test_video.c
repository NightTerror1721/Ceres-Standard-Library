// The GPU's screen (ceres/video.h): its level, its resolution, the vertical blank in the machine's time, and the
// copy engine.
#include "ceres/test.h"
#include "ceres/video.h"
#include "ceres/timer.h"
#include "string.h"

static unsigned int ram[64];

int main(void)
{
    TEST_SECTION("what the GPU is");
    CHECK_EQ((int)mmio_r32(GPU_ID), 0x55504743);
    CHECK(video_max_level() >= VIDEO_BITMAP);             // the standard profile's is V5
    CHECK(video_vram_size() >= 32u * 1024u * 1024u);

    TEST_SECTION("level");
    CHECK_EQ(video_level(), VIDEO_TEXT);                  // it starts in text
    CHECK_EQ(video_set_level(VIDEO_BITMAP), VIDEO_BITMAP);
    CHECK(video_set_level(6) <= video_max_level());       // never above what the profile allows
    CHECK_EQ(video_set_level(VIDEO_TEXT), VIDEO_TEXT);

    TEST_SECTION("resolution");
    CHECK_EQ(video_width(), 640);
    CHECK_EQ(video_height(), 480);
    CHECK_EQ(video_set_resolution(320, 240), 0);
    CHECK_EQ(video_width(), 320);
    CHECK_EQ((int)mmio_r32(GPU_BASE + 0x204), 40);        // the text plane's columns follow: 320 / 8
    CHECK_EQ(video_set_resolution(0, 10), -1);
    CHECK_EQ(video_set_resolution(5000, 240), -1);        // past the profile's largest
    CHECK_EQ(video_set_resolution(640, 480), 0);

    TEST_SECTION("the vertical blank keeps the machine's time");
    CHECK_EQ(video_refresh(), 60);
    video_wait_vblank();
    unsigned int frame = video_frame();
    uint64_t start = timer_nanos64();
    for (int i = 0; i < 6; i++)
        video_wait_vblank();
    CHECK_EQ(video_frame(), frame + 6);
    uint64_t took = timer_nanos64() - start;              // six frames of 1/60 s: 100 ms, to the loop's last look
    CHECK(took >= 99000000u && took <= 100100000u);
    video_present();
    CHECK(mmio_r32(GPU_STATUS) & GPU_STATUS_FLIP_PENDING);
    video_wait_present();
    CHECK((mmio_r32(GPU_STATUS) & GPU_STATUS_FLIP_PENDING) == 0);
    video_set_background(0x123456);
    CHECK_EQ((int)mmio_r32(GPU_BACKGROUND), 0x123456);
    video_set_background(0);

    TEST_SECTION("the copy engine");
    unsigned char* vram = (unsigned char*)(VRAM_BASE + 0x00800000u);
    for (int i = 0; i < 64; i++)
        ram[i] = 0x01010101u * (unsigned int)i;
    CHECK_EQ(video_copy(vram, ram, sizeof ram), 0);       // RAM to VRAM
    CHECK_EQ(((unsigned int*)vram)[63], 0x3F3F3F3Fu);
    memset(ram, 0, sizeof ram);
    CHECK_EQ(video_copy(ram, vram, sizeof ram), 0);       // and back
    CHECK_EQ(ram[5], 0x05050505u);
    CHECK_EQ(video_fill(vram + 1, 0xAABBCCDDu, 6), 0);    // the pattern lies as a word store at the address would
    CHECK_EQ(vram[0], 0);
    CHECK_EQ(vram[1], 0xCC);
    CHECK_EQ(vram[4], 0xDD);
    CHECK_EQ(vram[7], 0x01);                              // past the six bytes: what the copy left (word 1)
    CHECK_EQ(video_copy((void*)0x90000000u, ram, 4), -1); // the empty space between RAM and VRAM
    mmio_w32(GPU_IRQ_STATUS, 8u);                         // the fault acknowledged
    return test_summary();
}
