// SPDX-License-Identifier: MIT
// Sound Master player: one voice. A note start sets the voice's sample, period and envelope; every tick the effects
// (arpeggio / wave sequence, vibrato, slide, sweep, envelope, fade) update the voice and write its Paula registers.

#include "nuade_tracker.h"
#include "sm_internal.h"

#define ARPEGGIO_KEEP 0x80
#define ARPEGGIO_NOTE 0x81
#define ARPEGGIO_HOLD 0x82
#define ROW_NONE 0xFF
#define NOTE_SLIDE 0x40
#define INFO_ABSOLUTE 0x80
#define INFO_VOLUME 0x40
#define INFO_KEEP_INSTRUMENT 0x40
#define INFO_VALUE_MASK 0x3F
#define INSTRUMENT_MASK 0x3F
#define WAVE_LOOP_START 0x80
#define WAVE_KEEP_BIT 0x20
#define ENVELOPE_DONE 0xFFFF
#define VIBRATO_DEPTH_LIMIT 0x40
#define NOTE_RANGE 0x2E
#define NOTE_BASE 0x12
#define NOTE_MASK 0x3F
#define PERIOD_SHIFT 2 // the voice keeps the period times four
#define PAULA_VOLUME_MASK 0x7F
#define SLIDE_SPEED_MASK 0x7F
#define ENVELOPE_SHIFT 4

// AUDxLC / AUDxLEN

static void write_sample(const SmstPlayer* p, SmstVoice* v, uint32_t address, uint16_t length_words) {
    v->paula.lc = smst_sample_pointer(&p->module, address);
    v->paula.len_words = length_words;
}

// The voice plays its loop; a voice without one plays the silent word.

void smst_write_loop_registers(const SmstPlayer* p, SmstVoice* v) {
    if (v->loop_length == 0) {
        write_sample(p, v, p->module.tables.dummy, 1);
    } else {
        write_sample(p, v, v->loop_start, v->loop_length);
    }
}

typedef struct NotePeriod {
    uint16_t period;
    uint8_t note; // the note the original keeps afterwards
} NotePeriod;

// The period of a note: the transposes, the instrument's note offset, the table. `transpose` is false for the
// arpeggio, which adds its own steps.

static NotePeriod note_period(const SmstPlayer* p, const SmstVoice* v, uint8_t note, bool transpose) {
    uint8_t index = note;
    uint8_t kept = note;
    if (transpose && !(v->info & INFO_ABSOLUTE)) {
        index = (uint8_t)(index + v->pattern_transpose + p->position_transpose);
        kept = index;
    }
    if (p->module.version->period_wide) {
        index = (uint8_t)((index + v->note_offset) & NOTE_MASK);
    } else {
        kept = index;
        index = (uint8_t)((uint8_t)(index + v->note_offset - NOTE_BASE) & NOTE_MASK);
        if (index >= NOTE_RANGE) {
            kept = (uint8_t)(NOTE_BASE - v->note_offset);
            index = 0;
        }
    }
    return (NotePeriod) { .period = smst_period(&p->module, index), .note = kept };
}

// Vibrato at the start of a note or when its delay ends: an odd period starts a half step lower.

static void vibrato_start(SmstVoice* v) {
    if (v->vib_period & 1) {
        v->period = (uint16_t)(v->period - (v->vib_depth >> 1));
    }
}

// A row without a note sets the envelope from the info byte.

static void rest_volume(SmstVoice* v, uint8_t note) {
    if (!(note & NOTE_SLIDE) && (v->info & INFO_VOLUME)) {
        v->env = (uint16_t)((v->info & INFO_VALUE_MASK) << ENVELOPE_SHIFT);
        v->env_attack = ENVELOPE_DONE;
    }
}

// The instrument a note plays: the info byte plus the position's offset; a slide note and an info byte with bit 6 keep
// the voice's last instrument.

static uint8_t pick_instrument(SmstVoice* v, uint8_t note) {
    uint8_t number = v->instrument;
    if (!(note & NOTE_SLIDE) && !(v->info & INFO_KEEP_INSTRUMENT)) {
        number = v->info;
    }
    v->instrument = number;
    return (uint8_t)((number + v->position_instrument) & INSTRUMENT_MASK);
}

// A slide note: the voice moves from its period towards the note's at the info byte's speed. Returns the note the
// original keeps.

static uint8_t slide_start(const SmstPlayer* p, SmstVoice* v, uint8_t note) {
    const NotePeriod target = note_period(p, v, note, true);
    v->last_note = target.note;
    const uint16_t goal = (uint16_t)(target.period << PERIOD_SHIFT);
    v->slide_target = goal;
    const bool up = (int16_t)(goal - v->period) >= 0;
    const int16_t speed = (int16_t)(v->info & SLIDE_SPEED_MASK);
    v->slide_step = up ? speed : (int16_t)-speed;
    return target.note;
}

// The sample and the envelope of a new note.

static void start_sample(SmstPlayer* p, SmstVoice* v, const SmstInstrument* ins, uint8_t note) {
    const SmstModule* m = &p->module;
    const uint8_t wave = ins->arpeggio_length != 0 ? smst_u8(m, m->tables.wave + ins->arpeggio_start) : ins->wave;
    const SmstSlot slot = smst_slot(m, wave);
    write_sample(p, v, slot.sample, slot.length);
    v->loop_length = slot.loop_length;
    v->loop_start = slot.sample + slot.loop_offset;
    bool in_loop = false;
    switch (m->version->loop_start) {
        case SmstLoopStart_Never:
            break;
        case SmstLoopStart_Instrument:
            in_loop = (ins->wave & WAVE_LOOP_START) != 0;
            break;
        case SmstLoopStart_SelectedWave:
            in_loop = (wave & WAVE_LOOP_START) != 0;
            break;
    }
    if (in_loop) {
        write_sample(p, v, v->loop_start, v->loop_length);
    }
    uint8_t peak = ins->peak;
    if (!(note & NOTE_SLIDE) && (v->info & INFO_VOLUME)) {
        peak = v->info & INFO_VALUE_MASK;
    }
    v->env_peak = (uint16_t)(peak << ENVELOPE_SHIFT);
    v->env_sustain = (uint16_t)(ins->sustain << ENVELOPE_SHIFT);
    v->env = 0;
    v->env_attack = ins->attack;
    v->env_decay = ins->decay;
    v->release = ins->release;
    v->arp_pos = ins->arpeggio_start;
    v->vib_delay = ins->vibrato_delay;
    v->vib_depth = ins->vibrato_depth;
    v->vib_period = ins->vibrato_speed & 0x0F;
    v->vib_count = (uint8_t)(v->vib_period >> 1);
    v->vib_step = (uint8_t)(ins->vibrato_speed >> 4);
    if (v->vib_count != 0) {
        v->vib_dir = 1;
    }
    v->arp_length = ins->arpeggio_length;
    v->arp_repeat = ins->arpeggio_repeat;
    v->arp_note = ins->arpeggio_note;
    v->note_offset = ins->note_offset;
    if (m->version->sweep) {
        v->sweep_period = ins->sweep_period;
        v->sweep_count = ins->sweep_period;
        v->sweep_mask = ins->sweep_step & 0x0F;
        v->sweep_step = (int16_t)-(int16_t)(ins->sweep_step >> 4);
    }
}

void smst_note_start(SmstPlayer* p, SmstVoice* v) {
    const SmstVersion* ver = p->module.version;
    uint8_t note = v->note;
    if (note == 0 || note == ROW_NONE) {
        rest_volume(v, note);
        return;
    }
    v->slide_step = 0;
    if (note & NOTE_RELEASE) {
        if (ver->release_picks_instrument) {
            pick_instrument(v, note);
        }
        v->vib_count = (uint8_t)(v->vib_period >> 1);
        v->vib_dir = 1;
    } else {
        v->vib_dir = 0;
        const uint8_t number = pick_instrument(v, note);
        const SmstInstrument ins = smst_instrument(&p->module, number);
        start_sample(p, v, &ins, note);
        v->shown_instrument = number;
    }
    if (note & NOTE_SLIDE) {
        note = slide_start(p, v, note);
    } else {
        const NotePeriod np = note_period(p, v, note, true);
        note = np.note;
        v->last_note = np.note;
        v->paula.period = np.period;
        v->period = (uint16_t)(np.period << PERIOD_SHIFT);
        v->shown_note = nuade_period_to_note(np.period);
    }
    if (v->vib_delay == 0) {
        vibrato_start(v);
    }
    if (ver->dma_flow == SmstDmaFlow_RowThenNote) {
        v->dma_restart = true;
    }
    if (ver->rest_after_release && (v->note & NOTE_RELEASE)) {
        rest_volume(v, note);
    }
}

// The arpeggio table steps one entry a tick, the wave table next to it follows: a new value changes the loop (with bit
// 7 the sample itself, and the DMA starts over next tick). Returns the step.

static uint8_t arpeggio_step(SmstPlayer* p, SmstVoice* v) {
    const SmstModule* m = &p->module;
    const SmstTables* t = &m->tables;
    const uint8_t step = smst_u8(m, t->arpeggio + v->arp_pos);
    uint8_t pos = (uint8_t)(v->arp_pos + 1);
    if (pos == v->arp_length) {
        pos = v->arp_repeat;
    }
    v->arp_pos = pos;
    const uint8_t wave = smst_u8(m, t->wave + pos);
    if (wave & WAVE_LOOP_START) {
        v->dma_off_next = true;
        const SmstSlot slot = smst_slot(m, wave);
        v->loop_length = slot.length;
        v->loop_start = slot.sample;
    } else if ((!m->version->wave_repeat_check || wave != smst_u8(m, t->wave + pos - 1u))
               && !(m->version->wave_skip_bit5 && (wave & WAVE_KEEP_BIT))) {
        const SmstSlot slot = smst_slot(m, wave);
        v->loop_length = slot.loop_length;
        v->loop_start = slot.sample + slot.loop_offset;
    }
    return step;
}

// One vibrato step: the period moves by the depth, up or down; the direction flips every half period.

static void vibrato_step(const SmstVersion* ver, SmstVoice* v) {
    uint16_t release = 0;
    if (ver->release && v->env != 0) {
        release = v->release;
    }
    if (v->vib_dir > 0) {
        if ((v->vib_period >> 1) == v->vib_count && (!ver->vibrato_limit || v->vib_depth <= VIBRATO_DEPTH_LIMIT)) {
            v->vib_depth = (uint8_t)(v->vib_depth + v->vib_step);
        }
        v->period = (uint16_t)(v->period - v->vib_depth);
        v->env = (uint16_t)(v->env - release);
    } else {
        v->period = (uint16_t)(v->period + v->vib_depth);
        v->env = (uint16_t)(v->env + release);
    }
    v->vib_count--;
    if (v->vib_count == 0) {
        v->vib_dir = (int8_t)-v->vib_dir;
        v->vib_count = v->vib_period;
    }
}

// Vibrato (after its delay), then the slide towards the target period, then the period register.

static void vibrato_and_slide(const SmstVersion* ver, SmstVoice* v) {
    if (v->vib_dir != 0) {
        if (v->vib_delay != 0) {
            v->vib_delay--;
            if (v->vib_delay == 0) {
                vibrato_start(v);
            }
        } else {
            vibrato_step(ver, v);
        }
    }
    v->paula.period = (uint16_t)(v->period >> PERIOD_SHIFT);
    if (v->slide_step == 0) {
        return;
    }
    const uint16_t next = (uint16_t)(v->period + v->slide_step);
    const bool below = (int16_t)(next - v->slide_target) < 0;
    const bool arrived = v->slide_step < 0 ? below : !below;
    if (!arrived) {
        v->period = next;
        v->paula.period = (uint16_t)(next >> PERIOD_SHIFT);
        return;
    }
    v->slide_step = 0;
    v->vib_count = (uint8_t)(v->vib_period >> 1);
    v->vib_dir = 1;
    v->period = v->slide_target;
    v->paula.period = (uint16_t)(v->slide_target >> PERIOD_SHIFT);
    vibrato_start(v);
}

// The sweep moves the loop start every few ticks and turns round after a number of steps.

static void sweep(SmstVoice* v) {
    if ((v->sweep_count & v->sweep_mask) == 0) {
        v->loop_start += (uint32_t)(int32_t)v->sweep_step;
    }
    v->sweep_count--;
    if (v->sweep_count == 0) {
        v->sweep_step = (int16_t)-v->sweep_step;
        v->sweep_count = v->sweep_period;
    }
}

// The volume envelope: a held note attacks to the peak, a row without a note decays to the sustain.

static void envelope(SmstVoice* v) {
    uint16_t level = v->env;
    if (v->note == 0) {
        v->env_attack = ENVELOPE_DONE;
        if (level > v->env_sustain) {
            level = (uint16_t)(level - v->env_decay);
            if ((int16_t)level < 0 || level <= v->env_sustain) {
                level = v->env_sustain;
            }
            v->env = level;
        }
        return;
    }
    if (v->env_attack == 0) {
        v->env = v->env_peak;
        v->env_attack = ENVELOPE_DONE;
        return;
    }
    if (v->env_attack == ENVELOPE_DONE) {
        return;
    }
    level = (uint16_t)(level + v->env_attack);
    if (level >= v->env_peak) {
        level = v->env_peak;
        v->env_attack = ENVELOPE_DONE;
    }
    v->env = level;
}

// The fade factor of a voice, 64 = full. With per-voice fades the voice takes a step on the tick the fade counter
// reads 1, unless the position's byte exempts it.

static uint8_t fade_factor(SmstPlayer* p, SmstVoice* v) {
    if (!p->module.version->voice_fade) {
        return (v->position_instrument & VOICE_NO_FADE) ? FADE_FULL : p->fade;
    }
    uint8_t fade = v->fade;
    if (p->fade_count == 1 && !(v->position_instrument & VOICE_NO_FADE)) {
        if (p->fade_control & FADE_UP) {
            if (fade != FADE_FULL) {
                fade++;
            }
        } else if (fade != 0) {
            fade--;
        }
        v->fade = fade;
    }
    return fade;
}

static void write_volume(SmstPlayer* p, SmstVoice* v) {
    uint16_t volume = (uint16_t)(v->env >> ENVELOPE_SHIFT);
    const uint8_t fade = fade_factor(p, v);
    if (fade != FADE_FULL) {
        volume = (uint16_t)((uint16_t)(volume * fade) >> 6);
    }
    v->paula.volume = (uint8_t)(volume & PAULA_VOLUME_MASK);
}

// Every tick; the sweep stops for the "hold" arpeggio step.

void smst_voice_effects(SmstPlayer* p, SmstVoice* v, uint8_t* step) {
    const SmstVersion* ver = p->module.version;
    bool arpeggio_pitch = false;
    if (v->arp_length != 0) {
        *step = arpeggio_step(p, v);
        if (*step != ARPEGGIO_KEEP && !(ver->arpeggio_hold && *step == ARPEGGIO_HOLD)) {
            uint8_t note = v->arp_note;
            if (*step != ARPEGGIO_NOTE) {
                v->last_note = (uint8_t)(v->last_note + *step);
                note = v->last_note;
            }
            const NotePeriod np = note_period(p, v, note, false);
            if (ver->period_sets_last_note) {
                v->last_note = np.note;
            }
            v->paula.period = np.period;
            arpeggio_pitch = true;
        }
    }
    if (!arpeggio_pitch) {
        vibrato_and_slide(ver, v);
    }
    if (ver->sweep && v->loop_length != 0 && v->sweep_period != 0 && !(ver->arpeggio_hold && *step == ARPEGGIO_HOLD)) {
        sweep(v);
    }
    envelope(v);
    write_volume(p, v);
}
