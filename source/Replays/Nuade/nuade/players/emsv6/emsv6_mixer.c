// SPDX-License-Identifier: MIT
// EMS v6 player: the software mixer of the eagleplayer (Jarno Paananen's mixing code), STEREO14 mode, 68020 routines.
//
// 32 voices are summed into two buffers of 16-bit words, the even ones left and the odd ones right. Each sample byte
// goes through a volume table (volume x signed byte / 63 / channels), and the voice position is a 16.16 fixed-point
// count of source bytes. The sums are split into a high byte stream (Paula volume 64) and the low six bits (volume 1)
// that Paula adds to it as a 14-bit value, and appended to four 32 kB ring buffers that Paula loops.

#include "emsv6_internal.h"
#include "nuade_base.h"
#include <stdlib.h>
#include <string.h>

#define VOLUME_ROWS 65
#define SAMPLE_VALUES 256
#define SPLIT_ENTRIES 0x8000 // one per pair of 16-bit values: the lowest bit is dropped
#define MIX_BUFFER_WORDS 4096
#define SOURCE_CLOCK 3579264 // the clock the voice step is made from
#define DEFAULT_BPM 125
#define MIN_ROOM 0x10 // fewer samples than this are not mixed

// A silent voice that has run off a loop shorter than its step steps back this often at most.
#define MAX_SILENT_LOOPS 1000000
// Loop restarts within one block of one voice, at most.
#define MAX_REBASES 4096

// The volume table: row `volume`, column = the sample byte. The magnitude is cut at every step, then negated.

static void build_volume_table(int16_t* table, int channels) {
    for (uint32_t volume = 0; volume < VOLUME_ROWS; volume++) {
        for (uint32_t byte = 0; byte < SAMPLE_VALUES; byte++) {
            const bool negative = (byte & 0x80) != 0;
            const uint32_t magnitude = negative ? 0x80 - (byte & 0x7F) : byte & 0x7F;
            uint32_t value = volume * (magnitude << 8) / 63;
            value /= (uint32_t)channels;
            table[volume * SAMPLE_VALUES + byte] = (int16_t)(negative ? -(int32_t)(uint16_t)value : (int32_t)value);
        }
    }
}

// The split table: a 16-bit sum -> its high byte and its low six bits, negated together for a negative sum.

static void build_split_table(Ems6SplitEntry* table) {
    for (uint32_t index = 0; index < SPLIT_ENTRIES; index++) {
        const uint16_t value = (uint16_t)(index << 1);
        uint8_t high;
        uint8_t low;
        if (value & 0x8000) {
            const uint16_t magnitude = (uint16_t)(0x10000 - value);
            high = (uint8_t)-(int)(magnitude >> 8);
            low = (uint8_t)-(int)((magnitude & 0xFF) >> 2);
        } else {
            high = (uint8_t)(value >> 8);
            low = (uint8_t)((value & 0xFF) >> 2);
        }
        table[index] = (Ems6SplitEntry) { .high = (int8_t)high, .low = (int8_t)low };
    }
}

bool e6_mixer_init(Ems6Mixer* mx, int channels) {
    *mx = (Ems6Mixer) { .channels = (uint16_t)(channels > 0 ? channels : 1) };
    mx->volume_table = calloc(VOLUME_ROWS * SAMPLE_VALUES, sizeof(int16_t));
    mx->split_table = calloc(SPLIT_ENTRIES, sizeof(Ems6SplitEntry));
    bool ok = mx->volume_table != nullptr && mx->split_table != nullptr;
    for (int i = 0; i < E6_SIDES; i++) {
        mx->mix[i] = calloc(MIX_BUFFER_WORDS, sizeof(int16_t));
        ok = ok && mx->mix[i] != nullptr;
    }
    for (int i = 0; i < E6_RINGS; i++) {
        mx->ring[i] = calloc(E6_RING_SIZE, 1);
        ok = ok && mx->ring[i] != nullptr;
    }
    if (!ok) {
        e6_mixer_free(mx);
        return false;
    }
    build_volume_table(mx->volume_table, mx->channels);
    build_split_table(mx->split_table);
    return true;
}

void e6_mixer_free(Ems6Mixer* mx) {
    free(mx->volume_table);
    free(mx->split_table);
    for (int i = 0; i < E6_SIDES; i++) {
        free(mx->mix[i]);
    }
    for (int i = 0; i < E6_RINGS; i++) {
        free(mx->ring[i]);
    }
    *mx = (Ems6Mixer) { 0 };
}

// Rounded up to an even count.
uint16_t e6_mixer_ticks_for_bpm(uint32_t bpm) {
    return (uint16_t)((E6_RATE * 5 / (bpm * 2) + 1) & 0xFFFE);
}

// InitSound's part of the mixer: empty rings and voices, the rate and the tick lengths.

void e6_mixer_reset(Ems6Mixer* mx) {
    for (int i = 0; i < E6_RINGS; i++) {
        memset(mx->ring[i], 0, E6_RING_SIZE);
    }
    memset(mx->voices, 0, sizeof(mx->voices));
    mx->write_pos = 0;
    mx->read_pos = 0;
    mx->master_volume = E6_MASTER_FULL;
    mx->seq_tick_samples = e6_mixer_ticks_for_bpm(DEFAULT_BPM);
    mx->until_seq_tick = 0;
    mx->per_tick = ((uint32_t)E6_RATE << 8) / E6_SEQUENCER_HZ;
}

// The writer fills the rings up to two interrupts behind the model of the read position, i.e. the whole lap minus a
// margin.

uint16_t e6_mixer_room(const Ems6Mixer* mx) {
    const int32_t read = (int32_t)(mx->read_pos >> 8);
    int32_t write = (int32_t)mx->write_pos;
    if (write > read) {
        write -= E6_RING_SIZE;
    }
    const int32_t margin = (int32_t)(mx->per_tick >> 7) + write;
    int32_t room = read - margin;
    if (room < 0) {
        return 0;
    }
    room += room & 1;
    return room < MIN_ROOM ? 0 : (uint16_t)room;
}

// The offset register is a signed word: after 32 kB bytes are read backwards.
static uint8_t sample_byte(const Ems6Module* m, uint32_t pointer, uint16_t whole) {
    return e6_module_u8(m, pointer + (uint32_t)(int32_t)(int16_t)whole);
}

// Source bytes per output sample as 16.16: (clock / period << 14) / (rate >> 4), two integer divisions.
static uint32_t voice_step(uint16_t period) {
    const uint32_t quotient = ((uint32_t)SOURCE_CLOCK / period) << 14;
    return quotient / (E6_RATE >> 4);
}

// The remaining length is a 68k long: a damaged module's length wraps it as the replayer's 32-bit arithmetic does.
static int32_t wrap_s32(uint32_t value) {
    return value <= INT32_MAX ? (int32_t)value : (int32_t)(value - 0x80000000u) + INT32_MIN;
}

static void zero_words(int16_t* out, uint32_t count) {
    memset(out, 0, count * sizeof(int16_t));
}

// A voice with volume 0 produces nothing but its position still moves on.

static void advance_silent(Ems6MixVoice* v, uint32_t samples) {
    const uint32_t position = v->position + voice_step(v->period) * samples;
    const uint32_t whole = position >> 16;
    v->pointer += whole;
    v->position = position & 0xFFFF;
    v->remaining = wrap_s32((uint32_t)v->remaining - whole);
    if (v->remaining >= 0) {
        return;
    }
    if (!v->looping) {
        v->stopped = true;
        return;
    }
    for (int guard = 0; guard < MAX_SILENT_LOOPS && v->remaining <= 0; guard++) {
        v->pointer -= v->loop_length;
        v->remaining = wrap_s32((uint32_t)v->remaining + v->loop_length);
    }
}

// The end of a block: the whole bytes are folded into the pointer. A voice that ran past its end wraps to the loop.

static void finish_block(Ems6MixVoice* v) {
    const uint32_t whole = v->position >> 16;
    v->pointer += whole;
    v->position &= 0xFFFF;
    v->remaining = wrap_s32((uint32_t)v->remaining - whole);
    if (v->remaining >= 0) {
        return;
    }
    if (!v->looping) {
        v->stopped = true;
        return;
    }
    // The pointer ends up a loop length before the loop, as the replayer leaves it.
    v->pointer = v->loop_pointer - v->loop_length;
    v->remaining = wrap_s32((uint32_t)v->remaining + v->loop_length);
}

// The remaining length as a 16.16 distance. A length over 64 kB is cut by replacing its low word only, so its high word
// ends up in the fraction.

static uint32_t remaining_16_16(uint32_t remaining) {
    const uint32_t cut = remaining > 0xFFFF ? (remaining & 0xFFFF0000) | 0xFFFF : remaining;
    return cut << 16 | cut >> 16;
}

// One output sample's step. The replayer adds the swapped long (fraction high, whole bytes low) and then the carry of
// that add (`addx`) to the whole bytes, so a carry out of the whole bytes also lands in the fraction.

static uint32_t step_position(uint32_t position, uint32_t step) {
    const uint32_t whole_carry = ((position >> 16) + (step >> 16)) >> 16;
    return position + step + whole_carry;
}

// Mix `count` samples of one voice into `out` (written for the first voice of a side, added for the others).

static void mix_voice(const Ems6Mixer* mx, const Ems6Module* m, Ems6MixVoice* v, int16_t* out, uint32_t count,
                      bool first) {
    if (v->period == 0 || v->stopped) {
        if (first) {
            zero_words(out, count);
        }
        return;
    }
    if (v->volume == 0) {
        advance_silent(v, count);
        if (first) {
            zero_words(out, count);
        }
        return;
    }
    const uint32_t step = voice_step(v->period);
    const uint32_t row_index = (uint16_t)((uint16_t)(v->volume * mx->master_volume) >> 6);
    const int16_t* row = row_index < VOLUME_ROWS ? mx->volume_table + row_index * SAMPLE_VALUES : mx->volume_table;
    uint32_t pointer = v->pointer;
    uint32_t position = v->position;
    uint32_t to_end = (uint32_t)v->remaining;
    uint32_t left = count;
    uint32_t written = 0;

    bool at_end = to_end == 0;
    for (int rebases = 0;; rebases++) {
        if (at_end) {
            // A loop shorter than the overshoot would restart over and over; the replayer would hang.
            if (!v->looping || rebases >= MAX_REBASES) {
                v->stopped = true;
                if (first) {
                    zero_words(out + written, left);
                }
                return;
            }
            const uint32_t overshoot = (position >> 16) - (uint32_t)v->remaining;
            pointer = v->loop_pointer + overshoot;
            to_end = v->loop_length - overshoot;
            v->remaining = (int32_t)to_end;
            position &= 0xFFFF;
        }
        // The samples until the position reaches the end of the sample, rounded up.
        const uint32_t distance = remaining_16_16(to_end) - position;
        uint32_t until_end = left;
        if (step != 0) {
            until_end = distance / step + (distance % step != 0 ? 1 : 0);
        }
        at_end = false;
        while (left != 0 && until_end != 0) {
            const uint32_t n = left < until_end ? left : until_end;
            for (uint32_t i = 0; i < n; i++) {
                const int16_t value = row[sample_byte(m, pointer, (uint16_t)(position >> 16))];
                position = step_position(position, step);
                out[written + i] = first ? value : (int16_t)(out[written + i] + value);
            }
            written += n;
            left -= n;
            until_end -= n;
        }
        if (left == 0) {
            v->pointer = pointer;
            v->position = position;
            finish_block(v);
            return;
        }
        at_end = true;
    }
}

// The first voice of the side overwrites the buffer, the others add to it. With one channel only the first voice of
// each side is mixed.
static void mix_side(Ems6Mixer* mx, const Ems6Module* m, E6Side side, uint32_t count) {
    bool first = true;
    for (int i = side; i < E6_TRACKS; i += E6_SIDES) {
        mix_voice(mx, m, &mx->voices[i], mx->mix[side], count, first);
        first = false;
        if (mx->channels == 1) {
            break;
        }
    }
}

static void append_to_rings(Ems6Mixer* mx, E6Side side, uint32_t count) {
    int8_t* high = mx->ring[side == E6Side_Left ? E6Ring_HighLeft : E6Ring_HighRight];
    int8_t* low = mx->ring[side == E6Side_Left ? E6Ring_LowLeft : E6Ring_LowRight];
    for (uint32_t i = 0; i < count; i++) {
        const Ems6SplitEntry split = mx->split_table[(uint16_t)mx->mix[side][i] >> 1];
        const uint32_t at = (mx->write_pos + i) & E6_RING_MASK;
        high[at] = split.high;
        low[at] = split.low;
    }
}

void e6_mixer_render_block(Ems6Mixer* mx, const Ems6Module* m, uint32_t count) {
    if (count >= MIX_BUFFER_WORDS) {
        count = MIX_BUFFER_WORDS - 1;
    }
    for (E6Side side = E6Side_Left; side < E6_SIDES; side++) {
        mix_side(mx, m, side, count);
        append_to_rings(mx, side, count);
    }
    mx->write_pos = (mx->write_pos + count) & E6_RING_MASK;
}
