// SPDX-License-Identifier: MIT
// MusicMaker V8 player: the mixing routines of the 8 voice replayer for a 68000, translated from the UADE eagleplayer
// "MusicMaker-8V". A song says per channel pair (header byte 0x18) whether the 68020 routines (mm8_mix020.c) or these
// mix it.
//
// The voices' periods may be replaced by the channel period the song's table gives for them: every period up to the
// song's limit asks the channel for the table's period instead. The channel period is the smallest of the two, the
// block is as long as one tick at that period, and each voice goes through its data at the ratio of its own period to
// the channel period.
//
// The original mixes a block with code it writes first: a template of 64 samples whose pointer increments are patched
// for each voice. A voice that is not stepped moves on by one byte per sample; a stepped voice by the patched increment
// (0, 1 or 2 bytes), which the replayer lays out over the 64 samples with a Bresenham line. Here the increments are
// data (Mm8Step68k), built from the same arithmetic. A block of n samples starts at sample 64 - n % 64 of the chunk and
// then runs n / 64 whole chunks, so the pattern starts anew with every call. At the end of every chunk an accumulator
// adds the fraction the generator left; when it carries, the voices with a "carry advance" move on by one more byte.

#include "mm8_mix.h"

#define CHUNK 64
#define CHUNK_ROUND (1u << 16)  // 1.0 in the generators' 16.16 fractions
#define SLOW_CORRECTION_SHIFT 9 // the slow generator steps at (1 - 1/512) of the ratio
#define RATIO_FROM_COUNT 4      // the 8.8 ratio is four times the bytes of a chunk
#define UNIT_ROW_SHIFT 10       // the lone-voice ratio of the slow generator in 1/1024

// The period the voice asks the channel for.

static uint16_t mapped_period(const Mm8Block* b, int voice) {
    return mm_song_slow_period(b->e->core.song, b->ch->pair_period[voice]);
}

// The smaller of the two mapped periods (the first voice when they are equal). The accumulated fraction of a block
// length is rescaled with the voice's own period.

static uint16_t channel_period(const Mm8Block* b, uint16_t* raw_period) {
    const uint16_t ma = mapped_period(b, MM8_VOICE_A);
    const uint16_t mb = mapped_period(b, MM8_VOICE_B);
    *raw_period = ma > mb ? b->ch->pair_period[MM8_VOICE_B] : b->ch->pair_period[MM8_VOICE_A];
    return ma > mb ? mb : ma;
}

static bool defines_period(const Mm8Block* b, int voice) {
    return mapped_period(b, voice) == b->channel_period;
}

// The unsigned 16 bit division of the 68000: a quotient that does not fit leaves the dividend as it is, and the result
// holds the quotient in its low word. A division by zero (which traps) gives the dividend too.

static uint32_t divide(uint32_t dividend, uint32_t divisor) {
    divisor &= 0xFFFF;
    if (divisor == 0 || dividend / divisor > 0xFFFF) {
        return dividend;
    }
    return (dividend % divisor) << 16 | dividend / divisor;
}

// The quotient word of (a << shift) / b where the divisor is a word.

static uint16_t quotient(uint32_t a, int shift, uint32_t b) {
    return (uint16_t)divide(a << shift, b);
}

// The step of a voice that is stepped faster than one byte per sample is 2 * ((channel << 16) / (2 * own)): a ratio of
// 1 to 2 in 16.16. A voice that comes from the table has its own mapped period in the quotient, scaled to the channel
// period; the one that gives the channel period has not.

static uint32_t step_of(uint16_t channel, uint16_t mapped, uint16_t own) {
    uint16_t q = quotient(mapped, 16, (uint16_t)(2 * own));
    if (channel != mapped) {
        q = (uint16_t)divide((uint32_t)q * channel, mapped);
    }
    return 2u * q;
}

// A voice whose ratio is 1 or more: each sample moves it on by one byte, or by two when the error of the line goes
// negative. Fills the voice's step and ratio; returns the fraction for the chunk accumulator (none).

static uint16_t generate_fast(Mm8Step68k* s, uint32_t step) {
    int32_t error = (int32_t)(step >> 2);
    uint16_t count = 0;
    for (int j = 0; j < CHUNK; j++) {
        error = error - (int32_t)step + (int32_t)CHUNK_ROUND;
        if (error < 0) {
            error += (int32_t)CHUNK_ROUND;
            count += RATIO_FROM_COUNT;
            s->advance[j] = 2;
        } else {
            s->advance[j] = 1;
        }
    }
    s->stepped = true;
    s->carry_advance = false;
    s->ratio = (uint16_t)(MM8_UNIT_RATIO + count);
    return 0;
}

// A voice whose ratio is below 1 (`frac`, 16 bit): it moves on by one byte when the error of the line goes negative.
// The line runs a little slow (by 1/512); `aux` (the ratio in 1/1024, 0 for the voices that come from the table) lets
// the generator work out what is missing from a chunk: the ratio is raised, and the end of the chunk adds a byte when
// the accumulated fraction carries. Fills the voice's step and ratio; returns the fraction for the chunk accumulator.

static uint16_t generate_slow(Mm8Step68k* s, uint32_t frac, uint16_t aux) {
    const int32_t step = (int32_t)(frac - (frac >> SLOW_CORRECTION_SHIFT));
    int32_t error = (step >> 2) - step + (int32_t)CHUNK_ROUND;
    uint16_t count = 0;
    for (int j = 0; j < CHUNK; j++) {
        error -= step;
        if (error < 0) {
            error += (int32_t)CHUNK_ROUND;
            count += RATIO_FROM_COUNT;
            s->advance[j] = 1;
        } else {
            s->advance[j] = 0;
        }
    }
    s->stepped = true;
    s->carry_advance = false;
    s->ratio = count;
    if (aux == 0) {
        return 0;
    }
    // the count scaled by 1/ratio (the low word of the quotient)
    const uint16_t scaled = (uint16_t)divide((uint32_t)count << UNIT_ROW_SHIFT, aux);
    const uint16_t product = (uint16_t)((uint32_t)scaled * count);
    const uint16_t chunk_bytes = (uint16_t)(count << 8);
    if (chunk_bytes <= product) {
        return 0;
    }
    const uint16_t missing = (uint16_t)((chunk_bytes - product) << 6);
    const uint16_t extra = missing >> 14;
    if (extra != 0) {
        s->ratio = (uint16_t)(s->ratio + extra + 1);
    }
    s->carry_advance = true;
    return missing;
}

// A voice of the pair that is read at one byte per sample.

static void plain(Mm8Step68k* s) {
    *s = (Mm8Step68k) { .ratio = MM8_UNIT_RATIO };
}

// One voice of the pair as the generator sees it, sorted by the period it asks for.

typedef struct Role {
    int voice;
    uint16_t mapped;
    uint16_t own;
} Role;

// How the periods of the pair relate to the song's table.

typedef enum Pairing {
    Pairing_BothFromTable,    // both voices ask for a table period
    Pairing_TableVoiceFaster, // one does, and its table period is below the other voice's own period
    Pairing_TableVoiceSlower, // one does, and its table period is not below the other voice's own period
    Pairing_NeitherFromTable, // none does: the voice with the larger period is stepped
} Pairing;

static Pairing pairing_of(const Mm8Block* b, const uint16_t mapped[2], uint16_t limit) {
    const uint16_t raw_a = b->ch->pair_period[MM8_VOICE_A];
    const uint16_t raw_b = b->ch->pair_period[MM8_VOICE_B];
    const bool from_a = raw_a <= limit;
    const bool from_b = raw_b <= limit;
    if (from_a && from_b) {
        return Pairing_BothFromTable;
    }
    if (from_a) {
        return raw_b > mapped[MM8_VOICE_A] ? Pairing_TableVoiceFaster : Pairing_TableVoiceSlower;
    }
    if (from_b) {
        return raw_a > mapped[MM8_VOICE_B] ? Pairing_TableVoiceFaster : Pairing_TableVoiceSlower;
    }
    return Pairing_NeitherFromTable;
}

static void swap_roles(Role* a, Role* b) {
    const Role t = *a;
    *a = *b;
    *b = t;
}

// A voice stepped by the ratio of the channel period to its own period, with the generator for ratios above or below 1.
// Returns the fraction for the chunk accumulator.

static uint16_t generate_stepped(Mm8Step68k* s, uint32_t step) {
    return step < CHUNK_ROUND ? generate_slow(s, step, 0) : generate_fast(s, step);
}

// Lays out the stepping of the two voices (the replayer's generator front). The voices are taken in the order of the
// periods they ask for; the first one gives the channel period.

static void generate(const Mm8Block* b, Mm8Plan68k* p) {
    const uint16_t limit = b->e->core.song->slow_limit;
    Role first = { MM8_VOICE_A, p->mapped[MM8_VOICE_A], b->ch->pair_period[MM8_VOICE_A] };
    Role second = { MM8_VOICE_B, p->mapped[MM8_VOICE_B], b->ch->pair_period[MM8_VOICE_B] };
    const Pairing pairing = pairing_of(b, p->mapped, limit);
    if (second.mapped <= first.mapped) {
        swap_roles(&first, &second);
    }
    const uint16_t channel = first.mapped;
    switch (pairing) {
        case Pairing_BothFromTable:
            if (first.mapped == second.mapped && second.own <= first.own) {
                swap_roles(&first, &second);
            }
            generate_fast(&p->step[first.voice], step_of(channel, first.mapped, first.own));
            p->fraction = generate_stepped(&p->step[second.voice], step_of(channel, second.mapped, second.own));
            break;
        case Pairing_TableVoiceFaster:
            p->fraction = generate_slow(&p->step[second.voice], quotient(channel, 16, second.own),
                                        quotient(channel, UNIT_ROW_SHIFT, second.own));
            generate_fast(&p->step[first.voice], step_of(channel, first.mapped, first.own));
            break;
        case Pairing_TableVoiceSlower:
            if (second.own > limit) {
                swap_roles(&first, &second);
            }
            p->fraction = generate_stepped(&p->step[second.voice], step_of(channel, second.mapped, second.own));
            break;
        case Pairing_NeitherFromTable:
            p->fraction = generate_slow(&p->step[second.voice], quotient(first.own, 16, second.own),
                                        quotient(first.own, UNIT_ROW_SHIFT, second.own));
            break;
    }
}

// Whether the replayer writes a mixing routine for the pair at all. A voice that comes from the table needs one; so
// does a voice that is slower than the other of the pair. The period (and volume) of a voice that has no data is the
// one it had when its data ran out, if that happened during the block; before the block it is 0xFFFF.

static bool needs_generating(const Mm8Block* b, const uint16_t mapped[2]) {
    const uint16_t limit = b->e->core.song->slow_limit;
    const bool present[2] = { mm8_voice_present(b, MM8_VOICE_A), mm8_voice_present(b, MM8_VOICE_B) };
    for (int i = 0; i < 2; i++) {
        if (present[i] && b->ch->pair_period[i] <= limit) {
            return true;
        }
    }
    if (present[MM8_VOICE_A] && present[MM8_VOICE_B]) {
        return mapped[MM8_VOICE_A] != mapped[MM8_VOICE_B];
    }
    const int voice = present[MM8_VOICE_A] ? MM8_VOICE_A : MM8_VOICE_B;
    return mapped[voice] > mapped[1 - voice];
}

// The routine for the pair: the quieter voice is scaled, a voice that comes from the table (or the slower of the two)
// is stepped. Only the voices that have data are mixed.

static void select_routine(Mm8Block* b) {
    const Mm8Channel* ch = b->ch;
    Mm8Plan68k* p = &b->plan68k;
    *p = (Mm8Plan68k) { .scaled = -1 };
    plain(&p->step[MM8_VOICE_A]);
    plain(&p->step[MM8_VOICE_B]);
    for (int i = 0; i < 2; i++) {
        p->mapped[i] = mapped_period(b, i);
    }
    const uint16_t va = ch->pair_volume[MM8_VOICE_A];
    const uint16_t vb = ch->pair_volume[MM8_VOICE_B];
    b->row = mm8_volume_row(b->e, va, vb);
    if (va != vb) {
        p->scaled = va < vb ? MM8_VOICE_A : MM8_VOICE_B;
    }
    if (needs_generating(b, p->mapped)) {
        generate(b, p);
    }
}

// The bytes a voice takes from a block of n samples: the ratio of its pattern (a chunk's bytes), and one more.

static uint16_t stepped_bytes(uint16_t n, uint16_t ratio) {
    return (uint16_t)(((uint32_t)n * ratio >> 8) + 1);
}

// The bytes of each voice a block takes. The voice with the larger mapped period is counted by its ratio; the one with
// the smaller (which gives the channel period) only when it comes from the table.

static void needs(Mm8Block* b, uint16_t n) {
    const Mm8Plan68k* p = &b->plan68k;
    const uint16_t limit = b->e->core.song->slow_limit;
    const uint16_t raw_a = b->ch->pair_period[MM8_VOICE_A];
    const uint16_t raw_b = b->ch->pair_period[MM8_VOICE_B];
    const uint16_t ratio_a = p->step[MM8_VOICE_A].ratio;
    const uint16_t ratio_b = p->step[MM8_VOICE_B].ratio;
    if (p->mapped[MM8_VOICE_A] > p->mapped[MM8_VOICE_B]) {
        b->need[MM8_VOICE_A] = stepped_bytes(n, ratio_a);
        b->need[MM8_VOICE_B] = raw_b <= limit ? stepped_bytes(n, ratio_b) : n;
        return;
    }
    b->need[MM8_VOICE_A] = raw_a <= limit ? stepped_bytes(n, ratio_a) : n;
    if (raw_a != raw_b) {
        b->need[MM8_VOICE_B] = stepped_bytes(n, ratio_b);
    } else {
        b->need[MM8_VOICE_B] = raw_a <= limit ? b->need[MM8_VOICE_A] : n;
    }
}

// A ratio of 0 (a voice that is a 64th of the channel's speed) would divide by zero in the replayer.

static uint16_t consumption(const Mm8Block* b, int voice) {
    const uint16_t ratio = b->plan68k.step[voice].ratio;
    return ratio != 0 ? ratio : 1;
}

// Mixes `m` samples: the rest of the chunk the block enters, then whole chunks. The accumulator starts at 0 in every
// call.

static void run(Mm8Block* b, uint32_t m, uint32_t taken[2]) {
    const Mm8Plan68k* pl = &b->plan68k;
    const MmBlock* data = &b->e->core.inst->block;
    uint32_t ptr[2];
    bool present[2];
    for (int i = 0; i < 2; i++) {
        ptr[i] = b->ch->stream[i].ptr;
        present[i] = mm8_voice_present(b, i);
    }
    const uint32_t chunks = m / CHUNK;
    uint16_t accumulator = 0;
    uint32_t written = 0;
    for (uint32_t pass = 0; pass <= chunks; pass++) {
        for (uint32_t j = pass == 0 ? CHUNK - m % CHUNK : 0; j < CHUNK; j++) {
            int8_t sum = 0;
            for (int i = 0; i < 2; i++) {
                if (!present[i]) {
                    continue;
                }
                const uint8_t sample = mm_u8(data, ptr[i]);
                sum = (int8_t)(sum + (pl->scaled == i ? b->row[sample] : (int8_t)sample));
                ptr[i] += pl->step[i].stepped ? pl->step[i].advance[j] : 1u;
            }
            b->out[written++] = sum;
        }
        const uint32_t total = (uint32_t)accumulator + pl->fraction;
        accumulator = (uint16_t)total;
        if (total > 0xFFFF) {
            for (int i = 0; i < 2; i++) {
                if (present[i] && pl->step[i].carry_advance) {
                    ptr[i]++;
                }
            }
        }
    }
    b->out += m;
    for (int i = 0; i < 2; i++) {
        taken[i] = ptr[i] - b->ch->stream[i].ptr;
        b->ch->stream[i].ptr = ptr[i];
    }
}

const Mm8Mixer mm8_mixer_68k = {
    .channel_period = channel_period,
    .defines_period = defines_period,
    .needs = needs,
    .select = select_routine,
    .consumption = consumption,
    .run = run,
};
