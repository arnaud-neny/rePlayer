// SPDX-License-Identifier: MIT
// EarAche player: the replayer of the UADE eagleplayer "EarAche", one function per routine. Each tick it runs the
// sequencer every `speed` ticks (starting the notes of one row), then the volume table, the gate and the pitch of every
// voice, in the order channel 2, 0, 1, 3.

#include <nuade/players/earache.h>

#include "ea_internal.h"
#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>

#define MAX_VOLUME_TABLE_BYTE 0x40u
#define VOLUME_TABLE_RESTART 0xFFu
#define SLIDE_RESTART_ABOVE 0x40
#define MIN_SLIDE_PERIOD 100
#define NOTE_START_PERIOD 0x78u
#define PERIOD_INDEX_MASK 0x7FFu
#define OCTAVE_NOTES 12
#define FINE_TUNE_OCTAVE 5
#define PAUSED_ARPEGGIO_PHASE 2

bool ea_probe(const uint8_t* data, size_t size) {
    EaModule module;
    return ea_module_parse(&module, data, size);
}

EaPlayer* ea_create(const uint8_t* data, size_t size) {
    EaPlayer* player = calloc(1, sizeof(*player));
    uint8_t* bytes = nuade_paula_padded_copy(data, size);
    if (player == nullptr || bytes == nullptr) {
        free(player);
        free(bytes);
        return nullptr;
    }
    player->bytes = bytes;
    if (!ea_module_parse(&player->module, bytes, size)) {
        ea_destroy(player);
        return nullptr;
    }
    return player;
}

void ea_destroy(EaPlayer* player) {
    if (player != nullptr) {
        free(player->bytes);
        free(player);
    }
}

// InitSound: the first tick fetches the first position entry.

bool ea_start(EaPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    *player = (EaPlayer) { .bytes = player->bytes, .module = player->module, .speed_ticks = 1, .speed = 1 };
    for (uint32_t i = 0; i < EA_VOICES; i++) {
        player->voices[i] = (EaVoice) { .instrument = -1 };
        nuade_voice_stop(voices, i);
    }
    return true;
}

// The period of a new note: the table entry plus the instrument's fine tune, shifted left by 5 - octave (mod 8).

static uint16_t note_period(const EaInstrument* instrument, uint8_t note) {
    const uint32_t shift = (uint32_t)(FINE_TUNE_OCTAVE - (int)(note / OCTAVE_NOTES)) & 7u;
    const uint16_t fine = (uint16_t)((uint32_t)(int32_t)instrument->fine_tune << shift);
    return (uint16_t)(ea_period_entry(note) + fine);
}

// Starts the note of one voice: DMA off, the sample, the volume and pitch tables, then period and DMA on.

static void start_note(EaPlayer* player, uint32_t channel, EaCell cell) {
    if (cell.note == 0 && cell.instrument == 0) {
        return;
    }
    EaVoice* voice = &player->voices[channel];
    const EaInstrument instrument = ea_instrument(&player->module, cell.instrument);
    const EaSample sample = ea_sample(&player->module, instrument.sample);

    voice->paula.period = NOTE_START_PERIOD;
    nuade_paula_channel_dma_off(&voice->paula);
    voice->paula.len_words = sample.length_words;
    voice->paula.lc = sample.data;

    voice->volume_pos = instrument.volume_table;
    voice->volume_restart = instrument.volume_table;
    voice->arpeggio = instrument.arpeggio;
    if (instrument.arpeggio) {
        voice->arpeggio_phase = PAUSED_ARPEGGIO_PHASE;
        voice->arpeggio_offsets[0] = instrument.pitch_ticks;
        voice->arpeggio_offsets[1] = instrument.pitch_reload;
        voice->arpeggio_offsets[2] = instrument.arpeggio_step;
    } else {
        voice->slide_pos = instrument.pitch_table;
        voice->slide_restart = instrument.pitch_table;
    }
    voice->volume_ticks = instrument.volume_ticks;
    voice->volume_reload = instrument.volume_reload;
    voice->slide_ticks = instrument.pitch_ticks;
    voice->slide_reload = instrument.pitch_reload;

    voice->note = cell.note;
    voice->period = note_period(&instrument, cell.note);
    voice->paula.volume = instrument.volume;
    // (the replayer waits about nine raster lines here for the DMA stop to take effect)
    voice->paula.period = voice->period;
    nuade_paula_channel_dma_on(&voice->paula);
    if (instrument.gate_ticks != 0) {
        voice->gate_ticks = (uint8_t)(instrument.gate_ticks + 1u);
    } else {
        // plays the sample once, then repeats a single word
        voice->paula.len_words = 1;
        voice->gate_ticks = 0;
    }
    voice->instrument = cell.instrument;
}

// The sequencer reached the end of the pattern: the next position table entry. False when the table does not lead to
// a pattern.

static bool fetch_position(EaPlayer* player, bool* looped) {
    EaPattern pattern;
    if (!ea_pattern_at(&player->module, player->next_entry, &pattern)) {
        return false;
    }
    *looped = *looped || pattern.jumped;
    player->row = pattern.first_row;
    player->last_row = pattern.last_row;
    player->speed = pattern.speed;
    player->speed_ticks = pattern.speed;
    player->next_entry = pattern.next_index;
    return true;
}

// The next row; at the end of the pattern the next position entry. False when the position table is not followable.

static bool sequencer_step(EaPlayer* player, bool* looped) {
    player->row = (uint16_t)(player->row + 1u);
    if ((int16_t)player->row > (int16_t)player->last_row && !fetch_position(player, looped)) {
        return false;
    }
    for (uint32_t channel = 0; channel < EA_VOICES; channel++) {
        start_note(player, channel, ea_cell(&player->module, player->row, channel));
    }
    return true;
}

// The gate ran out: DMA off, the channel points at nothing and the voice goes idle.

static void stop_voice(EaVoice* voice) {
    nuade_paula_channel_dma_off(&voice->paula);
    voice->paula.lc = nullptr;
    voice->period = 0;
}

// One byte of the volume table: a volume, 0xFF to go back to the table start, anything else ends the table. Returns the
// ticks until the next byte.

static uint8_t volume_table_step(const EaModule* module, EaVoice* voice) {
    uint32_t pos = voice->volume_pos;
    bool restarted = false;
    for (;;) {
        const uint8_t value = ea_byte(module, pos);
        pos++;
        if (value <= MAX_VOLUME_TABLE_BYTE) {
            voice->paula.volume = value;
            voice->volume_pos = pos;
            return voice->volume_reload;
        }
        // (a table of restart bytes only would never return; the second restart ends the table)
        if (value != VOLUME_TABLE_RESTART || restarted) {
            return 0;
        }
        restarted = true;
        pos = voice->volume_restart;
    }
}

// The arpeggio cycles through the note, two offsets and a third; an offset of 0 sends the cycle back to phase 2.

static void arpeggio_step(EaVoice* voice) {
    voice->arpeggio_phase = (voice->arpeggio_phase + 1u) & 3u;
    uint32_t offset = 0;
    if (voice->arpeggio_phase != 0) {
        offset = voice->arpeggio_offsets[voice->arpeggio_phase - 1u];
        if (offset == 0) {
            voice->arpeggio_phase = PAUSED_ARPEGGIO_PHASE;
        }
    }
    voice->paula.period = ea_period_entry((offset + voice->note) & PERIOD_INDEX_MASK);
}

// One pitch table step: a signed period delta (the period never drops below 100), or a value above 0x40 to start the
// table again. Returns the ticks until the next step.

static uint8_t slide_step(const EaModule* module, EaVoice* voice) {
    uint32_t pos = voice->slide_pos;
    int8_t delta = (int8_t)ea_byte(module, pos);
    pos++;
    if (delta > SLIDE_RESTART_ABOVE) {
        pos = voice->slide_restart;
        delta = (int8_t)ea_byte(module, pos);
        pos++;
    }
    int16_t period = (int16_t)(uint16_t)(voice->period + delta);
    if (period < MIN_SLIDE_PERIOD) {
        period = MIN_SLIDE_PERIOD;
    }
    voice->paula.period = (uint16_t)period;
    voice->period = (uint16_t)period;
    voice->slide_pos = pos;
    return voice->slide_reload;
}

// The per-tick work of one voice: gate, volume table, then arpeggio or pitch slide.

static void voice_tick(const EaModule* module, EaVoice* voice) {
    if (voice->period == 0) {
        return;
    }
    if (voice->gate_ticks != 0) {
        voice->gate_ticks--;
        if (voice->gate_ticks == 0) {
            stop_voice(voice);
        }
    }
    if (voice->volume_ticks != 0) {
        voice->volume_ticks--;
        if (voice->volume_ticks == 0) {
            voice->volume_ticks = volume_table_step(module, voice);
        }
    }
    if (voice->arpeggio) {
        arpeggio_step(voice);
        return;
    }
    if (voice->slide_ticks != 0) {
        voice->slide_ticks--;
        if (voice->slide_ticks == 0) {
            voice->slide_ticks = slide_step(module, voice);
        }
    }
}

EaTickResult ea_tick(EaPlayer* player, NuadeVoices* voices) {
    EaTickResult result = { 0 };
    if (player->ended) {
        result.ended = true;
        return result;
    }
    player->speed_ticks--;
    if (player->speed_ticks == 0) {
        player->speed_ticks = player->speed;
        if (!sequencer_step(player, &result.looped)) {
            player->ended = true;
            result.ended = true;
            return result;
        }
    }
    static const uint32_t k_voice_order[EA_VOICES] = { 2, 0, 1, 3 };
    for (uint32_t i = 0; i < EA_VOICES; i++) {
        voice_tick(&player->module, &player->voices[k_voice_order[i]]);
    }
    for (uint32_t i = 0; i < EA_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, i, voices);
    }
    return result;
}

uint32_t ea_grid_ticks(const EaPlayer* player) {
    for (uint32_t i = 0; i < player->module.position_count; i++) {
        const EaPositionEntry entry = ea_position_entry(&player->module, i);
        if (entry.first != EA_JUMP_MARK && ea_entry_speed(entry) != 0) {
            return ea_entry_speed(entry);
        }
    }
    return 1;
}

EaVoiceState ea_voice_state(const EaPlayer* player, int voice) {
    const EaVoice* v = &player->voices[voice];
    return (EaVoiceState) { .period = v->instrument >= 0 ? ea_period_entry(v->note) : 0, .instrument = v->instrument };
}
