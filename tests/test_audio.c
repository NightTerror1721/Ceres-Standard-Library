// The tone generator, without a host to play it: the registers, the clamping the device does, and the note table.
// With no speakers a tone is never busy, so nothing here waits.
#include "ceres/test.h"
#include "ceres/audio.h"

int main(void)
{
    TEST_SECTION("the device");
    CHECK(audio_available());
    CHECK_EQ(audio_busy(), 0);

    TEST_SECTION("what a tone asks for reaches the registers");
    audio_play(440, 250, 200, AUDIO_TRIANGLE);
    CHECK_EQ((int)mmio_r32(AUDIO_FREQ), 440);
    CHECK_EQ((int)mmio_r32(AUDIO_DURATION), 250);
    CHECK_EQ((int)mmio_r32(AUDIO_VOLUME), 200);
    CHECK_EQ((int)mmio_r32(AUDIO_WAVE), AUDIO_TRIANGLE);
    CHECK_EQ(audio_busy(), 0);                          // nobody is playing it
    audio_wait();                                       // so this returns at once

    TEST_SECTION("the device clamps what it is given");
    audio_play(5, 0, 1000, AUDIO_NOISE);
    CHECK_EQ((int)mmio_r32(AUDIO_FREQ), AUDIO_MIN_HZ);
    CHECK_EQ((int)mmio_r32(AUDIO_VOLUME), 255);
    CHECK_EQ((int)mmio_r32(AUDIO_WAVE), AUDIO_NOISE);
    audio_play(90000, 10, 0, AUDIO_SINE);
    CHECK_EQ((int)mmio_r32(AUDIO_FREQ), AUDIO_MAX_HZ);
    CHECK_EQ((int)mmio_r32(AUDIO_VOLUME), 0);
    mmio_w32(AUDIO_WAVE, 77);                           // not a waveform: the one set stays
    CHECK_EQ((int)mmio_r32(AUDIO_WAVE), AUDIO_SINE);
    audio_stop();
    audio_beep();
    CHECK_EQ((int)mmio_r32(AUDIO_FREQ), 880);
    CHECK_EQ((int)mmio_r32(AUDIO_DURATION), 100);

    TEST_SECTION("notes");
    CHECK_EQ((int)audio_note_hz(69), 440);              // A4
    CHECK_EQ((int)audio_note_hz(60), 262);              // middle C
    CHECK_EQ((int)audio_note_hz(72), 523);              // C5
    CHECK_EQ((int)audio_note_hz(81), 880);              // A5
    CHECK_EQ((int)audio_note_hz(57), 220);              // A3
    CHECK_EQ((int)audio_note_hz(61), 277);              // C#4
    CHECK_EQ((int)audio_note_hz(68), 415);              // G#4
    CHECK_EQ((int)audio_note_hz(0), 8);                 // the lowest, C-1
    CHECK_EQ((int)audio_note_hz(127), 12544);           // the highest, G9
    CHECK_EQ((int)audio_note_hz(-1), 0);
    CHECK_EQ((int)audio_note_hz(128), 0);
    int never_falls = 1, rising = 1;
    for (int n = 1; n < 128; n++)
    {
        if (audio_note_hz(n) < audio_note_hz(n - 1))
            never_falls = 0;
        if (n >= 24 && audio_note_hz(n) <= audio_note_hz(n - 1))
            rising = 0;
    }
    CHECK(never_falls);                                 // a note is never lower than the one below it
    CHECK(rising);                                      // and from C1 up (33 Hz) always higher: whole hertz only ties the lowest few

    TEST_SECTION("a tune takes the time it says");
    struct audio_note tune[] = { { 60, 15 }, { -1, 10 }, { 64, 15 } };
    audio_play_tune(tune, 3, 100, AUDIO_SINE);
    CHECK_EQ((int)mmio_r32(AUDIO_FREQ), 330);           // the last note played: E4

    return test_summary();
}
