// SPDX-License-Identifier: MIT
// JamCracker format as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/jamcracker.h>
#include <stdio.h>

static NuadeProbe jamcracker_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return jc_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult jamcracker_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    JcPlayer* player = jc_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid JamCracker module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = JC_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = JC_TIMEBASE_HZ
    };
    return result;
}

static void jamcracker_close(void* user_data) {
    jc_destroy((JcPlayer*)user_data);
}

static bool jamcracker_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && jc_start((JcPlayer*)user_data, voices);
}

static NuadeTickResult jamcracker_tick(void* user_data, NuadeVoices* voices) {
    const JcTickResult tick = jc_tick((JcPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = JC_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void jamcracker_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const JcPlayer* player = (const JcPlayer*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "JamCracker");
    for (int i = 0; i < jc_instrument_count(player); i++) {
        const char* name = jc_instrument_name(player, i);
        if (name[0] != '\0') {
            sink->add_instrument(sink->ctx, (uint32_t)i, name);
        }
    }
}

static NuadePositionResult jamcracker_position(void* user_data) {
    const JcPosition p = jc_position((const JcPlayer*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.order,
                                                 .row = (uint32_t)p.row,
                                                 .rows = (uint32_t)p.rows,
                                                 .speed = (uint32_t)p.speed } };
}

// Note byte n is period table entry n - 1; entry 3 (857) is C-1.
static int32_t jamcracker_note(int note_byte) {
    return note_byte != 0 ? note_byte + 8 : NUADE_CELL_EMPTY;
}

static uint32_t jamcracker_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const JcPlayer* player = (const JcPlayer*)user_data;
    const JcPosition p = jc_position(player);
    const uint32_t n = cap < JC_VOICES ? cap : JC_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const JcVoiceState v = jc_voice_state(player, (int)i);
        const bool has_instrument = v.note != 0 && v.instrument >= 0 && v.instrument < jc_instrument_count(player);
        out[i] = (NuadeVoiceInfo) { .note = jamcracker_note(v.note),
                                    .instrument = has_instrument ? v.instrument : -1,
                                    .order = (uint32_t)p.order,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult jamcracker_tracker_info(void* user_data, const NuadePosition* at) {
    const JcPlayer* player = (const JcPlayer*)user_data;
    if ((int)at->order >= jc_order_count(player)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = JC_VOICES,
                                                .rows = (uint32_t)jc_pattern_rows(player, (int)at->order),
                                                .column_count = 8,
                                                .columns = { { "note", NuadeColumn_Note, 3 },
                                                             { "inst", NuadeColumn_Hex, 2 },
                                                             { "spd", NuadeColumn_Hex, 2 },
                                                             { "arp", NuadeColumn_Hex, 2 },
                                                             { "vib", NuadeColumn_Hex, 2 },
                                                             { "pha", NuadeColumn_Hex, 2 },
                                                             { "vol", NuadeColumn_Hex, 2 },
                                                             { "por", NuadeColumn_Hex, 2 } } } };
}

static NuadePatternCell jamcracker_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const JcCell c = jc_pattern_cell((const JcPlayer*)ctx, (int)order, (int)channel, (int)row);
    cell.value[0] = jamcracker_note(c.period);
    if (c.period != 0) {
        cell.value[1] = c.instrument;
    }
    const uint8_t fields[] = { c.speed, c.arpeggio, c.vibrato, c.phase, c.volume, c.porta };
    for (uint32_t i = 0; i < 6; i++) {
        if (fields[i] != 0) {
            cell.value[2 + i] = fields[i];
        }
    }
    return cell;
}

static uint32_t jamcracker_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                         int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const JcPlayer* player = (const JcPlayer*)user_data;
    if ((int)at->order >= jc_order_count(player)) {
        return 0;
    }
    return nuade_tracker_fill(at, JC_VOICES, (uint32_t)jc_pattern_rows(player, (int)at->order), channel, row_lo, row_hi,
                              out, cap, jamcracker_cell, user_data);
}

const NuadePlayer nuade_player_jamcracker = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "jamcracker",
    .name = "JamCracker",
    .version = "1",
    .extensions = "jam,jc",
    .probe = jamcracker_probe,
    .open = jamcracker_open,
    .close = jamcracker_close,
    .start = jamcracker_start,
    .tick = jamcracker_tick,
    .metadata = jamcracker_metadata,
    .position = jamcracker_position,
    .voice_info = jamcracker_voice_info,
    .tracker_info = jamcracker_tracker_info,
    .tracker_cells = jamcracker_tracker_cells,
};

JcPlayer* jc_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_jamcracker) {
        return nullptr;
    }
    return (JcPlayer*)nuade_player_data(song);
}
