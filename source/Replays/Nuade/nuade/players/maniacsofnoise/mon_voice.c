// SPDX-License-Identifier: MIT
// Maniacs of Noise player: the parts of a voice's tick that the versions of the replayer share.

#include "mon_internal.h"

// The period table entry times the tuning, shifted by 10: the note-on does it with two long shifts, the arpeggio with a
// long shift and a word shift, which drops the bits that the long shift leaves above bit 15.
#define PERIOD_SHIFT_HIGH 8
#define PERIOD_SHIFT_LOW 2
#define VOLUME_SHIFT 6
#define VIBRATO_SHIFT 7
#define SILENT_BUFFER_WORDS 4

bool mon_is_old_version(MonVersion version) {
    return version <= MonVersion_FlimbosQuest;
}

uint16_t mon_scaled_period(const MonModule* m, uint8_t note, uint16_t tuning) {
    return (uint16_t)(((uint32_t)mon_period_entry(m, note) * tuning) >> (PERIOD_SHIFT_HIGH + PERIOD_SHIFT_LOW));
}

uint16_t mon_scaled_period_word(const MonModule* m, uint8_t note, uint16_t tuning) {
    const uint16_t shifted = (uint16_t)(((uint32_t)mon_period_entry(m, note) * tuning) >> PERIOD_SHIFT_HIGH);
    return (uint16_t)(shifted >> PERIOD_SHIFT_LOW);
}

// The AUDxVOL of an envelope byte: scaled by the voice's volume and by the song volume, each with a word shift.

uint16_t mon_envelope_level(const MonPlayer* p, const MonVoice* v, uint8_t entry) {
    const uint16_t volume = (uint16_t)(v->volume_high << 8 | v->volume);
    const uint16_t voice_level = (uint16_t)((uint16_t)(entry * volume) >> VOLUME_SHIFT);
    return (uint16_t)((uint16_t)(voice_level * p->master) >> VOLUME_SHIFT);
}

uint32_t mon_divs(int32_t dividend, int16_t divisor) {
    const int32_t quotient = dividend / divisor;
    if (quotient < INT16_MIN || quotient > INT16_MAX) {
        return (uint32_t)dividend;
    }
    const int32_t remainder = dividend % divisor;
    return (uint32_t)(uint16_t)remainder << 16 | (uint16_t)quotient;
}

// The slide from the previous note's period to the new one: the period moves by slide_step / slide_length of the
// distance in each tick.

void mon_run_slide(MonVoice* v) {
    if (v->slide_length == 0) {
        return;
    }
    if (v->start == MonStart_Legato || v->start == MonStart_Fresh) {
        if (v->start == MonStart_Legato) {
            v->start = MonStart_Idle;
        }
        v->slide_step = 1;
        v->base_period = v->slide_from;
        v->slide_delta = (int16_t)(uint16_t)(v->slide_from - v->note_period);
        return;
    }
    const int32_t product = (int32_t)(int16_t)v->slide_step * v->slide_delta;
    const uint16_t moved = (uint16_t)mon_divs(product, (int16_t)v->slide_length);
    v->base_period = (uint16_t)(v->slide_from - moved);
    if (v->slide_step == v->slide_length) {
        v->slide_length = 0;
    } else {
        v->slide_step++;
    }
}

uint8_t mon_list_byte(const MonModule* m, uint32_t list, uint8_t* pos) {
    uint8_t position = *pos;
    for (int guard = 0; guard < MON_LIST_GUARD; guard++) {
        const uint8_t byte = mon_u8(m, list + position);
        if (byte != MON_LIST_JUMP) {
            *pos = (uint8_t)(position + 1);
            return byte;
        }
        position = (uint8_t)(position - mon_u8(m, list + position + 1u));
    }
    *pos = position;
    return 0;
}

// The older versions' vibrato list: a byte 0x80 jumps to the position in the byte behind it.

static uint8_t absolute_list_byte(const MonModule* m, uint32_t list, uint8_t* pos) {
    uint8_t position = *pos;
    for (int guard = 0; guard < MON_LIST_GUARD; guard++) {
        const uint8_t byte = mon_u8(m, list + position);
        if (byte != MON_LIST_JUMP) {
            *pos = (uint8_t)(position + 1);
            return byte;
        }
        position = mon_u8(m, list + position + 1u);
    }
    *pos = position;
    return 0;
}

// The vibrato list gives the period offset as a fraction of the period.

void mon_run_vibrato(MonPlayer* p, int voice, const MonInstrument* ins) {
    const MonModule* m = &p->module;
    MonVoice* v = &p->voices[voice];
    if (ins->vibrato == 0) {
        return;
    }
    if (v->vibrato_delay != 0) {
        v->vibrato_delay--;
        return;
    }
    if (--v->vibrato_timer != 0) {
        return;
    }
    v->vibrato_timer = ins->vibrato_speed;
    const uint32_t list = mon_song_address(m, ins->vibrato);
    const uint8_t entry = mon_is_old_version(m->layout->version) ? absolute_list_byte(m, list, &v->vibrato_pos)
                                                                 : mon_list_byte(m, list, &v->vibrato_pos);
    const int16_t depth = (int16_t)(int8_t)(uint8_t)(0u - entry);
    v->vibrato_offset = (uint16_t)(depth * (int16_t)(v->period >> VIBRATO_SHIFT));
}

void mon_select_instrument(MonPlayer* p, int voice, uint8_t number) {
    const MonModule* m = &p->module;
    MonVoice* v = &p->voices[voice];
    v->instrument = m->instruments + (uint32_t)number * MON_INSTRUMENT_BYTES;
    const MonInstrument ins = mon_read_instrument(m, v->instrument);
    p->instrument_register = v->instrument;
    v->new_instrument = true;
    v->volume = ins.volume;
    v->arpeggio_list = ins.arpeggio;
    v->display_instrument = number;
}

void mon_start_note(MonPlayer* p, int voice, uint8_t note, const MonInstrument* ins) {
    const MonModule* m = &p->module;
    MonVoice* v = &p->voices[voice];
    v->note = note;
    v->note_period = mon_scaled_period(m, note, ins->tuning);
    v->base_period = v->note_period;
    v->envelope_timer = 1;
    v->envelope_pos = 0;
    v->arpeggio_timer = 1;
    v->arpeggio_pos = 0;
    v->arpeggio_offset = 0;
    v->vibrato_delay = ins->vibrato_delay;
    v->vibrato_timer = 1;
    v->vibrato_pos = 0;
    v->vibrato_offset = 0;
    v->start = MonStart_Fresh;
    v->display_note = note;
}

// The sample of an instrument of the sample kind: the DMA starts one tick after the note-on, the loop registers are
// written one tick later. The loop starts at a word offset into the sample; MonVersion_Gyroscope adds it to the
// address once, as a byte offset.

void mon_run_sample(MonPlayer* p, int voice, const MonInstrument* ins) {
    const MonModule* m = &p->module;
    MonVoice* v = &p->voices[voice];
    NuadePaulaChannel* paula = &p->paula[voice];
    const uint32_t sample = m->songs + ins->sample;
    switch (v->start) {
        case MonStart_Idle:
            return;
        case MonStart_Legato:
            v->start = MonStart_Fresh;
            break;
        case MonStart_Fresh:
            v->start = MonStart_Loop;
            break;
        case MonStart_Loop:
            v->start = MonStart_Idle;
            break;
    }
    if (v->start != MonStart_Idle) {
        paula->lc = mon_sample(m, sample);
        paula->len_words = ins->length_words;
        nuade_paula_channel_dma_on(paula);
        return;
    }
    if (ins->loop_start == MON_NO_LOOP) {
        paula->lc = mon_sample(m, m->base + m->layout->silent_buffer);
        paula->len_words = SILENT_BUFFER_WORDS;
        return;
    }
    const uint32_t skip = (uint32_t)(int32_t)(int16_t)ins->loop_start;
    const uint32_t skip_bytes = m->layout->version == MonVersion_Gyroscope ? skip : skip * 2;
    paula->lc = mon_sample(m, sample + skip_bytes);
    paula->len_words = (uint16_t)(ins->length_words - ins->loop_start);
}

bool mon_step_voice(MonPlayer* p, int voice) {
    MonVoice* v = &p->voices[voice];
    const bool old_version = mon_is_old_version(p->module.layout->version);
    if (v->restart) {
        v->restart = false;
        if (!mon_sequence_next(p, voice, false)) {
            return false;
        }
    } else {
        p->instrument_register = v->instrument;
        if (--v->ticks_left != 0) {
            if (v->envelope_on) {
                if (old_version) {
                    mon_old_sustain(p, voice);
                } else {
                    mon_new_sustain(p, voice);
                }
            }
            return true;
        }
    }
    return old_version ? mon_old_read_pattern(p, voice) : mon_new_read_pattern(p, voice);
}
