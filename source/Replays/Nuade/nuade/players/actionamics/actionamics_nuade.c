// SPDX-License-Identifier: MIT
// Actionamics Sound Tool as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/actionamics.h>
#include <stdio.h>

#define ACTIONAMICS_NUADE_VOICES 4
#define ACTIONAMICS_ROWS 64 // rows of a pattern until a set rows effect says otherwise

// Period table index 34 is the ProTracker C-1 (period 856); NuadeColumn_Note counts semitones from C-0 and the lowest
// index is 33 semitones below it.
#define ACTIONAMICS_NOTE_BASE 2
#define ACTIONAMICS_NOTE_SHIFT (-2)

// The signature string is specific enough to be sure.
static NuadeProbe actionamics_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return act_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult actionamics_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    ActModule* module = act_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Actionamics module");
        return result;
    }
    const int subsongs = act_subsong_count(module);
    if (subsongs <= 0) {
        act_destroy(module);
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "Actionamics module without subsongs");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) { .voice_count = ACTIONAMICS_NUADE_VOICES,
                                    .subsong_count = (uint32_t)subsongs,
                                    .default_subsong = 0,
                                    .timebase_hz = act_timebase_hz(module) };
    return result;
}

static void actionamics_close(void* user_data) {
    act_destroy((ActModule*)user_data);
}

static bool actionamics_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return act_start((ActModule*)user_data, (int)subsong, voices);
}

static NuadeTickResult actionamics_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    return (NuadeTickResult) { .length = ACT_TICK_LENGTH,
                               .flags = act_tick((ActModule*)user_data) ? NuadeTick_Looped : NuadeTick_None };
}

static void actionamics_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const ActModule* module = (const ActModule*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Actionamics Sound Tool");
    const int count = act_get_sample_count(module);
    for (int i = 0; i < count; i++) {
        const char* name = act_get_sample_name(module, i);
        if (name[0] != '\0') {
            sink->add_instrument(sink->ctx, (uint32_t)i, name);
        }
    }
}

static NuadePositionResult actionamics_position(void* user_data) {
    const ActPosition p = act_position((const ActModule*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.position,
                                                 .row = (uint32_t)p.row,
                                                 .rows = (uint32_t)p.rows,
                                                 .tick = (uint32_t)p.tick,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t actionamics_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const ActModule* module = (const ActModule*)user_data;
    const ActPosition p = act_position(module);
    const uint32_t n = cap < ACTIONAMICS_NUADE_VOICES ? cap : ACTIONAMICS_NUADE_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const ActVoiceState v = act_voice_state(module, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note != 0 ? v.note + ACTIONAMICS_NOTE_BASE : -1,
                                    .instrument = v.instrument != 0 ? v.instrument - 1 : -1,
                                    .order = (uint32_t)p.position,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

// The rows of the pattern a position asks for: the count it carries, else the initial one; none outside the song.
static uint32_t actionamics_rows(const ActModule* module, const NuadePosition* at) {
    if ((int)at->order >= act_position_count(module)) {
        return 0;
    }
    return at->rows != 0 ? at->rows : ACTIONAMICS_ROWS;
}

static NuadeTrackerInfoResult actionamics_tracker_info(void* user_data, const NuadePosition* at) {
    const uint32_t rows = actionamics_rows((const ActModule*)user_data, at);
    if (rows == 0) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = ACTIONAMICS_NUADE_VOICES,
                                                .rows = rows,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3, ACTIONAMICS_NOTE_SHIFT },
                                                             { "ins", NuadeColumn_Hex, 2 },
                                                             { "fx", NuadeColumn_Hex, 2 },
                                                             { "arg", NuadeColumn_Hex, 2 } } } };
}

static NuadePatternCell actionamics_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const ActCell c = act_pattern_cell((const ActModule*)ctx, (int)order, (int)channel, (int)row);
    if (c.note != 0) {
        cell.value[0] = c.note + ACTIONAMICS_NOTE_BASE;
    }
    if (c.instrument != 0) {
        cell.value[1] = c.instrument;
    }
    if (c.effect != 0 || c.effect_arg != 0) {
        cell.value[2] = c.effect;
        cell.value[3] = c.effect_arg;
    }
    return cell;
}

static uint32_t actionamics_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                          int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const ActModule* module = (const ActModule*)user_data;
    return nuade_tracker_fill(at, ACTIONAMICS_NUADE_VOICES, actionamics_rows(module, at), channel, row_lo, row_hi, out,
                              cap, actionamics_cell, module);
}

const NuadePlayer nuade_player_actionamics = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "actionamics",
    .name = "Actionamics Sound Tool",
    .version = "1",
    .extensions = "ast",
    .probe = actionamics_probe,
    .open = actionamics_open,
    .close = actionamics_close,
    .start = actionamics_start,
    .tick = actionamics_tick,
    .metadata = actionamics_metadata,
    .position = actionamics_position,
    .voice_info = actionamics_voice_info,
    .tracker_info = actionamics_tracker_info,
    .tracker_cells = actionamics_tracker_cells,
};

ActModule* act_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_actionamics) {
        return nullptr;
    }
    return (ActModule*)nuade_player_data(song);
}
