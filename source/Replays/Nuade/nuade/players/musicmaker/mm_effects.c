// SPDX-License-Identifier: MIT
// MusicMaker V8 player: what a voice's note does every tick (slides, glides, bends, the note's table, the note and bend
// dependent volume), translated from the UADE eagleplayers "MusicMaker-4V" and "MusicMaker-8V".

#include "mm_core.h"

#define TABLE_BYTES_PER_ENTRY 40 // an entry of the note's table (a pair of bytes) covers 40 bytes of the sample
#define VOLUME_MAX 64
#define VOLUME2_MAX (2 * VOLUME_MAX)
#define BEND_DOWN_MAX 0x1000
#define FX_SCALE_SHIFT 7

static int32_t clamp(int32_t value, int32_t low, int32_t high) {
    return value < low ? low : (value > high ? high : value);
}

// A new sample: the note's table (if any) starts from its beginning.

static void start_sample(MmCore* c, MmVoice* t, const MmStep* step) {
    t->table_active = false;
    t->table_loop = false;
    t->table = step->slot < MM_TABLE_SLOTS ? c->slot_table[step->slot] : 0;
    if (t->table != 0) {
        t->table_active = true;
        t->table_phase = 0;
        // With 4 voices the limit is the length in words, doubled
        const uint32_t length = c->variant == MmVariant_4V ? step->length / 2 * 2 : step->length;
        t->table_limit = (uint16_t)length;
    }
}

// The pitch bends. With 4 voices they keep the scale between 0x100 and 0x1000 going down and up to 0x2000 going up for
// as long as the voice's bend lasts; with 8 voices a bend ends when the scale reaches its limit.

static void bend_pitch(const MmCore* c, MmVoice* t) {
    const bool one_shot = c->variant == MmVariant_8V;
    if (t->bend_down) {
        if (t->pitch_scale > BEND_DOWN_MAX) {
            t->pitch_scale = (uint16_t)(BEND_DOWN_MAX - c->bend_down_step);
        } else if ((int16_t)t->pitch_scale <= MM_UNIT_SCALE) {
            t->pitch_scale = MM_UNIT_SCALE;
            t->bend_down = !one_shot;
        } else {
            t->pitch_scale = (uint16_t)(t->pitch_scale - c->bend_down_step);
        }
    } else if (t->bend_up) {
        if ((int16_t)t->pitch_scale >= MM_PITCH_SCALE_MAX) {
            t->pitch_scale = MM_PITCH_SCALE_MAX;
            t->bend_up = !one_shot;
        } else {
            t->pitch_scale = (uint16_t)(t->pitch_scale + c->bend_up_step);
        }
    }
}

// The volume register: the twice-volume with its glide and slide, then the note table's tremolo, the note and bend
// dependent divisions.

static uint8_t update_volume(MmVoice* t, int8_t table_tremolo) {
    MmEffects* fx = &t->fx;
    if (fx->fade_step != 0) {
        int32_t change = (int32_t)(((uint32_t)fx->fade_step * fx->counter) >> FX_SCALE_SHIFT);
        if (fx->fade_down) {
            change = -change;
        }
        t->volume2 = (uint16_t)(2 * (fx->volume_base + change));
    }
    if (fx->mode > 0) {
        fx->toggle = !fx->toggle;
        if (!fx->toggle) {
            fx->volume_slide = (int16_t)-fx->volume_slide;
        }
    }
    t->volume2 = (uint16_t)clamp((int16_t)(t->volume2 + fx->volume_slide), 0, VOLUME2_MAX);
    int32_t volume = t->volume2 >> 1;
    if (t->table_active && table_tremolo != 0) {
        volume = clamp(volume + ((table_tremolo * volume) >> 5), 0, VOLUME_MAX);
    }
    if (t->volume_by_note) {
        volume = (volume << 8) / mm_note_volume_divisor[(t->note2 >> 1) % MM_NOTES];
    }
    if (t->pitch_scale != MM_UNIT_SCALE) {
        volume = (volume << 8) / t->pitch_scale;
    }
    return (uint8_t)clamp(volume, 0, VOLUME_MAX);
}

// The period register: the glide or the slides on the period, then the note table's vibrato, which also advances the
// table's phase.

static uint16_t update_period(const MmCore* c, MmVoice* t, int8_t table_vibrato) {
    MmEffects* fx = &t->fx;
    int32_t period = t->period;
    if (fx->glide_step != 0) {
        int32_t change = (int32_t)(((uint32_t)fx->glide_step * fx->counter) >> FX_SCALE_SHIFT);
        if (fx->glide_down) {
            change = -change;
        }
        period = (int16_t)(fx->glide_base + change);
    }
    if (fx->mode < 0) {
        fx->toggle = !fx->toggle;
        if (!fx->toggle) {
            fx->slide = (int16_t)-fx->slide;
        }
    }
    period = clamp((int16_t)(period + fx->slide), MM_PERIOD_MIN, MM_PERIOD_MAX);
    t->period = (uint16_t)period;
    if (t->table_active) {
        if (table_vibrato != 0) {
            period = clamp(period + ((period * table_vibrato) >> 8), MM_PERIOD_MIN, MM_PERIOD_MAX);
        }
        const uint32_t advance = (uint32_t)c->tempo_base * MM_BLOCK_PER_TEMPO / (uint32_t)period;
        const uint32_t phase = (uint32_t)t->table_phase + advance;
        t->table_phase = (uint16_t)phase;
        if (phase > 0xFFFF || t->table_phase > t->table_limit) {
            if (t->table_loop) {
                t->table_phase = (uint16_t)(t->table_phase - t->table_loop_length);
            } else {
                t->table_active = false;
            }
        }
    }
    fx->counter++;
    return (uint16_t)period;
}

void mm_core_update(MmCore* c, uint32_t voice, const MmStep* step) {
    MmVoice* t = &c->voice[voice];
    if (step->kind == MmStepKind_Note) {
        if (step->has_sample) {
            start_sample(c, t, step);
        }
        t->volume2 = (uint16_t)(2 * step->volume);
        t->period = step->period;
        t->note2 = step->note2;
    } else if (step->kind == MmStepKind_PitchScale) {
        t->pitch_scale = step->pitch_scale;
    }
    bend_pitch(c, t);
    int8_t tremolo = 0;
    int8_t vibrato = 0;
    if (t->table_active) {
        const uint32_t entry = t->table + (uint32_t)(t->table_phase / TABLE_BYTES_PER_ENTRY) * 2;
        tremolo = (int8_t)mm_u8(&c->inst->block, entry);
        vibrato = (int8_t)mm_u8(&c->inst->block, entry + 1);
    }
    t->out_volume = update_volume(t, tremolo);
    t->out_period = update_period(c, t, vibrato);
}
