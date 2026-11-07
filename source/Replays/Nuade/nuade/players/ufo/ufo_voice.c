// SPDX-License-Identifier: MIT
// UFO player: the four Paula voices. A note starts a sample with a volume that is the velocity scaled by the channel,
// then the volume follows the voice's envelope: it moves half the way to a target every tick (up to the note's volume
// at the start, down to 0 once the note is released). The period of a note is bent by the channel's bend value, in
// steps of the distance to the neighbouring notes of the period table.

#include "nuade_tracker.h"
#include "ufo_internal.h"

#define VOLUME_MASK 0x7F // the eagleplayer's volume patch keeps 7 bits
#define ATTACK_TARGET 0x63
#define NOTE_BASE 24 // the lowest note of the period table
#define OCTAVE 12
#define BEND_SHIFT 4
#define SILENT_REPEAT_WORDS 2 // an instrument without loop repeats a silent long word

// The replayer's table of periods, preceded by the two words of code before it that the lowest two notes read when
// they look at the note below. The last 24 entries repeat the top octave.
#define PERIOD_LEAD_IN 2
static const uint16_t period_table[PERIOD_LEAD_IN + UFO_PERIOD_ENTRIES] = {
    0x28C0, 0x4E75, 0x0FBE, 0x0EEE, 0x0DF7, 0x0D09, 0x0C8C, 0x0BDC, 0x0B1A, 0x0A8F, 0x09F4, 0x0969, 0x08D6,
    0x0853, 0x07DF, 0x0766, 0x070A, 0x069C, 0x063A, 0x05E4, 0x058D, 0x053F, 0x04F2, 0x04AE, 0x046B, 0x0429,
    0x03EF, 0x03B7, 0x0381, 0x034E, 0x0320, 0x02F2, 0x02C8, 0x029F, 0x027B, 0x0257, 0x0235, 0x0216, 0x01F7,
    0x01DB, 0x01C0, 0x01A7, 0x0190, 0x0179, 0x0164, 0x0150, 0x013D, 0x012B, 0x011A, 0x010A, 0x00FB, 0x00ED,
    0x00E0, 0x00D3, 0x00C7, 0x00BC, 0x00B2, 0x00A8, 0x009E, 0x0095, 0x008D, 0x0086, 0x00FB, 0x00ED, 0x00E0,
    0x00D3, 0x00C7, 0x00BC, 0x00B2, 0x00A8, 0x009E, 0x0095, 0x008D, 0x0086, 0x00FB, 0x00ED, 0x00E0, 0x00D3,
    0x00C7, 0x00BC, 0x00B2, 0x00A8, 0x009E, 0x0095, 0x008D, 0x0086,
};

// The table entry of a note index; -2 and -1 are the two words in front of it.

static uint16_t period_of(int index) {
    return period_table[PERIOD_LEAD_IN + index];
}

// Notes below the table's lowest wrap up into its first octave.

static unsigned note_index(uint8_t note) {
    return note >= NOTE_BASE ? (unsigned)note - NOTE_BASE : (unsigned)note % OCTAVE;
}

// AUDxLC ignores its lowest bit.

static const int8_t* chip_address(const int8_t* p) {
    return (const int8_t*)((uintptr_t)p & ~(uintptr_t)1);
}

// The period of a note with the channel's bend: towards a higher pitch for a positive bend (the replayer's u5a8e).

static uint16_t bent_period(const UfoState* state, unsigned channel, unsigned voice) {
    const int8_t bend = state->channels[channel].bend;
    const UfoVoice* v = &state->voices[voice];
    const int32_t base = state->channels[channel].base_period;
    if (bend > 0) {
        return (uint16_t)(base - (int32_t)bend * (int16_t)v->slide_down);
    }
    if (bend < 0) {
        const int16_t steps = (int8_t)-bend; // the byte negation: -128 stays -128
        return (uint16_t)(base + (int32_t)steps * (int16_t)v->slide_up);
    }
    return (uint16_t)base;
}

// A release: the envelope of the voice that plays this note on this channel starts falling.

static void release_note(UfoState* state, unsigned channel, unsigned index) {
    for (unsigned i = 0; i < UFO_VOICES; i++) {
        UfoVoice* v = &state->voices[i];
        if (v->channel == channel && v->note_index == index) {
            v->target = (int8_t)-v->instrument_volume;
            v->ceiling = 0;
        }
    }
}

static void start_note(const UfoSong* song, UfoState* state, unsigned channel, unsigned index, uint8_t volume) {
    UfoChannel* chan = &state->channels[channel];
    UfoVoice* v = &state->voices[channel % UFO_VOICES];
    const UfoInstrument* inst = &song->instruments[chan->instrument];

    nuade_paula_channel_dma_off(&v->paula);
    if (inst->start == nullptr) {
        return;
    }
    v->note_index = (uint16_t)index;
    v->channel = (uint16_t)channel;
    v->target = ATTACK_TARGET;
    v->instrument_volume = inst->volume;
    v->slide_down = (uint16_t)((period_of((int)index) - period_of((int)index + 2)) >> BEND_SHIFT);
    v->slide_up = (uint16_t)((period_of((int)index - 2) - period_of((int)index)) >> BEND_SHIFT);
    v->ceiling = volume;
    chan->base_period = period_of((int)index);

    v->paula.lc = chip_address(inst->start);
    v->paula.period = bent_period(state, channel, channel % UFO_VOICES);
    v->paula.len_words = (uint16_t)((inst->end - inst->start) >> 1);
    v->paula.volume = volume & VOLUME_MASK;
    v->level = (int8_t)volume;
    nuade_paula_channel_dma_on(&v->paula);
    // the replayer waits for the DMA start before it writes the repeat
    if (inst->loops) {
        v->paula.lc = chip_address(inst->loop_start);
        v->paula.len_words = (uint16_t)((inst->end - inst->loop_start) >> 1);
    } else {
        v->paula.lc = song->silence;
        v->paula.len_words = SILENT_REPEAT_WORDS;
    }
    v->display_note = nuade_period_to_note(chan->base_period);
    v->display_instrument = chan->instrument;
}

// A note event (the replayer's u5b02); a volume of 0 (a note off, or a velocity that scales to 0) releases the note
// instead.

void ufo_voice_note(const UfoSong* song, UfoState* state, unsigned channel, uint8_t note, uint8_t volume) {
    const unsigned index = note_index(note);
    if (volume == 0) {
        release_note(state, channel, index);
        return;
    }
    if (index > UFO_MAX_NOTE_INDEX || state->channels[channel].instrument >= UFO_INSTRUMENTS) {
        state->fault = true; // the replayer reads past its period table, or past its instrument records
        return;
    }
    start_note(song, state, channel, index, volume);
}

// A new bend value changes the period of the voice at once (u608c).

void ufo_voice_update_bend(UfoState* state, unsigned channel) {
    const unsigned voice = channel % UFO_VOICES;
    state->voices[voice].paula.period = bent_period(state, channel, voice);
}

// The volume pass of the interrupt (u5cda): the envelope of every voice.

void ufo_voices_update_volumes(UfoState* state) {
    for (unsigned i = 0; i < UFO_VOICES; i++) {
        UfoVoice* v = &state->voices[i];
        v->level = (int8_t)(((int32_t)v->level * 2 + v->target) >> 1);
        if (v->target < 0) {
            if (v->level < 0) {
                v->level = 0;
                v->target = 0;
            }
        } else if (v->level > (int8_t)v->ceiling) {
            v->level = (int8_t)v->ceiling;
            v->target = 0;
        }
        v->paula.volume = state->seq.playing ? (uint8_t)((uint16_t)(int16_t)v->level & VOLUME_MASK) : 0;
    }
}
