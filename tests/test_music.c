// Music and sound effects (ceres/music.h) on the audio device's channels. Nothing is heard in a test, so it reads
// back what the channels were told: the notes a song plays row by row, the instrument, and how effects take a
// channel and give it back.
#include "ceres/test.h"
#include "ceres/music.h"
#include "ceres/timer.h"
#include "ceres.h"

#define FREQ(ch)  mmio_r32(AUDIO_BASE + 0x40u + (unsigned int)(ch) * 0x20u)
#define WAVE(ch)  mmio_r32(AUDIO_BASE + 0x48u + (unsigned int)(ch) * 0x20u)
#define VOL(ch)   mmio_r32(AUDIO_BASE + 0x44u + (unsigned int)(ch) * 0x20u)

static const struct music_song tune = {
    .rows_per_minute = 600,
    .length = 6,
    .voice = { "C4 . E4 - G4 x", "A3 . . . . .", 0, "C2 C#2 Db2 bad . ." },
    .instrument = { MUSIC_LEAD, MUSIC_BASS, MUSIC_PAD, MUSIC_DRUM },
};

static const struct sfx zap = { AUDIO_SAWTOOTH, 1000, 200, 30, 200 };
static const struct sfx boom = { AUDIO_NOISE, 300, 100, 60, 255 };

int main(void)
{
    TEST_SECTION("the channels");
    CHECK_EQ(channels_available(), MUSIC_CHANNELS);
    struct voice_settings lead = MUSIC_LEAD;
    channel_set(2, &lead);
    CHECK_EQ(WAVE(2), (unsigned int)AUDIO_SQUARE);
    CHECK_EQ(VOL(2), 150u);
    channel_note_on(2, 523);
    CHECK_EQ(FREQ(2), 523u);
    CHECK_EQ(channel_sounding(2), 0);                 // keyed on, but no host mixes it here: nothing to wait for
    channel_stop(2);
    CHECK_EQ(channel_sounding(2), 0);

    TEST_SECTION("a song, row by row");
    music_play(&tune, 0);
    CHECK_EQ(music_playing(), 1);
    CHECK_EQ(music_row(), 0);
    CHECK_EQ(FREQ(0), audio_note_hz(60));             // C4
    CHECK_EQ(FREQ(1), audio_note_hz(57));             // A3
    CHECK_EQ(FREQ(3), audio_note_hz(36));             // C2
    CHECK_EQ(WAVE(1), (unsigned int)AUDIO_TRIANGLE);  // the bass's instrument
    music_step();                                     // ".": all held
    CHECK_EQ(FREQ(0), audio_note_hz(60));
    CHECK_EQ(FREQ(3), audio_note_hz(37));             // C#2
    music_step();
    CHECK_EQ(FREQ(0), audio_note_hz(64));             // E4
    CHECK_EQ(FREQ(3), audio_note_hz(37));             // Db2 is C#2
    music_step();                                     // "-": let go; "bad" is ignored
    music_step();
    CHECK_EQ(FREQ(0), audio_note_hz(67));             // G4
    music_step();                                     // "x": silent at once
    CHECK_EQ(channel_sounding(0), 0);
    music_step();                                     // past the end, not looping: over
    CHECK_EQ(music_playing(), 0);

    TEST_SECTION("in real time, looping");
    music_play(&tune, 1);
    uint64_t until = timer_nanos64() + 250000000ull;  // a quarter second at 10 rows a second
    while (timer_nanos64() < until)
        music_update();
    CHECK(music_playing());
    CHECK(music_row() >= 0 && music_row() < tune.length);   // it came round again, in range
    music_stop();
    CHECK_EQ(music_playing(), 0);

    TEST_SECTION("sound effects");
    music_play(&tune, 1);                             // channels 0, 1 and 3 are the song's; 2 is free
    CHECK_EQ(sfx_play(&zap, 1), 2);                   // the free one first
    CHECK_EQ(WAVE(2), (unsigned int)AUDIO_SAWTOOTH);
    CHECK_EQ(FREQ(2), 1000u);
    int taken = sfx_play(&boom, 5);                   // then one of the song's
    CHECK(taken == 3);
    CHECK_EQ(WAVE(3), (unsigned int)AUDIO_NOISE);
    music_step();                                     // the song leaves that channel alone meanwhile
    CHECK_EQ(WAVE(3), (unsigned int)AUDIO_NOISE);
    CHECK_EQ(sfx_play(&zap, 0), 1);                   // a third takes another of the song's
    CHECK_EQ(sfx_play(&zap, 0), 0);
    CHECK_EQ(sfx_play(&zap, 0), -1);                  // all four busy, none below priority 0
    CHECK_EQ(sfx_play(&boom, 9), 0);                  // a higher one takes the lowest
    until = timer_nanos64() + 100000000ull;
    while (timer_nanos64() < until)
        sfx_update();                                 // they all end
    music_step();
    music_step();
    CHECK_EQ(WAVE(3), (unsigned int)AUDIO_NOISE);     // the drum is noise too: its own instrument back
    CHECK_EQ(VOL(3), 180u);                           // ...at the drum's volume, not the effect's 255
    music_stop();
    return test_summary();
}
