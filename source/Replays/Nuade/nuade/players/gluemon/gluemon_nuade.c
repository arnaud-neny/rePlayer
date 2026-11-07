// SPDX-License-Identifier: MIT
// GlueMon as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/gluemon.h>
#include <stdio.h>

// The first entry of the period table (3424) is the C one octave below C-0 (1712).
#define GM_NOTE_OCTAVE_SHIFT (-1)

static NuadeProbe gluemon_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return gm_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult gluemon_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    GmPlayer* player = gm_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a GlueMon song");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = GM_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = GM_TIMEBASE_HZ
    };
    return result;
}

static void gluemon_close(void* user_data) {
    gm_destroy((GmPlayer*)user_data);
}

static bool gluemon_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)subsong;
    (void)sample_rate;
    return gm_start((GmPlayer*)user_data, voices);
}

static NuadeTickResult gluemon_tick(void* user_data, NuadeVoices* voices) {
    const GmTickResult tick = gm_tick((GmPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = GM_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void gluemon_metadata(void* user_data, const NuadeMetadataSink* sink) {
    char title[GM_NAME_LENGTH + 1];
    gm_title((const GmPlayer*)user_data, title);
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "GlueMon");
    if (title[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_TITLE, title);
    }
}

static NuadePositionResult gluemon_position(void* user_data) {
    const GmPlayer* player = (const GmPlayer*)user_data;
    const GmPosition p = gm_position(player);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = p.order,
                                                 .row = p.row,
                                                 .rows = gm_row_count(player),
                                                 .tick = p.tick,
                                                 .speed = p.speed } };
}

static uint32_t gluemon_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const GmPlayer* player = (const GmPlayer*)user_data;
    const GmPosition p = gm_position(player);
    const uint32_t n = cap < GM_VOICES ? cap : GM_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        out[i]
            = (NuadeVoiceInfo) { .note = gm_voice_note(player, i), .instrument = -1, .order = p.order, .row = p.row };
    }
    return n;
}

static NuadeTrackerInfoResult gluemon_tracker_info(void* user_data, const NuadePosition* at) {
    const GmPlayer* player = (const GmPlayer*)user_data;
    if (at->order >= gm_order_count(player)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = GM_VOICES,
                                                .rows = gm_row_count(player),
                                                .column_count = 2,
                                                .columns = { { "note", NuadeColumn_Note, 3, GM_NOTE_OCTAVE_SHIFT },
                                                             { "cmd", NuadeColumn_Hex, 2, 0 } } } };
}

static NuadePatternCell gluemon_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const GmCell c = gm_cell((const GmPlayer*)ctx, order, channel, row);
    if (c.note >= 0) {
        cell.value[0] = c.note;
    } else if (c.note_off) {
        cell.value[0] = NUADE_NOTE_OFF;
    }
    if (c.command >= 0) {
        cell.value[1] = c.command;
    }
    return cell;
}

static uint32_t gluemon_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                      int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const GmPlayer* player = (const GmPlayer*)user_data;
    if (at->order >= gm_order_count(player)) {
        return 0;
    }
    return nuade_tracker_fill(at, GM_VOICES, gm_row_count(player), channel, row_lo, row_hi, out, cap, gluemon_cell,
                              user_data);
}

const NuadePlayer nuade_player_gluemon = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "gluemon",
    .name = "GlueMon",
    .version = "1",
    .extensions = "glue,gm",
    .probe = gluemon_probe,
    .open = gluemon_open,
    .close = gluemon_close,
    .start = gluemon_start,
    .tick = gluemon_tick,
    .metadata = gluemon_metadata,
    .position = gluemon_position,
    .voice_info = gluemon_voice_info,
    .tracker_info = gluemon_tracker_info,
    .tracker_cells = gluemon_tracker_cells,
};

GmPlayer* gm_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_gluemon) {
        return nullptr;
    }
    return (GmPlayer*)nuade_player_data(song);
}
