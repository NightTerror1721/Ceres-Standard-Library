// Plays the opening of a well-known tune on the tone generator and lists each note as it goes.
//
// A plain `ceres run` has no speakers, so the machine is silent (and never busy) but the listing is the same and
// the tune takes as long as it says; `ceres run --window` plays it. Build it with -DTUNE_MS=20 for a quick
// silent run: every note lasts TUNE_MS milliseconds (200 by default).
#include <stdio.h>
#include "ceres/audio.h"

#ifndef TUNE_MS
#define TUNE_MS 200
#endif

static const char* const note_names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

#define N(midi, beats) { midi, (beats) * TUNE_MS }

static const struct audio_note tune[] = {
    N(64, 1), N(64, 1), N(65, 1), N(67, 1), N(67, 1), N(65, 1), N(64, 1), N(62, 1),
    N(60, 1), N(60, 1), N(62, 1), N(64, 1), N(64, 2), N(-1, 1), N(62, 2)
};

int main(void)
{
    int count = (int)(sizeof(tune) / sizeof(tune[0]));

    printf("audio device: %s\n", audio_available() ? "present" : "absent");
    for (int i = 0; i < count; i++)
    {
        int note = tune[i].note;
        if (note < 0)
        {
            printf("%2d  rest\n", i + 1);
            continue;
        }
        char label[8];
        snprintf(label, sizeof(label), "%s%d", note_names[note % 12], note / 12 - 1);
        printf("%2d  %-4s %5u Hz  %4u ms\n", i + 1, label, audio_note_hz(note), tune[i].ms);
    }

    audio_play_tune(tune, count, 128, AUDIO_TRIANGLE);
    puts("done");
    return 0;
}
