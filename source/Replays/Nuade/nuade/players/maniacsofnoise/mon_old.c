// SPDX-License-Identifier: MIT
// Maniacs of Noise player: the voice replayer of MonVersion_Gyroscope and MonVersion_FlimbosQuest. A voice whose note
// has run out reads its pattern; every other voice runs its envelope, slide, arpeggio, vibrato and sample start or wave
// list, and writes period and volume.
//
// The older versions keep lists as absolute positions (a byte with bit 7 set jumps to the position in its low bits, a
// vibrato byte 0x80 to the position in the next byte), play instruments of kind 0 as samples and the others as lists of
// waves, and have no swing, hold or wait commands. MonVersion_Gyroscope slides in equal steps and does not manage the
// envelope flag.

#include "mon_internal.h"

#define LIST_JUMP_BIT 0x80
#define LIST_POSITION_MASK 0x7F
#define WAVE_SET_LENGTH 0xFFFD // a wave list word: the wave length is the word behind it
#define WAVE_SET_PERIOD 0xFFFE // the period is the word behind it
#define WAVE_JUMP 0xFFFF       // jump back by the word behind it
#define WAVE_COMMAND_BYTES 4
#define WAVE_BYTES 2
#define WAVE_KIND_RESTARTING 2 // instruments of this kind keep their wave list across notes of the same instrument

static bool manages_envelope_flag(const MonPlayer* p) {
    return p->module.layout->version != MonVersion_Gyroscope;
}

static uint16_t length_ticks(const MonPlayer* p, uint8_t length) {
    return (uint16_t)((uint32_t)length * p->tempo);
}

// Note-on: the voice starts a note, stops the DMA and leaves the sample start to the next tick.

static void note_on(MonPlayer* p, int voice, uint8_t note, uint8_t length, const MonInstrument* ins) {
    MonVoice* v = &p->voices[voice];
    mon_start_note(p, voice, note, ins);
    v->ticks_left = length_ticks(p, length);
    v->wave_timer = 1;
    if (manages_envelope_flag(p)) {
        v->envelope_on = true;
    }
    nuade_paula_channel_dma_off(&p->paula[voice]);
}

bool mon_old_read_pattern(MonPlayer* p, int voice) {
    const MonModule* m = &p->module;
    MonVoice* v = &p->voices[voice];
    NuadePaulaChannel* paula = &p->paula[voice];
    MonInstrument ins = mon_read_instrument(m, p->instrument_register);
    uint32_t at = v->pattern;
    for (int guard = 0; guard < MON_COMMAND_GUARD; guard++) {
        const uint8_t byte = mon_u8(m, at++);
        switch (mon_pattern_command(m->layout->version, byte)) {
            case MonPatternCommand_Note:
                note_on(p, voice, (uint8_t)(byte + v->transpose), mon_u8(m, at++), &ins);
                v->pattern = at;
                return true;
            case MonPatternCommand_End:
                if (!mon_sequence_next(p, voice, true)) {
                    return false;
                }
                at = v->pattern;
                break;
            case MonPatternCommand_Instrument:
                mon_select_instrument(p, voice, mon_u8(m, at++));
                ins = mon_read_instrument(m, p->instrument_register);
                break;
            case MonPatternCommand_Volume:
                v->volume = mon_u8(m, at++);
                break;
            case MonPatternCommand_Slide: {
                const uint8_t note = (uint8_t)(mon_u8(m, at++) + v->transpose);
                v->slide_from = mon_scaled_period(m, note, ins.tuning);
                v->slide_length = length_ticks(p, mon_u8(m, at++));
                break;
            }
            case MonPatternCommand_SlideNote: {
                const uint8_t note = (uint8_t)(mon_u8(m, at++) + v->transpose);
                const uint16_t period = mon_scaled_period(m, note, ins.tuning);
                v->slide_from = v->note_period;
                v->note_period = period;
                v->base_period = period;
                v->slide_length = length_ticks(p, mon_u8(m, at++));
                v->ticks_left = length_ticks(p, mon_u8(m, at++));
                v->start = MonStart_Legato;
                if (manages_envelope_flag(p)) {
                    v->envelope_on = true;
                }
                v->pattern = at;
                return true;
            }
            case MonPatternCommand_Release:
                v->ticks_left = length_ticks(p, mon_u8(m, at++));
                paula->volume = 0;
                v->envelope_on = false;
                v->pattern = at;
                return true;
            case MonPatternCommand_Arpeggio:
            case MonPatternCommand_Wait:
            case MonPatternCommand_Swing:
            case MonPatternCommand_Rest:
            case MonPatternCommand_Unknown: // the walk of the songs has refused a module with one
                p->playing = false;
                return false;
        }
    }
    p->playing = false;
    return false;
}

// The byte of a list at the voice's position: a byte with bit 7 set jumps to the position in its low bits.

static uint8_t bit_list_byte(const MonModule* m, uint32_t list, uint8_t* pos) {
    uint8_t position = *pos;
    for (int guard = 0; guard < MON_LIST_GUARD; guard++) {
        const uint8_t byte = mon_u8(m, list + position);
        if ((byte & LIST_JUMP_BIT) == 0) {
            *pos = (uint8_t)(position + 1);
            return byte;
        }
        position = byte & LIST_POSITION_MASK;
    }
    *pos = position;
    return 0;
}

// The envelope list: any byte but a jump is the next level.

static void run_envelope(MonPlayer* p, int voice, const MonInstrument* ins) {
    const MonModule* m = &p->module;
    MonVoice* v = &p->voices[voice];
    if (--v->envelope_timer != 0) {
        return;
    }
    v->envelope_timer = ins->envelope_speed;
    v->env_volume = mon_envelope_level(p, v, bit_list_byte(m, mon_song_address(m, ins->envelope), &v->envelope_pos));
}

// MonVersion_Gyroscope: the slide moves the period by an equal step in each tick; the rest of the division is
// taken off in the last one.

static void run_linear_slide(MonVoice* v) {
    if (v->slide_length == 0) {
        return;
    }
    if (v->start == MonStart_Legato || v->start == MonStart_Fresh) {
        if (v->start == MonStart_Legato) {
            v->start = MonStart_Idle;
        }
        v->base_period = v->slide_from;
        const int32_t distance = (int16_t)(uint16_t)(v->slide_from - v->note_period);
        const uint32_t result = mon_divs(distance, (int16_t)v->slide_length);
        v->slide_step = (uint16_t)result;
        v->slide_remainder = (uint16_t)(result >> 16);
        return;
    }
    v->base_period = (uint16_t)(v->base_period - v->slide_step);
    v->slide_length--;
    if (v->slide_length == 0) {
        v->base_period = (uint16_t)(v->base_period - v->slide_remainder);
    }
}

// The arpeggio list of the instrument adds a period offset from notes relative to the voice's note.

static void run_arpeggio(MonPlayer* p, int voice, const MonInstrument* ins) {
    const MonModule* m = &p->module;
    MonVoice* v = &p->voices[voice];
    if (ins->arpeggio == 0 || --v->arpeggio_timer != 0) {
        return;
    }
    v->arpeggio_timer = ins->arpeggio_speed;
    const uint8_t entry = bit_list_byte(m, mon_song_address(m, ins->arpeggio), &v->arpeggio_pos);
    const uint8_t note = (uint8_t)(entry + v->note);
    v->arpeggio_offset = (uint16_t)(mon_scaled_period_word(m, note, ins->tuning) - v->note_period);
}

// Instruments of the other kinds: a list of waves, each played for a number of ticks. The list sets the wave length
// and the period too. A note-on restarts the list unless the instrument is of the kind that goes on and no instrument
// command came since. The position in the list is a signed word.

static void run_waves(MonPlayer* p, int voice, const MonInstrument* ins) {
    const MonModule* m = &p->module;
    MonVoice* v = &p->voices[voice];
    NuadePaulaChannel* paula = &p->paula[voice];
    if (v->start == MonStart_Fresh && (v->new_instrument || ins->kind != WAVE_KIND_RESTARTING)) {
        v->wave_pos = 0;
    } else {
        v->wave_timer--;
        if (v->wave_timer != 0) {
            return;
        }
    }
    const uint32_t list = mon_song_address(m, ins->wave_list);
    for (int guard = 0; guard < MON_LIST_GUARD; guard++) {
        const uint32_t entry_at = list + (uint32_t)(int32_t)(int16_t)v->wave_pos;
        const uint16_t entry = mon_u16(m, entry_at);
        const uint16_t argument = mon_u16(m, entry_at + WAVE_BYTES);
        if (entry == WAVE_SET_LENGTH) {
            v->wave_length = argument;
            v->wave_pos = (uint16_t)(v->wave_pos + WAVE_COMMAND_BYTES);
        } else if (entry == WAVE_SET_PERIOD) {
            v->period = argument;
            v->wave_pos = (uint16_t)(v->wave_pos + WAVE_COMMAND_BYTES);
        } else if (entry == WAVE_JUMP) {
            v->wave_pos = (uint16_t)(v->wave_pos - argument);
        } else {
            paula->len_words = v->wave_length;
            paula->lc = mon_sample(m, mon_song_address(m, entry));
            v->wave_timer = ins->wave_ticks;
            v->wave_pos = (uint16_t)(v->wave_pos + WAVE_BYTES);
            if (v->start == MonStart_Fresh) {
                v->start = MonStart_Idle;
                nuade_paula_channel_dma_on(paula);
            }
            return;
        }
    }
}

// A voice that goes on with its note: the envelope, slide, arpeggio, vibrato and sample start, then the registers.

void mon_old_sustain(MonPlayer* p, int voice) {
    MonVoice* v = &p->voices[voice];
    NuadePaulaChannel* paula = &p->paula[voice];
    const MonInstrument ins = mon_read_instrument(&p->module, v->instrument);
    run_envelope(p, voice, &ins);
    if (p->module.layout->version == MonVersion_Gyroscope) {
        run_linear_slide(v);
    } else {
        mon_run_slide(v);
    }
    run_arpeggio(p, voice, &ins);
    v->period = (uint16_t)(v->base_period + v->arpeggio_offset);
    mon_run_vibrato(p, voice, &ins);
    v->period = (uint16_t)(v->period + v->vibrato_offset);
    if (ins.kind == 0) {
        mon_run_sample(p, voice, &ins);
    } else {
        run_waves(p, voice, &ins);
    }
    v->new_instrument = false;
    paula->period = v->period;
    paula->volume = (uint8_t)v->env_volume;
}
