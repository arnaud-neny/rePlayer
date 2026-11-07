// SPDX-License-Identifier: MIT
// AProSys player, translated from the UADE eagleplayer "AProSys" (Petter A. Urkedal's player, adapted by Delirium &
// Buggs / DFT; compiled C).
//
// This file is the sequencer (order list, steps, rows) and the public interface; aprosys_voice.c runs the voices and
// aprosys_wave.c builds the synthesised waves. The module format is described in aprosys_internal.h.

#include "aprosys_internal.h"
#include "nuade_tracker.h"
#include <stdlib.h>
#include <string.h>

// The player reloads this before each tick: more jumps and envelope loops than this in one tick end the tick's work
// (the 68k player's guard against a module that loops without playing).
#define JUMP_BUDGET 255

// Fetching the next order entry or step may loop over entries that play nothing; the 68k code would run forever
#define SEQUENCER_LIMIT 4096

typedef enum SequencerPhase {
    SequencerPhase_Fetch,    // read the next order entry
    SequencerPhase_Step,     // start the next step of the entry, or leave the entry
    SequencerPhase_Row,      // play the next row of the step, or leave the step
    SequencerPhase_AfterRow, // the row has lasted its ticks: go on with the next one
} SequencerPhase;

// Rows

// The channel volume moves by `step` per row towards `target`; a step of 0 sets it. The byte arithmetic wraps like the
// player's (a step past 0 leaves a large value that is not at its target).
static uint8_t slide_channel_volume(uint8_t volume, uint8_t step, uint8_t target) {
    if (step == 0) {
        return target;
    }
    if (volume < target) {
        volume = (uint8_t)(volume + step);
        if (volume >= target) {
            return target;
        }
    }
    if (volume > target) {
        volume = (uint8_t)(volume - step);
        if (volume <= target) {
            return target;
        }
    }
    return volume;
}

// Pick the instrument and sample record of a note (or of its release). With the entry's "direct" flag the cell names
// the instrument and the entry offsets the sample; otherwise the entry offsets the instrument.
static void select_sound(const ApsModule* m, ApsVoice* v, const ApsCell* cell, const ApsTrackEntry* t, bool release) {
    if (t->instrument_direct) {
        v->instrument = m->instruments + (uint32_t)cell->instrument * APS_INSTRUMENT_SIZE;
        const ApsInstrument ins = aps_instrument(m, v->instrument);
        const uint16_t number = (uint16_t)((release ? ins.release_sample : ins.sample) + t->instrument);
        v->sample = m->sample_records + (uint32_t)number * APS_SAMPLE_RECORD_SIZE;
        return;
    }
    const uint16_t number = (uint16_t)(cell->instrument + t->instrument);
    v->instrument = m->instruments + (uint32_t)number * APS_INSTRUMENT_SIZE;
    const ApsInstrument ins = aps_instrument(m, v->instrument);
    v->sample = m->sample_records + (uint32_t)(release ? ins.release_sample : ins.sample) * APS_SAMPLE_RECORD_SIZE;
}

// A row without a new note ends the one that sounds: the release sample and envelope take over
static void release_note(ApsPlayer* p, ApsVoice* v, const ApsCell* cell, const ApsTrackEntry* t) {
    if (!v->note_on) {
        return;
    }
    v->note_on = false;
    const ApsInstrument current = aps_instrument(&p->module, v->instrument);
    if (current.release_sample != current.sample) {
        select_sound(&p->module, v, cell, t, true);
    }
    aps_release_envelope(p, v);
}

static void start_note(ApsPlayer* p, ApsVoice* v, const ApsCell* cell, const ApsTrackEntry* t) {
    const ApsModule* m = &p->module;
    v->note_on = true;
    v->note = (uint8_t)(cell->note + t->note);
    select_sound(m, v, cell, t, false);
    if (t->key) {
        aps_restart_envelopes(p, v, cell);
    }
    if (t->tempo) {
        v->volume = v->channel_volume;
        p->row_ticks = (uint8_t)(t->level * p->speed);
    } else {
        v->volume = (uint8_t)((t->level * v->channel_volume) >> 5);
    }
    const uint16_t period = aps_period_lookup(p, v->note);
    v->shown_note = period != 0 ? nuade_period_to_note(period) : -1;
    v->shown_instrument = (int)((v->instrument - m->instruments) / APS_INSTRUMENT_SIZE);
    aps_start_sound(p, v, t);
}

static void play_row(ApsPlayer* p) {
    const ApsModule* m = &p->module;
    for (uint32_t i = 0; i < APS_VOICES; i++) {
        ApsVoice* v = &p->voices[i];
        const ApsCell cell = aps_cell(m, p->step, i);
        const ApsTrackEntry t = aps_track_entry(m, v->track);
        v->kick = false;
        v->channel_volume = slide_channel_volume(v->channel_volume, cell.volume_step, cell.volume_target);
        if (t.arpeggio) {
            v->arpeggio = m->arpeggios + (uint16_t)(t.param + cell.arpeggio);
        } else {
            v->slide = t.param;
        }
        if (t.note == APS_NO_NOTE || cell.note == APS_NO_NOTE) {
            release_note(p, v, &cell, &t);
        } else {
            start_note(p, v, &cell, &t);
        }
    }
    // A chained sound is started by an audio interrupt, which the player asks for by writing the channel's data
    // register while its DMA is off
    for (uint32_t i = 0; i < APS_VOICES; i++) {
        if (p->voices[i].kick) {
            p->voices[i].irq_enabled = true;
            p->voices[i].irq_pending = true;
        }
    }
}

// The sequencer

// A step starts: the row length and each voice's first track entry
static void load_step(ApsPlayer* p) {
    const ApsModule* m = &p->module;
    p->row_ticks = (uint8_t)(aps_step(m, p->step).duration * p->speed);
    for (uint32_t i = 0; i < APS_VOICES; i++) {
        p->voices[i].track = m->tracks + (uint32_t)aps_cell(m, p->step, i).track * APS_TRACK_ENTRY_SIZE;
    }
}

// Move the order position by the entry's argument. False when the jump budget ran out.
static bool jump_order(ApsPlayer* p, const ApsOrderEntry* entry) {
    if (entry->command == ApsOrder_Jump && entry->param >= 0x80) {
        p->looped = true;
    }
    p->order_pos = (uint8_t)(p->order_pos + entry->param);
    return --p->jump_budget != 0;
}

// Read order entries until one plays steps; false when the song ends
static bool fetch_order(ApsPlayer* p) {
    const ApsModule* m = &p->module;
    for (uint32_t n = 0; n < SEQUENCER_LIMIT; n++) {
        const ApsOrderEntry entry = aps_order_entry(m, p->order_pos);
        switch (entry.command) {
            case ApsOrder_End:
                return false;
            case ApsOrder_Steps:
                p->step = m->steps + (uint32_t)entry.param * APS_STEP_SIZE;
                return true;
            case ApsOrder_Repeat:
                if (p->repeat_left == APS_NO_REPEAT) {
                    p->repeat_left = entry.count;
                }
                p->repeat_left--;
                if (p->repeat_left != 0) {
                    if (!jump_order(p, &entry)) {
                        return false;
                    }
                    continue;
                }
                p->repeat_left = APS_NO_REPEAT;
                break;
            case ApsOrder_Jump:
                if (!jump_order(p, &entry)) {
                    return false;
                }
                continue;
            case ApsOrder_Octave:
                p->period_base = entry.param & 0x3f;
                break;
            case ApsOrder_Speed:
                p->speed = entry.param;
                break;
            default:
                break;
        }
        p->order_pos++;
    }
    return false;
}

// Walk the song from `phase` to the next row that lasts at least one tick, playing every row on the way. False when
// the song ends.
static bool run_sequencer(ApsPlayer* p, SequencerPhase phase) {
    const ApsModule* m = &p->module;
    for (uint32_t n = 0; n < SEQUENCER_LIMIT; n++) {
        switch (phase) {
            case SequencerPhase_Fetch:
                if (!fetch_order(p)) {
                    return false;
                }
                p->steps_done = 0;
                phase = SequencerPhase_Step;
                break;
            case SequencerPhase_Step:
                if (p->steps_done < aps_order_entry(m, p->order_pos).count) {
                    load_step(p);
                    p->row = 0;
                    phase = SequencerPhase_Row;
                } else {
                    p->order_pos++;
                    phase = SequencerPhase_Fetch;
                }
                break;
            case SequencerPhase_Row:
                if (p->row < aps_step(m, p->step).rows) {
                    play_row(p);
                    p->ticks_left = p->row_ticks;
                    phase = SequencerPhase_AfterRow;
                } else {
                    p->step += APS_STEP_SIZE;
                    p->steps_done++;
                    phase = SequencerPhase_Step;
                }
                break;
            case SequencerPhase_AfterRow:
                if (p->ticks_left != 0) {
                    return true;
                }
                for (uint32_t i = 0; i < APS_VOICES; i++) {
                    p->voices[i].track += APS_TRACK_ENTRY_SIZE;
                }
                p->row++;
                phase = SequencerPhase_Row;
                break;
        }
    }
    return false;
}

// The order list reached its end command: all audio DMA and interrupts off. The 68k player goes on running its
// sequencer into the data behind the order list; the port stops here.
static void stop_audio(ApsPlayer* p) {
    for (uint32_t i = 0; i < APS_VOICES; i++) {
        nuade_paula_channel_dma_off(&p->voices[i].paula);
        p->voices[i].irq_enabled = false;
        p->voices[i].irq_pending = false;
    }
    p->ended = true;
}

// Public interface

bool aps_probe(const uint8_t* data, size_t size) {
    ApsModule module;
    return aps_module_parse(&module, data, size, nullptr);
}

ApsPlayer* aps_create(const uint8_t* data, size_t size) {
    ApsModule probe;
    if (!aps_module_parse(&probe, data, size, nullptr)) {
        return nullptr;
    }
    ApsPlayer* p = calloc(1, sizeof(ApsPlayer));
    if (p == nullptr) {
        return nullptr;
    }
    p->data = nuade_paula_padded_copy(data, size);
    p->chip = (int8_t*)nuade_paula_padded_copy(nullptr, APS_CHIP_BYTES);
    if (p->data == nullptr || p->chip == nullptr) {
        aps_destroy(p);
        return nullptr;
    }
    p->wave = p->chip + APS_SILENCE_DISTANCE;
    aps_module_parse(&p->module, p->data, size, p->wave);
    return p;
}

void aps_destroy(ApsPlayer* player) {
    if (player != nullptr) {
        free(player->data);
        free(player->chip);
        free(player);
    }
}

bool aps_start(ApsPlayer* p, int subsong, NuadeVoices* voices) {
    if (p == nullptr || voices == nullptr || subsong != 0) {
        return false;
    }
    memset(p->chip, 0, APS_CHIP_BYTES);
    p->order_pos = 0;
    p->steps_done = 0;
    p->row = 0;
    p->step = APS_ADDR_VECTORS;
    p->ticks_left = 0;
    p->row_ticks = 0;
    p->speed = 0;
    p->repeat_left = APS_NO_REPEAT;
    p->jump_budget = JUMP_BUDGET;
    p->period_base = 0;
    p->looped = false;
    p->ended = false;
    for (uint32_t i = 0; i < APS_VOICES; i++) {
        p->voices[i] = (ApsVoice) { .instrument = APS_ADDR_VECTORS,
                                    .arpeggio = APS_ADDR_VECTORS,
                                    .sample = APS_ADDR_VECTORS,
                                    .track = APS_ADDR_VECTORS,
                                    .wave_build = i * APS_WAVE_BUFFER_BYTES,
                                    .wave_spare = (i + APS_VOICES) * APS_WAVE_BUFFER_BYTES,
                                    .shown_note = -1,
                                    .shown_instrument = -1 };
        for (uint32_t e = 0; e < APS_ENVELOPES; e++) {
            p->voices[i].env[e].stream = APS_ADDR_VECTORS;
        }
        NuadeVoiceState state;
        nuade_voice_stop(voices, i);
        nuade_voice_get_state(voices, i, &state);
        p->voices[i].wraps_seen = state.wraps;
    }

    // InitSound plays the first row
    if (!run_sequencer(p, SequencerPhase_Fetch)) {
        stop_audio(p);
    }
    for (uint32_t i = 0; i < APS_VOICES; i++) {
        aps_service_interrupts(p, &p->voices[i]);
    }
    return true;
}

ApsTickResult aps_tick(ApsPlayer* p, NuadeVoices* voices) {
    if (p->ended) {
        return (ApsTickResult) { .ended = true };
    }
    p->looped = false;
    for (uint32_t i = 0; i < APS_VOICES; i++) {
        aps_answer_wraps(p, &p->voices[i], i, voices);
    }

    p->jump_budget = JUMP_BUDGET;
    aps_update_voices(p);
    p->ticks_left--;
    if (!run_sequencer(p, SequencerPhase_AfterRow)) {
        stop_audio(p);
    }
    for (uint32_t i = 0; i < APS_VOICES; i++) {
        aps_service_interrupts(p, &p->voices[i]);
        nuade_paula_channel_sync(&p->voices[i].paula, i, voices);
    }
    return (ApsTickResult) { .looped = p->looped, .ended = p->ended };
}

ApsVoiceState aps_voice_state(const ApsPlayer* p, int voice) {
    const ApsVoice* v = &p->voices[voice];
    return (ApsVoiceState) { .note = v->shown_note, .instrument = v->shown_instrument };
}
