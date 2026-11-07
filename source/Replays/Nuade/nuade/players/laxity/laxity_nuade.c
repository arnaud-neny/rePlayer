// SPDX-License-Identifier: MIT
// Laxity (Powertracker) as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/laxity.h>
#include <stdio.h>

// Note n is period table entry n, which starts two octaves below the C-0 period (1712 = entry 13): the Note column
// counts from one octave below C-0 (octave_shift -1), a voice's note is reported from C-0 (the lowest octave clamps).
#define LX_VOLUME_BYTE_NONE 0xFF // the volume byte of a cell with nothing in the column
#define LX_NOTE_C0 13
#define LX_COLUMN_SHIFT (-1)
#define LX_COLUMN_NOTE_BASE 1

static NuadeProbe laxity_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return lx_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult laxity_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    LxPlayer* player = lx_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Laxity module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = LX_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = LX_TIMEBASE_HZ
    };
    return result;
}

static void laxity_close(void* user_data) {
    lx_destroy((LxPlayer*)user_data);
}

static bool laxity_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && lx_start((LxPlayer*)user_data, voices);
}

static NuadeTickResult laxity_tick(void* user_data, NuadeVoices* voices) {
    const LxTickResult tick = lx_tick((LxPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = LX_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void laxity_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const LxPlayer* player = (const LxPlayer*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Laxity");
    sink->set_tag(sink->ctx, NUADE_META_TITLE, lx_title(player));
    for (int i = 1; i <= lx_instrument_count(player); i++) {
        sink->add_instrument(sink->ctx, (uint32_t)(i - 1), lx_instrument_name(player, i));
    }
}

static NuadePositionResult laxity_position(void* user_data) {
    const LxPosition p = lx_position((const LxPlayer*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.order,
                                                 .row = (uint32_t)p.row,
                                                 .rows = (uint32_t)p.rows,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t laxity_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const LxPlayer* player = (const LxPlayer*)user_data;
    const LxPosition p = lx_position(player);
    const uint32_t n = cap < LX_VOICES ? cap : LX_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const LxVoiceState v = lx_voice_state(player, (int)i);
        int note = -1;
        if (v.note != 0) {
            note = v.note > LX_NOTE_C0 ? v.note - LX_NOTE_C0 : 0;
        }
        out[i] = (NuadeVoiceInfo) { .note = note,
                                    .instrument = v.instrument > 0 ? v.instrument - 1 : -1,
                                    .order = (uint32_t)p.order,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult laxity_tracker_info(void* user_data, const NuadePosition* at) {
    const LxPlayer* player = (const LxPlayer*)user_data;
    if ((int)at->order >= lx_order_count(player)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = LX_VOICES,
                                                .rows = (uint32_t)lx_position(player).rows,
                                                .column_count = 5,
                                                .columns = { { "note", NuadeColumn_Note, 3, LX_COLUMN_SHIFT },
                                                             { "inst", NuadeColumn_Hex, 2, 0 },
                                                             { "vol", NuadeColumn_Hex, 2, 0 },
                                                             { "fx", NuadeColumn_Hex, 1, 0 },
                                                             { "arg", NuadeColumn_Hex, 2, 0 } } } };
}

static NuadePatternCell laxity_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const LxCell c = lx_pattern_cell((const LxPlayer*)ctx, (int)order, (int)channel, (int)row);
    if (c.note != 0) {
        cell.value[0] = c.note - LX_COLUMN_NOTE_BASE;
    }
    if (c.instrument != 0) {
        cell.value[1] = c.instrument;
    }
    if (c.volume_byte != LX_VOLUME_BYTE_NONE) {
        cell.value[2] = c.volume_byte;
    }
    if (c.effect != 0 || c.param != 0) {
        cell.value[3] = c.effect;
        cell.value[4] = c.param;
    }
    return cell;
}

static uint32_t laxity_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                     int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const LxPlayer* player = (const LxPlayer*)user_data;
    if ((int)at->order >= lx_order_count(player)) {
        return 0;
    }
    return nuade_tracker_fill(at, LX_VOICES, (uint32_t)lx_position(player).rows, channel, row_lo, row_hi, out, cap,
                              laxity_cell, user_data);
}

const NuadePlayer nuade_player_laxity = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "laxity",
    .name = "Laxity",
    .version = "1",
    .extensions = "pt,powt",
    .probe = laxity_probe,
    .open = laxity_open,
    .close = laxity_close,
    .start = laxity_start,
    .tick = laxity_tick,
    .metadata = laxity_metadata,
    .position = laxity_position,
    .voice_info = laxity_voice_info,
    .tracker_info = laxity_tracker_info,
    .tracker_cells = laxity_tracker_cells,
};

LxPlayer* lx_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_laxity) {
        return nullptr;
    }
    return (LxPlayer*)nuade_player_data(song);
}
