// SPDX-License-Identifier: MIT
// MusicMaker V8 player: the software mixer of the 8 voice replayer, translated from the UADE eagleplayer
// "MusicMaker-8V".
//
// Two voices share a Paula channel. A block has as many samples as one tick lasts at the faster voice's period; the
// slower voice is stepped through its data at the ratio of the periods. The channel plays at the louder voice's volume:
// that voice's samples are copied, the other voice's go through a table that scales them by the ratio of the volumes. A
// voice that runs out of data in the middle of a block continues with its loop; without one the block goes on with the
// other voice alone. This file is the driver (block lengths, the voices' data, the splitting of a block where a voice
// runs short); mm8_mix020.c and mm8_mix68k.c are the two sets of routines that mix the samples, and the song chooses
// one for each channel pair.

#include "mm8_mix.h"
#include <string.h>

#define MIN_REMAINING 3 // a voice with this many bytes or fewer left restarts from its loop
#define ACCUM_ONE 0x10000
#define ACCUM_TWO 0x20000
#define SUB_BLOCK_GUARD 256
#define MIN_BLOCK 2 // a block has at least one word, which is all Paula can play

// Row v of the table: v / 64 of a sample (a positive sample s gives s * v / 64, a negative one -((-s - 1) * v / 64)).
// The samples are 7 bit, so the rows between are never read.

void mm8_build_tables(int8_t* tables) {
    memset(tables, 0, (size_t)MM_VOLUME_ROWS * MM_VOLUME_ROW_SIZE);
    for (int v = 0; v < MM_VOLUME_ROWS; v++) {
        int8_t* row = tables + v * MM_VOLUME_ROW_SIZE;
        for (int j = 0; j < MM_VOLUME_ROWS; j++) {
            const int scaled = j * v / MM_VOLUME_ROWS;
            row[j] = (int8_t)scaled;
            row[MM_VOLUME_ROW_SIZE - 1 - j] = (int8_t)-scaled;
        }
    }
}

// The row that scales the quieter of two volumes to the louder one.

const int8_t* mm8_volume_row(const Mm8* e, uint16_t va, uint16_t vb) {
    const uint16_t low = va < vb ? va : vb;
    const uint16_t high = va < vb ? vb : va;
    const uint32_t v = low < high ? (uint32_t)low * MM_VOLUME_ROWS / high : 0;
    return e->tables + v * MM_VOLUME_ROW_SIZE;
}

// The voice's data for a block: a voice that started a sample takes it (and the stepping starts over); one with few
// bytes left restarts from its loop.

static void load_stream(Mm8Stream* s, Mm8Voice* r, uint16_t* phase) {
    if (r->started) {
        s->ptr = r->sample;
        s->remaining = r->length;
        r->length = 0;
        *phase = 0;
    } else if (s->remaining <= MIN_REMAINING) {
        s->remaining = 0;
        s->ptr = r->loop_sample;
        if (s->ptr != 0) {
            s->remaining = r->loop_length;
        }
    }
}

// The length of a block in samples at the channel period. What does not fit is carried to the next blocks: the length
// is made even, and when the fractions add up to 2 samples the following block is shorter or longer by 2. The
// accumulated fraction is rescaled when the period of the voice that gives the channel period changes (by that voice's
// own period).

static uint16_t block_length(Mm8Channel* ch, uint32_t tempo_block, uint16_t period, uint16_t raw_period) {
    if (ch->last_period != 0 && ch->last_period != raw_period) {
        ch->accum = (uint32_t)(uint16_t)(ch->accum / raw_period) * ch->last_period;
    }
    ch->last_period = raw_period;
    uint16_t n = (uint16_t)(tempo_block / period);
    const uint16_t fraction = (uint16_t)(((tempo_block % period) << 16) / period);
    if (ch->carry != 0) {
        n = (uint16_t)(n + ch->carry);
        ch->carry = 0;
    }
    if (n & 1) {
        n++;
        ch->accum -= (uint16_t)~fraction;
    } else {
        ch->accum += fraction;
    }
    if ((int32_t)ch->accum < 0) {
        ch->accum += ACCUM_TWO;
        ch->carry = (uint16_t)(ch->carry - 2);
    } else if (ch->accum >= ACCUM_TWO) {
        ch->accum -= ACCUM_TWO;
        ch->carry = (uint16_t)(ch->carry + 2);
    }
    return n;
}

// The samples a voice can give before it runs out, as the replayer counts them (with the ratio rounded up).

static uint32_t samples_until_end(const Mm8Block* b, const Mm8Mixer* mixer, int voice) {
    const uint16_t consumption = mixer->consumption(b, voice);
    const uint32_t remaining = b->ch->stream[voice].remaining;
    return consumption == MM8_UNIT_RATIO ? remaining : (remaining << 8) / consumption;
}

// A voice takes its loop; returns whether it has one.

static bool load_loop(Mm8Stream* s, const Mm8Voice* r) {
    s->ptr = r->loop_sample;
    s->remaining = s->ptr != 0 ? r->loop_length : 0;
    return s->ptr != 0 && r->loop_length != 0;
}

// Voice x has run out of data. It takes its loop, and the other voice of the pair does too when it has few bytes left.
// Returns true when the block's routine has to be chosen anew (a voice without a loop is over for this block).

static bool voice_ended(Mm8Block* b, int x) {
    Mm8Channel* ch = b->ch;
    Mm8Stream* sx = &ch->stream[x];
    Mm8Stream* so = &ch->stream[1 - x];
    const Mm8Voice* rx = b->rec[x];
    const Mm8Voice* ro = b->rec[1 - x];
    if (!load_loop(sx, rx)) {
        sx->remaining = 0;
        if (so->remaining <= MIN_REMAINING) {
            load_loop(so, ro);
        }
        return true;
    }
    if (so->remaining > MIN_REMAINING) {
        return false;
    }
    so->ptr = ro->loop_sample;
    if (so->ptr != 0) {
        so->remaining = ro->loop_length;
        return false;
    }
    so->remaining = 0;
    if (sx->remaining <= MIN_REMAINING) {
        load_loop(sx, rx);
    }
    return true;
}

// Mixes the n samples of a block, splitting it where a voice runs out of data.

static void mix_samples(Mm8Block* b, const Mm8Mixer* mixer, uint32_t n) {
    Mm8Channel* ch = b->ch;
    uint32_t left = n;
    for (int guard = 0; guard < SUB_BLOCK_GUARD && left != 0; guard++) {
        const bool is_short[2]
            = { mm8_voice_present(b, MM8_VOICE_A) && b->need[MM8_VOICE_A] > ch->stream[MM8_VOICE_A].remaining,
                mm8_voice_present(b, MM8_VOICE_B) && b->need[MM8_VOICE_B] > ch->stream[MM8_VOICE_B].remaining };
        uint32_t taken[2];
        int ends = 0;
        uint32_t m = left;
        if (!is_short[0] && !is_short[1]) {
            ends = -1; // the rest of the block
        } else if (is_short[0] && is_short[1]) {
            const uint32_t ma = samples_until_end(b, mixer, MM8_VOICE_A);
            const uint32_t mb = samples_until_end(b, mixer, MM8_VOICE_B);
            const bool a_slow = !mixer->defines_period(b, MM8_VOICE_A);
            ends = a_slow ? (ma <= mb ? 0 : 1) : (mb <= ma ? 1 : 0);
            m = ends == 0 ? ma : mb;
        } else {
            ends = is_short[0] ? 0 : 1;
            m = samples_until_end(b, mixer, ends);
        }
        if (m > left) {
            m = left;
        }
        mixer->run(b, m, taken);
        left -= m;
        for (int i = 0; i < 2; i++) {
            const uint32_t remaining = ch->stream[i].remaining;
            ch->stream[i].remaining = (uint16_t)(taken[i] > remaining ? 0 : remaining - taken[i]);
            b->need[i] = (uint16_t)(b->need[i] - taken[i]);
        }
        if (ends >= 0 && voice_ended(b, ends)) {
            mixer->select(b);
        }
    }
}

// Nothing to play: a block of the tempo's length that is not written.

static void silent_block(Mm8* e, Mm8Channel* ch) {
    uint32_t words = (uint16_t)(mm8_silent_words(e->tempo) + ch->carry);
    if (words > e->silent_capacity) {
        words = e->silent_capacity;
    }
    ch->period = MM_SILENT_PERIOD;
    ch->volume = 0;
    ch->len_words = (uint16_t)(words != 0 ? words : 1);
    ch->carry = 0;
    ch->accum = ACCUM_ONE;
    ch->last_period = 0;
}

void mm8_mix_channel(Mm8* e, uint32_t c) {
    Mm8Channel* ch = &e->channel[c];
    const Mm8Mixer* mixer = e->core.song->pair_uses_020[c] ? &mm8_mixer_020 : &mm8_mixer_68k;
    Mm8Voice* voice[2]
        = { &e->voice[mm8_pair_voice_index(c, MM8_VOICE_A)], &e->voice[mm8_pair_voice_index(c, MM8_VOICE_B)] };
    Mm8Block b = { .e = e, .ch = ch, .rec = { voice[0], voice[1] }, .out = ch->buffer[0] };
    for (int i = 0; i < 2; i++) {
        load_stream(&ch->stream[i], voice[i], &ch->phase);
        const bool has = ch->stream[i].remaining != 0;
        ch->pair_period[i] = has ? voice[i]->period : MM8_NO_PERIOD;
        ch->pair_volume[i] = has ? voice[i]->volume : 0;
    }
    if (!mm8_voice_present(&b, MM8_VOICE_A) && !mm8_voice_present(&b, MM8_VOICE_B)) {
        silent_block(e, ch);
        return;
    }
    uint16_t raw_period;
    b.channel_period = mixer->channel_period(&b, &raw_period);
    uint16_t n = block_length(ch, (uint32_t)e->tempo * MM_BLOCK_PER_TEMPO, b.channel_period, raw_period);
    // The buffers are sized for the lowest period: a block stays inside them
    const uint32_t most = e->buffer_size & ~1u;
    n = n < MIN_BLOCK ? MIN_BLOCK : (n > most ? (uint16_t)most : n);
    ch->period = b.channel_period;
    ch->volume = ch->pair_volume[0] > ch->pair_volume[1] ? ch->pair_volume[0] : ch->pair_volume[1];
    ch->len_words = (uint16_t)(n >> 1);
    mixer->select(&b);
    mixer->needs(&b, n);
    mix_samples(&b, mixer, n);
}
