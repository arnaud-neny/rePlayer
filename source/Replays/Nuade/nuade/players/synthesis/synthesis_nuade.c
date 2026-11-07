// SPDX-License-Identifier: MIT
// Synthesis (Synth4.0 / Synth4.2) as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/synthesis.h>
#include <stdio.h>

#define SYNTHESIS_NUADE_VOICES 4

// Notes are period table indexes; index 49 (period 856) is C-1, so the column counts from index 1 and spells three
// octaves down.
#define SYNTHESIS_NOTE_BASE 1
#define SYNTHESIS_NOTE_STOP 0x7f
#define SYNTHESIS_OCTAVE_SHIFT (-3)

// The module mark is specific enough to be sure.
static NuadeProbe synthesis_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return syn_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult synthesis_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    SynModule* module = syn_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Synthesis module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) { .voice_count = SYNTHESIS_NUADE_VOICES,
                                    .subsong_count = (uint32_t)syn_subsong_count(module),
                                    .default_subsong = 0,
                                    .timebase_hz = SYN_TICK_HZ };
    return result;
}

static void synthesis_close(void* user_data) {
    syn_destroy((SynModule*)user_data);
}

static bool synthesis_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return syn_start((SynModule*)user_data, (int)subsong, voices);
}

static NuadeTickResult synthesis_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    return (NuadeTickResult) { .length = 1,
                               .flags = syn_tick((SynModule*)user_data) ? NuadeTick_Looped : NuadeTick_None };
}

static void synthesis_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const SynModule* module = (const SynModule*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, syn_format_name(module));
    if (syn_title(module)[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_TITLE, syn_title(module));
    }
}

static NuadePositionResult synthesis_position(void* user_data) {
    const SynPosition p = syn_position((const SynModule*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.position,
                                                 .row = (uint32_t)p.row,
                                                 .rows = (uint32_t)p.rows,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t synthesis_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const SynModule* module = (const SynModule*)user_data;
    const SynPosition p = syn_position(module);
    const uint32_t n = cap < SYNTHESIS_NUADE_VOICES ? cap : SYNTHESIS_NUADE_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const SynVoiceState v = syn_voice_state(module, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note != 0 ? v.note - SYNTHESIS_NOTE_BASE : -1,
                                    .instrument = v.instrument != 0 ? v.instrument - 1 : -1,
                                    .order = (uint32_t)p.position,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult synthesis_tracker_info(void* user_data, const NuadePosition* at) {
    const SynModule* module = (const SynModule*)user_data;
    if ((int)at->order >= syn_get_num_positions(module)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = SYNTHESIS_NUADE_VOICES,
                                                .rows = at->rows != 0 ? at->rows : (uint32_t)syn_default_rows(module),
                                                .column_count = 5,
                                                .columns = { { "note", NuadeColumn_Note, 3, SYNTHESIS_OCTAVE_SHIFT },
                                                             { "inst", NuadeColumn_Hex, 2, 0 },
                                                             { "arp", NuadeColumn_Hex, 1, 0 },
                                                             { "fx", NuadeColumn_Hex, 1, 0 },
                                                             { "arg", NuadeColumn_Hex, 2, 0 } } } };
}

// A track line as written in the module.
typedef struct SynCell {
    uint8_t note;
    uint8_t instrument;
    uint8_t arpeggio;
    uint8_t effect;
    uint8_t effect_arg;
} SynCell;

static NuadePatternCell synthesis_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const SynModule* module = (const SynModule*)ctx;
    uint16_t start_row = 0;
    int8_t sound_transpose, note_transpose;
    syn_get_position(module, (int)order, (int)channel, &start_row, &sound_transpose, &note_transpose);
    SynCell c = { 0 };
    syn_get_cell(module, start_row + (int)row, &c.note, &c.instrument, &c.arpeggio, &c.effect, &c.effect_arg);
    if (c.note == SYNTHESIS_NOTE_STOP) {
        cell.value[0] = NUADE_NOTE_CUT;
    } else if (c.note != 0) {
        cell.value[0] = c.note - SYNTHESIS_NOTE_BASE;
    }
    if (c.instrument != 0) {
        cell.value[1] = c.instrument;
    }
    if (c.arpeggio != 0) {
        cell.value[2] = c.arpeggio;
    }
    if (c.effect != 0 || c.effect_arg != 0) {
        cell.value[3] = c.effect;
        cell.value[4] = c.effect_arg;
    }
    return cell;
}

static uint32_t synthesis_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                        int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const SynModule* module = (const SynModule*)user_data;
    if ((int)at->order >= syn_get_num_positions(module)) {
        return 0;
    }
    const uint32_t rows = at->rows != 0 ? at->rows : (uint32_t)syn_default_rows(module);
    return nuade_tracker_fill(at, SYNTHESIS_NUADE_VOICES, rows, channel, row_lo, row_hi, out, cap, synthesis_cell,
                              user_data);
}

const NuadePlayer nuade_player_synthesis = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "synthesis",
    .name = "Synthesis",
    .version = "1",
    .extensions = "syn",
    .probe = synthesis_probe,
    .open = synthesis_open,
    .close = synthesis_close,
    .start = synthesis_start,
    .tick = synthesis_tick,
    .metadata = synthesis_metadata,
    .position = synthesis_position,
    .voice_info = synthesis_voice_info,
    .tracker_info = synthesis_tracker_info,
    .tracker_cells = synthesis_tracker_cells,
};

SynModule* syn_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_synthesis) {
        return nullptr;
    }
    return (SynModule*)nuade_player_data(song);
}
