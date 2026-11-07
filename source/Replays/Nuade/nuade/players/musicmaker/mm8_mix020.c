// SPDX-License-Identifier: MIT
// MusicMaker V8 player: the mixing routines of the 8 voice replayer for a 68020, translated from the UADE eagleplayer
// "MusicMaker-8V".
//
// The stepped voice is read through a 16 bit fraction of the ratio of the periods: each of its samples is held for as
// many output samples as the fraction says, and the fraction carries from one block to the next.

#include "mm8_mix.h"

#define RATIO_ROUND_UP 4
#define PERIOD_SIGN 0x8000 // a period that a signed word reads as negative: no voice

// The period of the faster voice (the first of two equal ones).

static uint16_t channel_period(const Mm8Block* b, uint16_t* raw_period) {
    const uint16_t pa = b->ch->pair_period[MM8_VOICE_A];
    const uint16_t pb = b->ch->pair_period[MM8_VOICE_B];
    *raw_period = pa <= pb ? pa : pb;
    return *raw_period;
}

static bool defines_period(const Mm8Block* b, int voice) {
    return b->ch->pair_period[voice] == b->channel_period;
}

// The step of the stepped voice per sample (a 16 bit fraction): the smaller period over the larger.

static uint16_t step_of(uint16_t pa, uint16_t pb) {
    const uint16_t small = pa < pb ? pa : pb;
    const uint16_t large = pa < pb ? pb : pa;
    return (uint16_t)(((uint32_t)small << 16) / large);
}

// What a block uses to count how much of the stepped voice it takes: the step in 8.8, rounded up.

static uint16_t consumption(const Mm8Block* b, int voice) {
    if (defines_period(b, voice)) {
        return MM8_UNIT_RATIO;
    }
    const uint16_t step = step_of(b->ch->pair_period[MM8_VOICE_A], b->ch->pair_period[MM8_VOICE_B]);
    return (uint16_t)((step >> 8) + ((step & 0xFF) != 0 ? RATIO_ROUND_UP : 0));
}

// The bytes of a voice with `period` that a block of n samples at the channel period takes: the stepped voice's
// samples, and one more.

static uint16_t bytes_needed(uint16_t n, uint16_t channel, uint16_t period) {
    if (period == channel || period >= PERIOD_SIGN) {
        return n;
    }
    const uint16_t q = (uint16_t)(((uint32_t)channel << 16) / period);
    return (uint16_t)(((uint32_t)n * q >> 16) + 1);
}

static void needs(Mm8Block* b, uint16_t n) {
    for (int i = 0; i < 2; i++) {
        b->need[i] = bytes_needed(n, b->channel_period, b->ch->pair_period[i]);
    }
}

// The replayer's choice of routine. One voice is stepped when its period is above the other's, and scaled when the
// other is louder (a voice that has no data has period 0xFFFF and volume 0). Two voices: the quieter is scaled, the
// slower is stepped; with equal periods both are copied, with equal volumes none is scaled.

static void select_routine(Mm8Block* b) {
    const Mm8Channel* ch = b->ch;
    const uint16_t pa = ch->pair_period[MM8_VOICE_A];
    const uint16_t pb = ch->pair_period[MM8_VOICE_B];
    const uint16_t va = ch->pair_volume[MM8_VOICE_A];
    const uint16_t vb = ch->pair_volume[MM8_VOICE_B];
    const bool has_a = mm8_voice_present(b, MM8_VOICE_A);
    const bool has_b = mm8_voice_present(b, MM8_VOICE_B);
    Mm8Plan020 p = { .step = step_of(pa, pb) };
    b->row = mm8_volume_row(b->e, va, vb);
    if (!has_a && !has_b) {
        b->plan020 = p;
        return;
    }
    p.use_x = true;
    if (!has_a || !has_b) {
        const bool x_is_a = has_a;
        p.x = x_is_a ? MM8_VOICE_A : MM8_VOICE_B;
        p.x_slow = x_is_a ? pa > pb : pb > pa;
        p.x_table = x_is_a ? vb > va : va > vb;
        b->plan020 = p;
        return;
    }
    p.use_y = true;
    const bool a_slower = pa > pb;
    if (pa == pb) {
        p.x = va <= vb ? MM8_VOICE_A : MM8_VOICE_B;
        p.x_table = va != vb;
    } else if (va == vb) {
        p.x = a_slower ? MM8_VOICE_A : MM8_VOICE_B;
        p.x_slow = true;
    } else {
        const bool a_weaker = va < vb;
        p.x = a_weaker ? MM8_VOICE_A : MM8_VOICE_B;
        p.x_table = true;
        if (a_weaker == a_slower) {
            p.x_slow = true;
        } else {
            p.y_slow = true;
        }
    }
    p.y = 1 - p.x;
    b->plan020 = p;
}

// The next byte of a voice, through the volume table when the voice is the scaled one.

static int8_t fetch(const MmBlock* data, uint32_t* ptr, const int8_t* row) {
    const uint8_t sample = mm_u8(data, (*ptr)++);
    return row != nullptr ? row[sample] : (int8_t)sample;
}

// Mixes `m` samples into the block. Reports the bytes taken from each voice (the stepped voice is read one sample
// ahead; the replayer's exit routine takes that back).

static void run(Mm8Block* b, uint32_t m, uint32_t taken[2]) {
    const Mm8Plan020* pl = &b->plan020;
    Mm8Channel* ch = b->ch;
    const MmBlock* data = &b->e->core.inst->block;
    const int8_t* x_row = pl->x_table ? b->row : nullptr;
    Mm8Stream* xs = &ch->stream[pl->x];
    Mm8Stream* ys = &ch->stream[pl->y];
    uint32_t px = xs->ptr;
    uint32_t py = ys->ptr;
    uint16_t phase = ch->phase;
    int8_t held = 0;
    taken[0] = 0;
    taken[1] = 0;
    if (pl->use_x && pl->x_slow) {
        held = fetch(data, &px, x_row);
    } else if (pl->use_y && pl->y_slow) {
        held = fetch(data, &py, nullptr);
    }
    for (uint32_t n = 0; n < m; n++) {
        int8_t o = 0;
        if (pl->use_x && pl->use_y) {
            if (pl->x_slow) {
                o = (int8_t)(fetch(data, &py, nullptr) + held);
            } else if (pl->y_slow) {
                o = (int8_t)(fetch(data, &px, x_row) + held);
            } else {
                const int8_t sx = fetch(data, &px, x_row);
                o = (int8_t)(sx + fetch(data, &py, nullptr));
            }
        } else if (pl->use_x) {
            o = pl->x_slow ? held : fetch(data, &px, x_row);
        }
        b->out[n] = o;
        if (pl->x_slow || pl->y_slow) {
            const uint32_t sum = (uint32_t)phase + pl->step;
            phase = (uint16_t)sum;
            if (sum > 0xFFFF) {
                held = pl->x_slow ? fetch(data, &px, x_row) : fetch(data, &py, nullptr);
            }
        }
    }
    b->out += m;
    if (pl->use_x && pl->x_slow) {
        px--;
    }
    if (pl->use_y && pl->y_slow) {
        py--;
    }
    ch->phase = phase;
    if (pl->use_x) {
        taken[pl->x] = px - xs->ptr;
        xs->ptr = px;
    }
    if (pl->use_y) {
        taken[pl->y] = py - ys->ptr;
        ys->ptr = py;
    }
}

const Mm8Mixer mm8_mixer_020 = {
    .channel_period = channel_period,
    .defines_period = defines_period,
    .needs = needs,
    .select = select_routine,
    .consumption = consumption,
    .run = run,
};
