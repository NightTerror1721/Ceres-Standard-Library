#include "ceres/audio.h"
#include "ceres/timer.h"

int audio_available(void)
{
    return mmio_r32(AUDIO_STATUS) != 0xFFFFFFFFu;         // an unattached slot reads all ones
}

void audio_play(unsigned int hz, unsigned int ms, unsigned int volume, enum audio_wave wave)
{
    mmio_w32(AUDIO_FREQ, hz);                             // the device clamps what it is given
    mmio_w32(AUDIO_DURATION, ms);
    mmio_w32(AUDIO_VOLUME, volume);
    mmio_w32(AUDIO_WAVE, (unsigned int)wave);
    mmio_w32(AUDIO_CMD, AUDIO_CMD_PLAY);
}

void audio_stop(void)
{
    mmio_w32(AUDIO_CMD, AUDIO_CMD_STOP);
}

int audio_busy(void)
{
    unsigned int status = mmio_r32(AUDIO_STATUS);
    return status != 0xFFFFFFFFu && (status & AUDIO_BUSY) != 0;
}

// The device raises its request when a tone ends, and that ends a halt whether or not it is taken.
void audio_wait(void)
{
    while (audio_busy())
        __builtin_halt();
}

void audio_beep(void)
{
    audio_play(880, 100, 128, AUDIO_SQUARE);
}

// The twelve notes of the octave from C7 (MIDI 96), in hertz, to the whole hertz. Every other octave is these
// halved (with rounding) or doubled: from this high a base, the low notes come out right to the hertz.
static const unsigned int octave7[12] = { 2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951 };

unsigned int audio_note_hz(int midi_note)
{
    if (midi_note < 0 || midi_note > 127)
        return 0;
    int octave = midi_note / 12 - 1;                      // 60 is C4
    unsigned int hz = octave7[midi_note % 12];
    if (octave >= 7)
        return hz << (octave - 7);
    int shift = 7 - octave;
    return (hz + (1u << (shift - 1))) >> shift;
}

void audio_play_tune(const struct audio_note* tune, int count, unsigned int volume, enum audio_wave wave)
{
    for (int i = 0; i < count; i++)
    {
        unsigned int hz = tune[i].note < 0 ? 0 : audio_note_hz(tune[i].note);
        if (hz == 0)
        {
            audio_stop();
            timer_wait_ms(tune[i].ms);                   // a rest: silence for the time
            continue;
        }
        uint64_t ends = timer_nanos64() + (uint64_t)tune[i].ms * 1000000u;
        audio_play(hz, tune[i].ms, volume, wave);
        audio_wait();
        // With no speakers the tone is never busy: the note still takes its time, so a tune lasts as long as it
        // says whether or not anything plays it.
        timer_wait_until_ns64(ends);
    }
}
