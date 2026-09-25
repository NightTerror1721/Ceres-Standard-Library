#pragma once

#include "../ceres.h"

// Audio device (0xFF090000): a tone generator with one voice - a note of a given frequency, duration, volume
// and waveform. A beeper with a choice of timbre, not a sample player. See CeresASM
// docs/07-IO-Devices-and-Ports.md.
//
// The device only holds what the program asked for; a host with speakers plays it (`ceres run --window` does).
// Anywhere else the machine is silent and a tone is never busy, so a program never waits for speakers that are
// not there. audio_available() says whether the machine has the device at all.
//
//   audio_play(440, 200, 128, AUDIO_SQUARE);     // starts a tone and returns at once
//   audio_wait();                                // until it has finished
//
// A tone that runs its whole duration raises interrupt 22 (IRQ_AUDIO) when it ends (ceres/irq.h). For music - four
// channels with envelopes, songs and sound effects that do not block - see ceres/music.h.

#define AUDIO_STATUS    (AUDIO_BASE + 0x00)   // R: bit0 set while a tone is playing
#define AUDIO_FREQ      (AUDIO_BASE + 0x04)   // RW: hertz, 20 .. 20000
#define AUDIO_DURATION  (AUDIO_BASE + 0x08)   // RW: milliseconds; 0 plays until stopped
#define AUDIO_VOLUME    (AUDIO_BASE + 0x0C)   // RW: 0 .. 255
#define AUDIO_WAVE      (AUDIO_BASE + 0x10)   // RW: an enum audio_wave
#define AUDIO_CMD       (AUDIO_BASE + 0x14)   // W: 1 plays with the registers above, 2 stops

#define AUDIO_CMD_PLAY  1
#define AUDIO_CMD_STOP  2
#define AUDIO_BUSY      0x01
#define AUDIO_MIN_HZ    20
#define AUDIO_MAX_HZ    20000

enum audio_wave
{
    AUDIO_SQUARE = 0,
    AUDIO_TRIANGLE = 1,
    AUDIO_SAWTOOTH = 2,
    AUDIO_SINE = 3,
    AUDIO_NOISE = 4
};

int  audio_available(void);        // 1 when the machine has the device (an unattached slot reads all ones)
void audio_play(unsigned int hz, unsigned int ms, unsigned int volume, enum audio_wave wave);
                                   // starts a tone, replacing one already playing; ms 0 plays until audio_stop
void audio_stop(void);
int  audio_busy(void);             // 1 while a tone is playing
void audio_wait(void);             // returns when the tone has ended (at once when nothing is playing); halts meanwhile
void audio_beep(void);             // a short square-wave beep, 880 Hz for 100 ms, and returns at once

// Notes by MIDI number (60 = middle C, 69 = the A of 440 Hz). audio_note_hz() is the frequency of a note in
// equal temperament, to the whole hertz (so the very lowest notes, a few hertz apart, can share a value); a note outside
// 0..127 gives 0.
unsigned int audio_note_hz(int midi_note);

// A tune: notes played one after another, each waited out. A note number below 0 is a rest.
struct audio_note
{
    int note;                      // a MIDI number, or -1 for a rest
    unsigned int ms;               // how long it lasts
};
void audio_play_tune(const struct audio_note* tune, int count, unsigned int volume, enum audio_wave wave);
