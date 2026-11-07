// SPDX-License-Identifier: MIT
// PumaTracker as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/pumatracker.h>
#include <stdio.h>

static NuadeProbe pumatracker_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return puma_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult pumatracker_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    PumaPlayer* player = puma_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid PumaTracker module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = PUMA_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = PUMA_TIMEBASE_HZ
    };
    return result;
}

static void pumatracker_close(void* user_data) {
    puma_destroy((PumaPlayer*)user_data);
}

static bool pumatracker_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)subsong;
    (void)sample_rate;
    return puma_start((PumaPlayer*)user_data, voices);
}

static NuadeTickResult pumatracker_tick(void* user_data, NuadeVoices* voices) {
    const PumaTickResult tick = puma_tick((PumaPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = PUMA_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void pumatracker_metadata(void* user_data, const NuadeMetadataSink* sink) {
    char title[13] = { 0 };
    const char* name = puma_title((const PumaPlayer*)user_data);
    for (int i = 0; i < 12 && name[i] >= 32 && name[i] < 127; i++) {
        title[i] = name[i];
    }
    if (title[0] != 0) {
        sink->set_tag(sink->ctx, NUADE_META_TITLE, title);
    }
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "PumaTracker");
}

#define PUMA_NUADE_ROWS 32

// Notes are byte offsets into the period table, 2 per semitone; offset 2 is the lowest note, 2 octaves below C-0.
static int32_t pumatracker_note(int note_byte) {
    return note_byte >= 2 ? note_byte / 2 - 1 : NUADE_CELL_EMPTY;
}

static NuadePositionResult pumatracker_position(void* user_data) {
    const PumaPosition p = puma_position((const PumaPlayer*)user_data);
    return (NuadePositionResult) {
        .valid = true,
        .position
        = { .mode = NuadeScroll_Rows, .order = (uint32_t)p.order, .row = (uint32_t)p.row, .speed = (uint32_t)p.speed }
    };
}

static uint32_t pumatracker_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const PumaPlayer* player = (const PumaPlayer*)user_data;
    const PumaPosition p = puma_position(player);
    const uint32_t n = cap < PUMA_VOICES ? cap : PUMA_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const PumaVoiceState v = puma_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note < 0 ? -1 : pumatracker_note(v.note),
                                    .instrument = v.instrument,
                                    .order = (uint32_t)p.order,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult pumatracker_tracker_info(void* user_data, const NuadePosition* at) {
    if ((int)at->order >= puma_position_count((const PumaPlayer*)user_data)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = PUMA_VOICES,
                                                .rows = PUMA_NUADE_ROWS,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3, -2 },
                                                             { "inst", NuadeColumn_Hex, 2 },
                                                             { "fx", NuadeColumn_Hex, 1 },
                                                             { "arg", NuadeColumn_Hex, 2 } } } };
}

static NuadePatternCell pumatracker_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const PumaCell c = puma_pattern_cell((const PumaPlayer*)ctx, (int)order, (int)channel, (int)row);
    cell.value[0] = pumatracker_note(c.note);
    if (c.note != 0 && c.instrument != 0) {
        cell.value[1] = c.instrument;
    }
    if (c.effect != 0) {
        cell.value[2] = c.effect;
        cell.value[3] = c.effect_arg;
    }
    return cell;
}

static uint32_t pumatracker_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                          int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    if ((int)at->order >= puma_position_count((const PumaPlayer*)user_data)) {
        return 0;
    }
    return nuade_tracker_fill(at, PUMA_VOICES, PUMA_NUADE_ROWS, channel, row_lo, row_hi, out, cap, pumatracker_cell,
                              user_data);
}

const NuadePlayer nuade_player_pumatracker = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "pumatracker",
    .name = "PumaTracker",
    .version = "1",
    .extensions = "puma",
    .probe = pumatracker_probe,
    .open = pumatracker_open,
    .close = pumatracker_close,
    .start = pumatracker_start,
    .tick = pumatracker_tick,
    .metadata = pumatracker_metadata,
    .position = pumatracker_position,
    .voice_info = pumatracker_voice_info,
    .tracker_info = pumatracker_tracker_info,
    .tracker_cells = pumatracker_tracker_cells,
};

PumaPlayer* puma_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_pumatracker) {
        return nullptr;
    }
    return (PumaPlayer*)nuade_player_data(song);
}
