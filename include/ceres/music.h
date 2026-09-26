#pragma once

#include "../stddef.h"
#include "audio.h"

// Music and sound effects on the audio device's four channels (CeresASM 848fae2): each a waveform, a volume and
// an ADSR envelope, mixed by the host. Nothing here blocks: a song moves on when music_update() is called - once
// a frame, or from a task (ceres/task.h) - by the machine's time that has passed, and a sound effect ends by itself.
//
//   static const struct music_song tune = {
//       .rows_per_minute = 480, .length = 8,
//       .voice = { "C4 . E4 . G4 . C5 -", "C3 - - - G2 - - -", 0, 0 },
//       .instrument = { MUSIC_LEAD, MUSIC_BASS },
//   };
//   music_play(&tune, 1);                       // looping
//   for (;;) { game_frame(); music_update(); }
//
// A voice is text, one token a row, separated by spaces: a note ("C4", "F#3", "Bb5": a letter, # or b, the
// octave; C4 is middle C), "-" to let the note go (its release), "." to keep what is sounding, and "x" for
// silence at once. A voice shorter than the song is silent after its end; one of 0 is not used.
//
// A sound effect takes a channel for as long as it lasts - a free one, else one the song is using (that voice
// is silent meanwhile and comes back at its next note), else one playing an effect of lower priority - and
// sweeps its frequency from one pitch to another.

#define MUSIC_CHANNELS 4

struct voice_settings
{
    enum audio_wave wave;
    unsigned int duty;              // a square's high part in 256ths (128 is half); 0 means 128
    unsigned int volume;            // 0..255
    unsigned int attack_ms, decay_ms, sustain, release_ms;   // sustain 0..255
};

// A few instruments to start from.
#define MUSIC_LEAD   { AUDIO_SQUARE, 64, 150, 5, 80, 150, 120 }
#define MUSIC_BASS   { AUDIO_TRIANGLE, 0, 200, 2, 40, 200, 60 }
#define MUSIC_PAD    { AUDIO_SINE, 0, 120, 200, 200, 180, 400 }
#define MUSIC_DRUM   { AUDIO_NOISE, 0, 180, 1, 60, 0, 30 }

// ---- the channels ----
int  channels_available(void);                           // MUSIC_CHANNELS on a machine that has them, else 0
void channel_set(int ch, const struct voice_settings* v);
void channel_note_on(int ch, unsigned int hz);           // the attack starts
void channel_note_off(int ch);                           // the release starts
void channel_stop(int ch);                               // silent at once
int  channel_sounding(int ch);                           // 1 until its release has run out (the host mixes it); 0 with no host that does

// ---- songs ----
struct music_song
{
    unsigned int rows_per_minute;   // the tempo
    int length;                     // rows before it ends (or starts over)
    const char* voice[MUSIC_CHANNELS];
    struct voice_settings instrument[MUSIC_CHANNELS];
};

void music_play(const struct music_song* song, int loop);   // from its first row; stops one already playing
void music_stop(void);                                   // every voice lets go
int  music_playing(void);
int  music_row(void);                                    // the row reached (-1 before the first)
void music_update(void);                                 // moves on by the time since the song started
void music_step(void);                                   // one row, now: for a program that counts its own time

// ---- sound effects ----
struct sfx
{
    enum audio_wave wave;
    unsigned int hz_from, hz_to;    // swept linearly over its length
    unsigned int ms;
    unsigned int volume;
};

int  sfx_play(const struct sfx* s, int priority);        // the channel it took, or -1 when every one is busy with more
void sfx_update(void);                                   // sweeps and ends effects (music_update calls it too)
