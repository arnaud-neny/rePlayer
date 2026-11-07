// SPDX-License-Identifier: MIT
// Jochen Hippel 7 voice player: the software mixer. Four voices are mixed into the buffers that Paula channel 0 plays
// (two buffers, one is filled while the other plays; the channel's audio interrupt is the tick). A voice steps through
// its sample at the ratio of the mixing period to its own period, in 16.16 fixed point; its samples go through a table
// that scales them by the voice's volume, the four are added and a second table turns the sum into the output byte.

#include "h7v_internal.h"

#include <string.h>

#define VOLUME_LEVELS 64
#define MAX_VOLUME 63
#define MIN_SAMPLE_WORDS 0x11 // a voice with fewer words than this plays the silent sample instead
#define SILENCE_WORDS 0x80
#define MAX_SAMPLE_WORDS 0x3FFF
#define CLIP_TABLE_SIZE 1024
#define OFFSET_BINARY 0x80

// The samples of a buffer: the interrupt's tempo gives the length, and the buffer holds this many words
#define BUFFER_WORDS (H7V_BUFFER_BYTES / 2)

// What a voice that never played reads: 0xD0 bytes before the end of the idle window, and its volume is 0 until a note
// starts.
#define IDLE_BYTES 0xD0
#define IDLE_POSITION 0xFFF0

// The output byte for the sum of the four voices' table values (each 0..255, silence 128): the sum divided by 4, or
// clipped to the range of one voice.

static void build_clip_table(H7vMixer* mixer) {
    for (int sum = 0; sum < CLIP_TABLE_SIZE; sum++) {
        int value;
        if (mixer->hard_clip) {
            value = sum < 384 ? OFFSET_BINARY : (sum < 640 ? OFFSET_BINARY + sum - 384 : OFFSET_BINARY - 1);
        } else {
            value = OFFSET_BINARY + sum / 4;
        }
        mixer->clip_table[sum] = (uint8_t)value;
    }
}

// A sample byte at a volume 0..63, offset binary.

static void build_volume_table(H7vMixer* mixer) {
    for (int volume = 0; volume < VOLUME_LEVELS; volume++) {
        for (int byte = 0; byte < 256; byte++) {
            const int scaled = (int8_t)byte * volume / MAX_VOLUME;
            mixer->volume_table[volume][byte] = (uint8_t)(scaled + OFFSET_BINARY);
        }
    }
}

void h7v_mixer_init(H7vMixer* mixer, bool hard_clip, int8_t tempo_offset) {
    *mixer = (H7vMixer) { 0 };
    mixer->hard_clip = hard_clip;
    mixer->next = 1;
    build_clip_table(mixer);
    build_volume_table(mixer);
    for (int i = 0; i < H7V_MIXED_TRACKS; i++) {
        H7vMixVoice* v = &mixer->voice[i];
        v->pos = IDLE_POSITION;
        v->end = mixer->idle + IDLE_BYTES;
        v->reload_end = v->end;
        v->reload_bytes = IDLE_BYTES;
    }
    mixer->output = (NuadePaulaChannel) {
        .lc = (const int8_t*)mixer->buffer[0], .len_words = 2, .period = H7V_MIX_PERIOD, .volume = H7V_MIX_VOLUME
    };
    h7v_mixer_set_tempo(mixer, tempo_offset);
}

// The interrupt's length in output words: the rate gives 20 samples per kHz, the tempo offset scales it in 256ths.
// The replayer's buffers hold 300 words; a longer interrupt would write past them, so it is cut there.

void h7v_mixer_set_tempo(H7vMixer* mixer, int8_t tempo_offset) {
    const uint32_t samples = ((uint32_t)H7V_MIX_RATE * 100 / 5) * (uint32_t)(uint16_t)(tempo_offset + 0x100) >> 8;
    const uint32_t words = (uint16_t)samples >> 1;
    mixer->words = (uint16_t)(words < BUFFER_WORDS ? words : BUFFER_WORDS);
}

// Switch a voice on or off for the coming buffer, and set where it continues at the end of its sample. A voice that
// was off starts from the beginning of the sample in the registers.

static void update_voice(H7vMixer* mixer, H7vMixVoice* m, H7vVoice* v) {
    if (!v->mixer_on) {
        m->step = 0;
        m->restart = true;
        return;
    }
    NuadePaulaChannel* regs = &v->paula;
    if ((regs->len_words & MAX_SAMPLE_WORDS) < MIN_SAMPLE_WORDS) {
        regs->lc = mixer->silence;
        regs->len_words = SILENCE_WORDS;
    }
    const uint16_t bytes = (uint16_t)((regs->len_words & MAX_SAMPLE_WORDS) * 2);
    m->reload_end = regs->lc + bytes;
    m->reload_bytes = bytes;
    if (m->restart) {
        m->restart = false;
        m->end = regs->lc + bytes;
        m->pos = (uint32_t)(0 - (uint32_t)bytes);
    }
}

void h7v_mixer_voices_update(H7vMixer* mixer, H7vSong* song) {
    for (int i = 0; i < H7V_MIXED_TRACKS; i++) {
        update_voice(mixer, &mixer->voice[i], &song->voices[H7V_PAULA_TRACKS + i]);
    }
}

// The step and the volume row of a voice from its period and volume registers. A voice with period 0 keeps what it has.

static void set_step(H7vMixVoice* m, const H7vVoice* v) {
    const uint16_t period = v->paula.period;
    if (period == 0) {
        return;
    }
    m->volume = (uint8_t)(v->paula.volume < MAX_VOLUME + 1 ? v->paula.volume : MAX_VOLUME);
    // 2048 times the ratio of the mixing period to the voice's period; a quotient that does not fit 16 bits is not
    // stored (divu overflows) and the dividend's low word stays
    const uint32_t dividend = (uint32_t)H7V_MIX_PERIOD << 11;
    const uint32_t quotient = (dividend >> 16) >= period ? dividend : dividend / period;
    const uint32_t fixed = (quotient & 0xFFFF) << 5;
    m->step = fixed << 16 | fixed >> 16;
}

// One output sample later: the position moves by the step (the carry of the fraction goes to the integer part); when it
// reaches the end of the sample the voice continues with its loop.

static void advance(H7vMixVoice* m) {
    const uint64_t sum = (uint64_t)m->pos + m->step;
    const uint32_t carry = (uint32_t)(sum >> 32);
    const uint32_t pos = (uint32_t)sum;
    const uint16_t offset = (uint16_t)((pos & 0xFFFF) + carry);
    m->pos = (pos & 0xFFFF0000u) | offset;
    if (!(offset & 0x8000)) {
        m->end = m->reload_end;
        m->pos = (pos & 0xFFFF0000u) | (uint16_t)(offset - m->reload_bytes);
    }
}

void h7v_mixer_mix(H7vMixer* mixer, const H7vSong* song) {
    for (int i = 0; i < H7V_MIXED_TRACKS; i++) {
        set_step(&mixer->voice[i], &song->voices[H7V_PAULA_TRACKS + i]);
    }
    int8_t* out = mixer->buffer[mixer->next];
    for (uint32_t n = 0; n < (uint32_t)mixer->words * 2; n++) {
        uint32_t sum = 0;
        for (int i = 0; i < H7V_MIXED_TRACKS; i++) {
            const H7vMixVoice* m = &mixer->voice[i];
            sum += mixer->volume_table[m->volume][(uint8_t)m->end[(int16_t)(m->pos & 0xFFFF)]];
        }
        out[n] = (int8_t)mixer->clip_table[sum];
        for (int i = 0; i < H7V_MIXED_TRACKS; i++) {
            advance(&mixer->voice[i]);
        }
    }
}
