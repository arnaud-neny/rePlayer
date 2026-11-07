// SPDX-License-Identifier: MIT
// Protracker4 as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/protracker4.h>
#include <stdio.h>

static NuadeProbe protracker4_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return pt4_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult protracker4_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    Pt4Player* player = pt4_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Protracker4 module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = PT4_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = PT4_TIMEBASE_HZ
    };
    return result;
}

static void protracker4_close(void* user_data) {
    pt4_destroy((Pt4Player*)user_data);
}

static bool protracker4_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && pt4_start((Pt4Player*)user_data, voices);
}

static NuadeTickResult protracker4_tick(void* user_data, NuadeVoices* voices) {
    const Pt4TickResult tick = pt4_tick((Pt4Player*)user_data, voices);
    return (NuadeTickResult) { .length = tick.length, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void protracker4_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const Pt4Player* player = (const Pt4Player*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Protracker4");
    if (pt4_title(player)[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_TITLE, pt4_title(player));
    }
}

static NuadePositionResult protracker4_position(void* user_data) {
    const Pt4Position p = pt4_position((const Pt4Player*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.order,
                                                 .row = (uint32_t)p.row,
                                                 .rows = PT4_ROWS,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t protracker4_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const Pt4Player* player = (const Pt4Player*)user_data;
    const Pt4Position p = pt4_position(player);
    const uint32_t n = cap < PT4_VOICES ? cap : PT4_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const Pt4VoiceState v = pt4_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note,
                                    .instrument = v.sample > 0 ? v.sample - 1 : -1,
                                    .order = (uint32_t)p.order,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult protracker4_tracker_info(void* user_data, const NuadePosition* at) {
    if ((int)at->order >= pt4_order_count((const Pt4Player*)user_data)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = PT4_VOICES,
                                                .rows = PT4_ROWS,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3, 0 },
                                                             { "smp", NuadeColumn_Hex, 2, 0 },
                                                             { "cmd", NuadeColumn_Hex, 1, 0 },
                                                             { "arg", NuadeColumn_Hex, 2, 0 } } } };
}

static NuadePatternCell protracker4_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const Pt4Cell c = pt4_pattern_cell((const Pt4Player*)ctx, (int)order, (int)channel, (int)row);
    if (c.period != 0) {
        cell.value[0] = nuade_period_to_note(c.period);
    }
    if (c.sample != 0) {
        cell.value[1] = c.sample;
    }
    if (c.effect != 0 || c.param != 0) {
        cell.value[2] = c.effect;
        cell.value[3] = c.param;
    }
    return cell;
}

static uint32_t protracker4_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                          int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const Pt4Player* player = (const Pt4Player*)user_data;
    if ((int)at->order >= pt4_order_count(player)) {
        return 0;
    }
    return nuade_tracker_fill(at, PT4_VOICES, PT4_ROWS, channel, row_lo, row_hi, out, cap, protracker4_cell, user_data);
}

const NuadePlayer nuade_player_protracker4 = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "protracker4",
    .name = "Protracker4",
    .version = "1",
    .extensions = "ptm",
    .probe = protracker4_probe,
    .open = protracker4_open,
    .close = protracker4_close,
    .start = protracker4_start,
    .tick = protracker4_tick,
    .metadata = protracker4_metadata,
    .position = protracker4_position,
    .voice_info = protracker4_voice_info,
    .tracker_info = protracker4_tracker_info,
    .tracker_cells = protracker4_tracker_cells,
};

Pt4Player* pt4_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_protracker4) {
        return nullptr;
    }
    return (Pt4Player*)nuade_player_data(song);
}
