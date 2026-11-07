// SPDX-License-Identifier: MIT
// ZoundMonitor as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/zoundmonitor.h>
#include <stdio.h>

// Note n is period table entry n; entry 1 (856) is C-1.
#define ZM_NOTE_BASE 11
#define ZM_NOTE_REST 0x3F

static NuadeProbe zoundmonitor_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return zm_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult zoundmonitor_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const ZmCreateResult created = zm_create(source->data, source->size, source->path, source->io);
    switch (created.status) {
        case ZmStatus_Ok:
            result.user_data = created.player;
            result.info = (NuadeSongInfo) {
                .voice_count = ZM_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = ZM_TIMEBASE_HZ
            };
            break;
        case ZmStatus_MissingSample:
            result.error.status = NuadeStatus_IoError;
            snprintf(result.error.message, sizeof(result.error.message), "sample \"%s\" not found in Samples/",
                     created.missing_sample);
            break;
        case ZmStatus_OutOfMemory:
            result.error.status = NuadeStatus_OutOfMemory;
            snprintf(result.error.message, sizeof(result.error.message), "out of memory");
            break;
        case ZmStatus_InvalidData:
            result.error.status = NuadeStatus_InvalidData;
            snprintf(result.error.message, sizeof(result.error.message), "not a valid ZoundMonitor module");
            break;
    }
    return result;
}

static void zoundmonitor_close(void* user_data) {
    zm_destroy((ZmPlayer*)user_data);
}

static bool zoundmonitor_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && zm_start((ZmPlayer*)user_data, voices);
}

static NuadeTickResult zoundmonitor_tick(void* user_data, NuadeVoices* voices) {
    const ZmTickResult tick = zm_tick((ZmPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = ZM_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void zoundmonitor_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const ZmPlayer* player = (const ZmPlayer*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "ZoundMonitor");
    for (int i = 0; i < ZM_INSTRUMENTS; i++) {
        const char* name = zm_instrument_name(player, i);
        if (name[0] != '\0') {
            sink->add_instrument(sink->ctx, (uint32_t)i, name);
        }
    }
}

static NuadePositionResult zoundmonitor_position(void* user_data) {
    const ZmPosition p = zm_position((const ZmPlayer*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.order,
                                                 .row = (uint32_t)p.row,
                                                 .rows = ZM_ROWS,
                                                 .speed = (uint32_t)p.speed } };
}

static int32_t zoundmonitor_note(int note) {
    if (note == 0) {
        return NUADE_CELL_EMPTY;
    }
    return note == ZM_NOTE_REST ? NUADE_NOTE_CUT : note + ZM_NOTE_BASE;
}

static uint32_t zoundmonitor_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const ZmPlayer* player = (const ZmPlayer*)user_data;
    const ZmPosition p = zm_position(player);
    const uint32_t n = cap < ZM_VOICES ? cap : ZM_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const ZmVoiceState v = zm_voice_state(player, (int)i);
        const bool valid_note = v.note != 0 && v.note != ZM_NOTE_REST;
        out[i] = (NuadeVoiceInfo) { .note = valid_note ? v.note + ZM_NOTE_BASE : -1,
                                    .instrument = v.instrument > 0 ? v.instrument - 1 : -1,
                                    .order = (uint32_t)p.order,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult zoundmonitor_tracker_info(void* user_data, const NuadePosition* at) {
    const ZmPlayer* player = (const ZmPlayer*)user_data;
    if ((int)at->order >= zm_order_count(player)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = ZM_VOICES,
                                                .rows = ZM_ROWS,
                                                .column_count = 5,
                                                .columns = { { "note", NuadeColumn_Note, 3, 0 },
                                                             { "inst", NuadeColumn_Hex, 1, 0 },
                                                             { "ctl", NuadeColumn_Hex, 1, 0 },
                                                             { "vol", NuadeColumn_Hex, 2, 0 },
                                                             { "arg", NuadeColumn_Hex, 2, 0 } } } };
}

static NuadePatternCell zoundmonitor_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const ZmCell c = zm_pattern_cell((const ZmPlayer*)ctx, (int)order, (int)channel, (int)row);
    cell.value[0] = zoundmonitor_note(c.note);
    if (c.instrument != 0) {
        cell.value[1] = c.instrument;
    }
    if (c.control != 0) {
        cell.value[2] = c.control;
    }
    if (c.volume != 0) {
        cell.value[3] = (uint8_t)c.volume;
    }
    if (c.param != 0) {
        cell.value[4] = c.param;
    }
    return cell;
}

static uint32_t zoundmonitor_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                           int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const ZmPlayer* player = (const ZmPlayer*)user_data;
    if ((int)at->order >= zm_order_count(player)) {
        return 0;
    }
    return nuade_tracker_fill(at, ZM_VOICES, ZM_ROWS, channel, row_lo, row_hi, out, cap, zoundmonitor_cell, user_data);
}

const NuadePlayer nuade_player_zoundmonitor = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "zoundmonitor",
    .name = "ZoundMonitor",
    .version = "1",
    .extensions = "sng,zm",
    .probe = zoundmonitor_probe,
    .open = zoundmonitor_open,
    .close = zoundmonitor_close,
    .start = zoundmonitor_start,
    .tick = zoundmonitor_tick,
    .metadata = zoundmonitor_metadata,
    .position = zoundmonitor_position,
    .voice_info = zoundmonitor_voice_info,
    .tracker_info = zoundmonitor_tracker_info,
    .tracker_cells = zoundmonitor_tracker_cells,
};

ZmPlayer* zm_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_zoundmonitor) {
        return nullptr;
    }
    return (ZmPlayer*)nuade_player_data(song);
}
