// SPDX-License-Identifier: MIT
// Maniacs of Noise player: the voice replayer of MonVersion_DeathTrap, MonVersion_DragonWars and
// MonVersion_MysteryWorldsEnd. A voice whose note has run out reads its pattern; every other voice runs its envelope,
// slide, arpeggio, vibrato and sample start, and writes period and volume.
//
// MonVersion_DragonWars adds the arpeggio list and a voice mask (a byte at the start of the image) that keeps
// a voice's DMA and registers untouched, and starts the sample of every kind of instrument. MonVersion_MysteryWorldsEnd
// has no voice mask and plays instruments of kind 0 as a sequence of waves.

#include "mon_internal.h"

#define ENVELOPE_HOLD 0x81 // an envelope byte: stay while the note is held, else go on behind it
#define WAVE_SPEED 0xFE    // a wave list byte: set the speed from the byte that follows
#define WAVE_JUMP 0xFF     // a wave list byte: jump back by the byte that follows

// A length byte in ticks: the tempo times the byte, less the swing the voice owes.

static uint16_t length_ticks(const MonPlayer* p, MonVoice* v, uint8_t length) {
    const uint16_t ticks = (uint16_t)((uint32_t)length * p->tempo - v->swing);
    v->swing = 0;
    return ticks;
}

// MonVersion_DragonWars: the voices the byte at the start of the image enables.

static bool voice_audible(const MonPlayer* p, int voice) {
    return p->module.layout->version != MonVersion_DragonWars
           || (mon_u8(&p->module, MON_VOICE_MASK_ADDRESS) >> voice & 1) != 0;
}

static bool has_arpeggio(const MonPlayer* p) {
    return p->module.layout->version != MonVersion_DeathTrap;
}

// Note-on: the voice starts a note, stops the DMA and leaves the sample start to the next tick.

static void note_on(MonPlayer* p, int voice, uint8_t note, uint8_t length, const MonInstrument* ins) {
    MonVoice* v = &p->voices[voice];
    mon_start_note(p, voice, note, ins);
    v->ticks_left = length_ticks(p, v, length);
    v->envelope_hold = true;
    v->envelope_on = true;
    if (voice_audible(p, voice)) {
        nuade_paula_channel_dma_off(&p->paula[voice]);
    }
}

bool mon_new_read_pattern(MonPlayer* p, int voice) {
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
                v->slide_length = (uint16_t)((uint32_t)mon_u8(m, at++) * p->tempo);
                break;
            }
            case MonPatternCommand_SlideNote: {
                const uint8_t note = (uint8_t)(mon_u8(m, at++) + v->transpose);
                const uint16_t period = mon_scaled_period(m, note, ins.tuning);
                v->slide_from = v->note_period;
                v->note_period = period;
                v->base_period = period;
                v->slide_length = (uint16_t)((uint32_t)mon_u8(m, at++) * p->tempo);
                v->ticks_left = length_ticks(p, v, mon_u8(m, at++));
                v->start = MonStart_Legato;
                v->envelope_on = true;
                v->pattern = at;
                return true;
            }
            case MonPatternCommand_Release:
                v->ticks_left = length_ticks(p, v, mon_u8(m, at++));
                if (voice_audible(p, voice)) {
                    paula->volume = 0;
                }
                v->envelope_on = false;
                v->pattern = at;
                return true;
            case MonPatternCommand_Arpeggio:
                v->arpeggio_timer = 1;
                v->arpeggio_pos = 0;
                v->arpeggio_offset = 0;
                v->arpeggio_list = mon_u16(m, at);
                at += 2;
                break;
            case MonPatternCommand_Wait:
                v->ticks_left = mon_u8(m, at++);
                v->envelope_on = false;
                nuade_paula_channel_dma_off(paula);
                v->pattern = at;
                return true;
            case MonPatternCommand_Swing:
                v->swing = mon_u8(m, at++);
                break;
            case MonPatternCommand_Rest:
                v->ticks_left = length_ticks(p, v, mon_u8(m, at++));
                v->envelope_hold = false;
                v->pattern = at;
                return true;
            case MonPatternCommand_Unknown: // the walk of the songs has refused a module with one
                p->playing = false;
                return false;
        }
    }
    p->playing = false;
    return false;
}

// The envelope list: 0x81 stays on the byte while the note is held and goes on behind it after the release. The index
// into the list is a word that the hold moves, the position of the voice a byte.

static void run_envelope(MonPlayer* p, int voice, const MonInstrument* ins) {
    const MonModule* m = &p->module;
    MonVoice* v = &p->voices[voice];
    if (--v->envelope_timer != 0) {
        return;
    }
    v->envelope_timer = ins->envelope_speed;
    const uint32_t list = mon_song_address(m, ins->envelope);
    uint16_t index = v->envelope_pos;
    for (int guard = 0; guard < MON_LIST_GUARD; guard++) {
        const uint8_t byte = mon_u8(m, list + (uint32_t)(int32_t)(int16_t)index);
        if (byte == ENVELOPE_HOLD && !v->envelope_hold) {
            v->envelope_pos = (uint8_t)(v->envelope_pos + 2);
            index = (uint16_t)(index + 2);
        } else if (byte == ENVELOPE_HOLD || byte == MON_LIST_JUMP) {
            const uint8_t back = mon_u8(m, list + (uint32_t)(int32_t)(int16_t)index + 1u);
            index = (uint16_t)((index & 0xFF00) | (uint8_t)(index - back));
            v->envelope_pos = (uint8_t)(v->envelope_pos - back);
        } else {
            v->envelope_pos = (uint8_t)(v->envelope_pos + 1);
            v->env_volume = mon_envelope_level(p, v, byte);
            return;
        }
    }
}

// MonVersion_DragonWars and later: the arpeggio list adds a period offset from notes relative to the voice's note.

static void run_arpeggio(MonPlayer* p, int voice, const MonInstrument* ins) {
    const MonModule* m = &p->module;
    MonVoice* v = &p->voices[voice];
    if (v->arpeggio_list == 0 || --v->arpeggio_timer != 0) {
        return;
    }
    v->arpeggio_timer = ins->arpeggio_speed;
    const uint8_t entry = mon_list_byte(m, mon_song_address(m, v->arpeggio_list), &v->arpeggio_pos);
    const uint8_t note = (uint8_t)(entry + v->note);
    v->arpeggio_offset = (uint16_t)(mon_scaled_period_word(m, note, ins->tuning) - v->note_period);
}

// MonVersion_MysteryWorldsEnd, instruments of kind 0: a list of waves, each played for a number of ticks. A note-on
// without a new instrument restarts the list only when the instrument says so.

static void run_waves(MonPlayer* p, int voice, const MonInstrument* ins) {
    const MonModule* m = &p->module;
    MonVoice* v = &p->voices[voice];
    NuadePaulaChannel* paula = &p->paula[voice];
    if (v->start == MonStart_Fresh && (v->new_instrument || ins->restart_waves)) {
        v->wave_pos = 0;
        v->wave_timer = 1;
    }
    if (--v->wave_timer == 0) {
        v->wave_timer = v->wave_speed;
        v->period_fixed = 0;
        const uint32_t list = mon_song_address(m, ins->wave_list);
        uint8_t pos = (uint8_t)v->wave_pos;
        for (int guard = 0; guard < MON_LIST_GUARD; guard++) {
            const uint8_t byte = mon_u8(m, list + pos);
            if (byte == WAVE_SPEED) {
                v->wave_speed = mon_u8(m, list + pos + 1u);
                v->wave_timer = v->wave_speed;
                pos = (uint8_t)(pos + 2);
            } else if (byte == WAVE_JUMP) {
                pos = (uint8_t)(pos - mon_u8(m, list + pos + 1u));
            } else {
                const MonWave wave = mon_read_wave(m, byte);
                v->wave_length = wave.length_words;
                v->wave = mon_song_address(m, wave.offset);
                pos = (uint8_t)(pos + 1);
                break;
            }
        }
        v->wave_pos = pos;
    }
    paula->lc = mon_sample(m, v->wave);
    paula->len_words = v->wave_length;
    if (v->start == MonStart_Fresh) {
        v->start = MonStart_Idle;
        nuade_paula_channel_dma_on(paula);
    }
}

// A voice that goes on with its note: the envelope, slide, arpeggio, vibrato and sample start, then the registers.

void mon_new_sustain(MonPlayer* p, int voice) {
    MonVoice* v = &p->voices[voice];
    NuadePaulaChannel* paula = &p->paula[voice];
    const MonVersion version = p->module.layout->version;
    const MonInstrument ins = mon_read_instrument(&p->module, v->instrument);
    run_envelope(p, voice, &ins);
    mon_run_slide(v);
    if (has_arpeggio(p)) {
        run_arpeggio(p, voice, &ins);
    }
    v->period = (uint16_t)(v->base_period + v->arpeggio_offset);
    mon_run_vibrato(p, voice, &ins);
    v->period = (uint16_t)(v->period + v->vibrato_offset);
    if (ins.kind != 0 || version == MonVersion_DragonWars) {
        if (voice_audible(p, voice)) {
            mon_run_sample(p, voice, &ins);
        } else {
            v->start = MonStart_Idle;
        }
    } else if (version == MonVersion_MysteryWorldsEnd) {
        run_waves(p, voice, &ins);
    }
    v->new_instrument = false;
    if (voice_audible(p, voice)) {
        paula->period = v->period_fixed != 0 ? v->period_fixed : v->period;
        paula->volume = (uint8_t)v->env_volume;
    }
}
