// Music and sound effects on the audio device's channels. See ceres/music.h.
#include "ceres/music.h"
#include "ceres/timer.h"
#include "ceres.h"

#define CH_COMMAND   (AUDIO_BASE + 0x20)
#define CH_STATUS    (AUDIO_BASE + 0x24)
#define CH_COUNT     (AUDIO_BASE + 0x28)
#define CH_REG(ch, off) (AUDIO_BASE + 0x40u + (unsigned int)(ch) * 0x20u + (off))
#define CH_FREQ      0x00u
#define CH_VOLUME    0x04u
#define CH_WAVE      0x08u
#define CH_DUTY      0x0Cu
#define CH_ATTACK    0x10u
#define CH_DECAY     0x14u
#define CH_SUSTAIN   0x18u
#define CH_RELEASE   0x1Cu
#define KEY_ON       1u
#define KEY_OFF      2u
#define STOP         3u

static int valid(int ch)
{
    return ch >= 0 && ch < MUSIC_CHANNELS;
}

int channels_available(void)
{
    unsigned int n = mmio_r32(CH_COUNT);
    return n == 0xFFFFFFFFu ? 0 : (int)n;
}

void channel_set(int ch, const struct voice_settings* v)
{
    if (!valid(ch) || v == 0)
        return;
    mmio_w32(CH_REG(ch, CH_WAVE), (unsigned int)v->wave);
    mmio_w32(CH_REG(ch, CH_DUTY), v->duty != 0 ? v->duty : 128u);
    mmio_w32(CH_REG(ch, CH_VOLUME), v->volume);
    mmio_w32(CH_REG(ch, CH_ATTACK), v->attack_ms);
    mmio_w32(CH_REG(ch, CH_DECAY), v->decay_ms);
    mmio_w32(CH_REG(ch, CH_SUSTAIN), v->sustain);
    mmio_w32(CH_REG(ch, CH_RELEASE), v->release_ms);
}

void channel_note_on(int ch, unsigned int hz)
{
    if (!valid(ch))
        return;
    mmio_w32(CH_REG(ch, CH_FREQ), hz);
    mmio_w32(CH_COMMAND, ((unsigned int)ch << 8) | KEY_ON);
}

void channel_note_off(int ch)
{
    if (valid(ch))
        mmio_w32(CH_COMMAND, ((unsigned int)ch << 8) | KEY_OFF);
}

void channel_stop(int ch)
{
    if (valid(ch))
        mmio_w32(CH_COMMAND, ((unsigned int)ch << 8) | STOP);
}

int channel_sounding(int ch)
{
    unsigned int bits = mmio_r32(CH_STATUS);
    return valid(ch) && bits != 0xFFFFFFFFu && ((bits >> ch) & 1u);
}

// ---- sound effects ----

struct effect
{
    const struct sfx* s;             // 0: the channel is free of effects
    int priority;
    unsigned long long start_ns;
};

static struct effect effects[MUSIC_CHANNELS];
static int effect_ends_restore[MUSIC_CHANNELS];     // the song's instrument goes back on when the effect ends

static const struct music_song* song = 0;

void sfx_update(void)
{
    unsigned long long now = timer_nanos64();
    for (int ch = 0; ch < MUSIC_CHANNELS; ch++)
    {
        struct effect* e = &effects[ch];
        if (e->s == 0)
            continue;
        unsigned long long elapsed_ms = (now - e->start_ns) / 1000000ull;
        if (elapsed_ms >= e->s->ms)
        {
            channel_note_off(ch);
            e->s = 0;
            if (effect_ends_restore[ch] && song != 0)
                channel_set(ch, &song->instrument[ch]);   // the voice is the song's again, from its next note
            continue;
        }
        long long span = (long long)e->s->hz_to - (long long)e->s->hz_from;
        unsigned int hz = (unsigned int)((long long)e->s->hz_from + span * (long long)elapsed_ms / (long long)e->s->ms);
        mmio_w32(CH_REG(ch, CH_FREQ), hz);
    }
}

static int voice_used(int ch)
{
    return song != 0 && song->voice[ch] != 0;
}

int sfx_play(const struct sfx* s, int priority)
{
    if (s == 0 || s->ms == 0)
        return -1;
    sfx_update();
    int chosen = -1;
    // A free channel first, then one of the song's, then the effect of lowest priority below this one.
    for (int ch = MUSIC_CHANNELS - 1; ch >= 0 && chosen < 0; ch--)
        if (effects[ch].s == 0 && !voice_used(ch))
            chosen = ch;
    for (int ch = MUSIC_CHANNELS - 1; ch >= 0 && chosen < 0; ch--)
        if (effects[ch].s == 0)
            chosen = ch;
    if (chosen < 0)
    {
        int lowest = -1;
        for (int ch = 0; ch < MUSIC_CHANNELS; ch++)
            if (effects[ch].priority < priority && (lowest < 0 || effects[ch].priority < effects[lowest].priority))
                lowest = ch;
        chosen = lowest;
    }
    if (chosen < 0)
        return -1;
    struct voice_settings v = { s->wave, 128, s->volume, 1, 0, 255, 20 };
    channel_set(chosen, &v);
    channel_note_on(chosen, s->hz_from);
    effects[chosen].s = s;
    effects[chosen].priority = priority;
    effects[chosen].start_ns = timer_nanos64();
    effect_ends_restore[chosen] = voice_used(chosen);
    return chosen;
}

// ---- songs ----

static const char* cursor[MUSIC_CHANNELS];
static int looping = 0;
static int row = -1;
static unsigned long long started_ns = 0;

// The next token of a voice, into token[8]; 0 at its end.
static int next_token(int ch, char* token)
{
    const char* p = cursor[ch];
    if (p == 0)
        return 0;
    while (*p == ' ')
        p++;
    if (*p == 0)
    {
        cursor[ch] = 0;
        return 0;
    }
    int n = 0;
    while (*p != 0 && *p != ' ')
    {
        if (n < 7)
            token[n++] = *p;
        p++;
    }
    token[n] = 0;
    cursor[ch] = p;
    return 1;
}

// A note's MIDI number ("C4" is 60), or -1.
static int note_of(const char* t)
{
    static const int semitone[7] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
    char letter = t[0];
    if (letter >= 'a' && letter <= 'g')
        letter = (char)(letter - 32);
    if (letter < 'A' || letter > 'G')
        return -1;
    int s = semitone[letter - 'A'];
    int i = 1;
    if (t[i] == '#') { s++; i++; }
    else if (t[i] == 'b') { s--; i++; }
    int octave = 0, digits = 0;
    int negative = t[i] == '-';
    if (negative) i++;
    while (t[i] >= '0' && t[i] <= '9')
    {
        octave = octave * 10 + (t[i] - '0');
        i++;
        digits++;
    }
    if (digits == 0 || t[i] != 0)
        return -1;
    if (negative) octave = -octave;
    int midi = (octave + 1) * 12 + s;
    return midi >= 0 && midi <= 127 ? midi : -1;
}

static void rewind_song(void)
{
    for (int ch = 0; ch < MUSIC_CHANNELS; ch++)
        cursor[ch] = song != 0 ? song->voice[ch] : 0;
    row = -1;
}

void music_play(const struct music_song* s, int loop)
{
    music_stop();
    if (s == 0 || s->length <= 0 || s->rows_per_minute == 0)
        return;
    song = s;
    looping = loop;
    for (int ch = 0; ch < MUSIC_CHANNELS; ch++)
        if (s->voice[ch] != 0 && effects[ch].s == 0)
            channel_set(ch, &s->instrument[ch]);
    rewind_song();
    started_ns = timer_nanos64();
    music_step();                                        // the first row sounds at once
}

void music_stop(void)
{
    if (song != 0)
        for (int ch = 0; ch < MUSIC_CHANNELS; ch++)
            if (song->voice[ch] != 0 && effects[ch].s == 0)
                channel_note_off(ch);
    song = 0;
    row = -1;
}

int music_playing(void)
{
    return song != 0;
}

int music_row(void)
{
    return row;
}

void music_step(void)
{
    if (song == 0)
        return;
    row++;
    if (row >= song->length)
    {
        if (!looping)
        {
            music_stop();
            return;
        }
        rewind_song();
        row = 0;
        started_ns = timer_nanos64();
    }
    for (int ch = 0; ch < MUSIC_CHANNELS; ch++)
    {
        char token[8];
        if (song->voice[ch] == 0 || !next_token(ch, token))
            continue;
        if (effects[ch].s != 0)
            continue;                                    // an effect has this channel; the song's turn comes back
        if (token[0] == '.')
            continue;
        if (token[0] == '-')
            channel_note_off(ch);
        else if (token[0] == 'x')
            channel_stop(ch);
        else
        {
            int midi = note_of(token);
            if (midi >= 0)
            {
                if (effect_ends_restore[ch])
                {
                    channel_set(ch, &song->instrument[ch]);
                    effect_ends_restore[ch] = 0;
                }
                channel_note_on(ch, audio_note_hz(midi));
            }
        }
    }
}

void music_update(void)
{
    sfx_update();
    if (song == 0)
        return;
    for (;;)
    {
        // The row due now: the time since the start * rows per minute / one minute.
        unsigned long long elapsed = timer_nanos64() - started_ns;
        long long due = (long long)(elapsed / 1000000ull) * (long long)song->rows_per_minute / 60000ll;
        if (row >= due)
            return;
        int before = row;
        music_step();
        if (song == 0)
            return;                                      // it ended
        if (row < before)
            continue;                                    // it started over, and its clock with it
    }
}
