// SPDX-License-Identifier: MIT
// Paul Shields player: what the two sequencers do alike. Envelopes, the volume slide, the period of a note and the
// registers a note start writes. The volume register is the slid volume plus the envelope level, less 64, clamped to
// 0..64.

#include "nuade_utils.h"
#include "ps_internal.h"

#define ENVELOPE_RESTART 0x81 // the next step comes from the envelope's first byte
#define ENVELOPE_FETCH_LIMIT 256
#define MAX_VOLUME 64
#define VOLUME_OFFSET 64 // subtracted from volume + level
#define RELEASE_STEP 2
#define NOTE_C0_BIAS 48 // the note byte of C-0: C-1 is byte 60, period 856
#define NOTE_UNITS_PER_OCTAVE 96

// The volume slide moves the voice's volume one step towards its target every `slide_rate` ticks.

void psh_volume_slide(PshVoice* v) {
    v->slide_counter--;
    if (v->slide_counter != 0 || v->volume_current == v->volume_target) {
        return;
    }
    v->volume_current = (uint16_t)(v->volume_current + (v->volume_current < v->volume_target ? 1 : -1));
    v->slide_counter = v->slide_rate;
}

// The next step of an envelope: a signed byte for the value, or the end or restart mark. Returns false when the
// envelope has no step now; otherwise `step` holds it and `after` is where the envelope goes on.

static bool next_envelope_step(const PshModule* m, int32_t restart, int32_t next, int8_t* step, int32_t* after) {
    int32_t at = next;
    for (int guard = 0; guard < ENVELOPE_FETCH_LIMIT; guard++) {
        const uint8_t byte = psh_u8(m, at++);
        if (byte == PSH_ENVELOPE_END) {
            return false;
        }
        if (byte == ENVELOPE_RESTART) {
            at = restart;
            continue;
        }
        *step = (int8_t)byte;
        *after = at;
        return true;
    }
    return false;
}

void psh_volume_envelope(const PshModule* m, PshVoice* v) {
    int8_t step;
    int32_t after;
    v->volume_counter--;
    if (v->volume_counter != 0 || !next_envelope_step(m, v->volume_restart, v->volume_next, &step, &after)) {
        return;
    }
    v->level = (uint16_t)(v->level + step);
    v->volume_counter = v->volume_interval;
    v->volume_next = after;
}

void psh_pitch_envelope(const PshModule* m, PshVoice* v) {
    int8_t step;
    int32_t after;
    v->pitch_counter--;
    if (v->pitch_counter != 0 || !next_envelope_step(m, v->pitch_restart, v->pitch_next, &step, &after)) {
        return;
    }
    v->period = (uint16_t)(v->period + step);
    v->pitch_counter = v->pitch_interval;
    v->pitch_next = after;
}

// An envelope starts at its first byte, the number of ticks per step, and runs from the byte after it.

void psh_select_volume_envelope(const PshModule* m, PshVoice* v, int64_t offset) {
    v->volume_interval = psh_u8(m, offset);
    v->volume_counter = v->volume_interval;
    v->volume_restart = (int32_t)(offset + 1);
    v->volume_next = v->volume_restart;
}

void psh_select_pitch_envelope(const PshModule* m, PshVoice* v, int64_t offset) {
    v->pitch_interval = psh_u8(m, offset);
    v->pitch_counter = v->pitch_interval;
    v->pitch_restart = (int32_t)(offset + 1);
    v->pitch_next = v->pitch_restart;
}

void psh_select_sample(PshVoice* v, const PshSample* sample) {
    v->sample = sample->start;
    v->length_words = sample->length_words;
    v->loop = sample->loop;
    v->loop_length_words = sample->loop_length_words;
    v->sample_tune = (int16_t)sample->tune;
    v->sample_volume = sample->volume;
}

// A pattern duration: one byte, or a zero and a big-endian word.

void psh_read_duration(const PshModule* m, PshVoice* v) {
    const uint8_t byte = psh_u8(m, v->stream++);
    if (byte != 0) {
        v->duration = byte;
        return;
    }
    const uint8_t high = psh_u8(m, v->stream++);
    v->duration = (uint16_t)(high << 8 | psh_u8(m, v->stream++));
}

// The period of a note byte * 8: the note and the voice's tunings pick an entry of the one-octave table, shifted down
// by the octave, plus the voice's period offset. All sums are 16 bit. A negative sum is out of the table: the oldest
// replayer returns the longest period for it (`negative_is_longest`).
//
// The other replayers read the table at its end minus twice the remainder with no bounds check. A pitch sweep can take
// the remainder outside the table, but only on silent ticks (the sweep tail of a note at volume 0), where the value
// read is whatever memory lies before the table; the index is clamped to the table instead.

#define PERIOD_COUNT 96

static const uint16_t k_periods[PERIOD_COUNT] = {
    27364, 27167, 26972, 26778, 26585, 26394, 26204, 26015, 25828, 25642, 25458, 25275, 25093, 24912, 24733, 24555,
    24378, 24203, 24029, 23856, 23684, 23514, 23345, 23177, 23010, 22845, 22680, 22517, 22355, 22194, 22035, 21876,
    21719, 21562, 21407, 21253, 21100, 20949, 20798, 20648, 20500, 20352, 20206, 20060, 19916, 19773, 19631, 19489,
    19349, 19210, 19072, 18935, 18798, 18663, 18529, 18396, 18263, 18132, 18001, 17872, 17743, 17616, 17489, 17363,
    17238, 17114, 16991, 16869, 16747, 16627, 16507, 16389, 16271, 16154, 16037, 15922, 15807, 15694, 15581, 15469,
    15357, 15247, 15137, 15028, 14920, 14813, 14706, 14601, 14496, 14391, 14288, 14185, 14083, 13982, 13881, 13781,
};

uint16_t psh_note_period(const PshVoice* v, uint16_t note_units, bool negative_is_longest) {
    uint16_t rest = (uint16_t)(note_units + v->pitch_tune + v->sample_tune);
    if (negative_is_longest && (int16_t)rest < 0) {
        return 0xFFFF;
    }
    unsigned octave = 0;
    rest = (uint16_t)(rest - NOTE_UNITS_PER_OCTAVE);
    while ((int16_t)rest >= 0) {
        octave++;
        rest = (uint16_t)(rest - NOTE_UNITS_PER_OCTAVE);
    }
    const int32_t entry = PERIOD_COUNT + (int16_t)(uint16_t)(2 * rest) / 2;
    const uint16_t base = k_periods[entry < 0 ? 0 : entry >= PERIOD_COUNT ? PERIOD_COUNT - 1 : entry];
    const unsigned shift = octave & 63;
    return (uint16_t)((shift < 16 ? base >> shift : 0) + v->note_offset);
}

// A slide: the period runs from `start` to `target` in `duration` ticks. The step per tick is the difference divided by
// the duration, in 16.16 fixed point.

void psh_start_slide(PshVoice* v, uint16_t start, uint16_t target, uint16_t duration) {
    v->period = start;
    const int16_t difference = (int16_t)(target - start);
    const uint32_t distance = difference < 0 ? (uint16_t)-difference : (uint16_t)difference;
    uint32_t step = 0;
    if (duration != 0) {
        const uint32_t whole = distance / duration;
        const uint32_t rest = distance % duration;
        step = whole << 16 | (rest << 16) / duration;
    }
    v->period_slide = difference < 0 ? 0u - step : step;
}

void psh_add_period_slide(PshVoice* v) {
    const uint32_t sum = ((uint32_t)v->period << 16 | v->period_fraction) + v->period_slide;
    v->period = (uint16_t)(sum >> 16);
    v->period_fraction = (uint16_t)sum;
}

// While the note is released the level falls by two per tick, as long as it is not negative.

void psh_release_level(PshVoice* v) {
    if ((int16_t)v->level >= 0) {
        v->level = (uint16_t)(v->level - RELEASE_STEP);
    }
}

// A note starts: the envelopes start over, the channel's DMA goes off and gets the sample; the DMA comes on at the
// start of the next tick, and the loop pointer goes in after it.

void psh_start_note(PshPlayer* p, uint32_t channel) {
    PshVoice* v = &p->voices[channel];
    NuadePaulaChannel* paula = &p->paula[channel];
    v->level = v->volume_override != 0 ? v->volume_override : v->sample_volume;
    v->volume_counter = v->volume_interval;
    v->volume_next = v->volume_restart;
    v->pitch_counter = v->pitch_interval;
    v->pitch_next = v->pitch_restart;
    nuade_paula_channel_dma_off(paula);
    paula->lc = psh_chip_pointer(&p->module, v->sample);
    paula->len_words = v->length_words;
    v->dma_pending = true;
    v->repeat_pending = true;
    v->releasing = false;
}

void psh_write_registers(PshPlayer* p, uint32_t channel, uint16_t period) {
    PshVoice* v = &p->voices[channel];
    const int16_t sum = (int16_t)(v->volume_current + v->level - VOLUME_OFFSET);
    const uint8_t volume = sum < 0 ? 0 : sum > MAX_VOLUME ? MAX_VOLUME : (uint8_t)sum;
    p->paula[channel].period = period;
    p->paula[channel].volume = volume;
}

// The order list starts over; the loop of the layout's loop channel is the eagleplayer's SongEnd.

static void restart_order_list(PshPlayer* p, uint32_t channel) {
    PshVoice* v = &p->voices[channel];
    v->order_next = v->order_restart;
    if (channel == p->module.layout->loop_channel) {
        p->song_end = true;
    }
}

// The all-voice volume slide code sets the slide of every voice.

static void slide_all_volumes(PshPlayer* p, uint16_t target, uint16_t rate) {
    for (uint32_t i = 0; i < PSH_VOICES; i++) {
        p->voices[i].volume_target = target;
        p->voices[i].slide_rate = rate;
        p->voices[i].slide_counter = rate;
    }
}

// Order list entries up to the next pattern. The entries below 0xFFE8 are patterns (the top bit says that a repeat
// count follows); the others change the voice or the song and the list goes on. False when the voice stopped.

static bool read_order_list(PshPlayer* p, uint32_t channel) {
    const PshModule* m = &p->module;
    PshVoice* v = &p->voices[channel];
    for (int guard = 0; guard < PSH_COMMAND_LIMIT; guard++) {
        const uint16_t word = psh_u16(m, v->order_next);
        v->order_next += 2;
        if (word < PSH_ORDER_CODE_MIN) {
            v->pattern_repeats = 1;
            if ((word & PSH_ORDER_REPEATS_BIT) != 0) {
                v->pattern_repeats = psh_u16(m, v->order_next);
                v->order_next += 2;
            }
            v->pattern = word & PSH_ORDER_OFFSET_MASK;
            v->stream = v->pattern;
            return true;
        }
        PshOrderAction action;
        if (!psh_order_action(m->layout, word, &action)) {
            break; // the static walk refuses a song with such a word
        }
        switch (action) {
            case PshOrderAction_VolumeSlide:
                v->volume_target = psh_u16(m, v->order_next);
                v->slide_rate = psh_u16(m, v->order_next + 2);
                v->slide_counter = v->slide_rate;
                v->order_next += 4;
                break;
            case PshOrderAction_PitchOffset:
                v->note_offset = (int16_t)psh_u16(m, v->order_next);
                v->order_next += 2;
                break;
            case PshOrderAction_Loop:
                restart_order_list(p, channel);
                break;
            case PshOrderAction_Stop:
                return psh_stop_song(p, channel);
            case PshOrderAction_AllVolumeSlide:
                slide_all_volumes(p, psh_u16(m, v->order_next), psh_u16(m, v->order_next + 2));
                v->order_next += 4;
                break;
            case PshOrderAction_PitchTune:
                v->pitch_tune = (int16_t)psh_u16(m, v->order_next);
                v->order_next += 2;
                break;
        }
    }
    psh_stop_voice(v);
    return false;
}

// A voice that stopped is not stepped any more; its channel plays on.

void psh_stop_voice(PshVoice* v) {
    v->active = false;
}

// The pattern command "next pattern": the pattern plays again while it has repeats left, then the next entry of the
// voice's order list is read. False when the voice stopped.

bool psh_next_pattern(PshPlayer* p, uint32_t channel) {
    PshVoice* v = &p->voices[channel];
    v->pattern_repeats--;
    if (v->pattern_repeats != 0) {
        v->stream = v->pattern;
        return true;
    }
    return read_order_list(p, channel);
}

// The order list code "loop" run as a pattern command: the list starts over and its next entry is read.

bool psh_restart_order_list(PshPlayer* p, uint32_t channel) {
    restart_order_list(p, channel);
    return read_order_list(p, channel);
}

// The order list code "stop" run as a pattern command.

bool psh_stop_song(PshPlayer* p, uint32_t channel) {
    psh_stop_voice(&p->voices[channel]);
    p->song_end = true;
    return false;
}

// A note byte as semitones from C-0; the bytes below it play lower than C-0.

int psh_display_note(uint8_t note) {
    return note >= NOTE_C0_BIAS ? note - NOTE_C0_BIAS : 0;
}
