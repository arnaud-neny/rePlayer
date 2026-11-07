// SPDX-License-Identifier: MIT
// The Musical Enlightenment as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/musicalenlightenment.h>
#include <stdio.h>

// Period table index 0 (856) is C-1.
#define TME_NOTE_BASE 12

static NuadeProbe musicalenlightenment_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return tme_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult musicalenlightenment_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    TmePlayer* player = tme_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a module this player models exactly");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = TME_VOICES,
                                    .subsong_count = (uint32_t)tme_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = TME_TIMEBASE_HZ };
    return result;
}

static void musicalenlightenment_close(void* user_data) {
    tme_destroy((TmePlayer*)user_data);
}

static bool musicalenlightenment_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return tme_start((TmePlayer*)user_data, subsong, voices);
}

static NuadeTickResult musicalenlightenment_tick(void* user_data, NuadeVoices* voices) {
    const TmeTickResult tick = tme_tick((TmePlayer*)user_data, voices);
    return (NuadeTickResult) { .length = TME_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void musicalenlightenment_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "The Musical Enlightenment");
}

static NuadePositionResult musicalenlightenment_position(void* user_data) {
    const TmePosition p = tme_position((const TmePlayer*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.order,
                                                 .row = (uint32_t)p.row,
                                                 .rows = TME_ROWS,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t musicalenlightenment_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const TmePlayer* player = (const TmePlayer*)user_data;
    const TmePosition p = tme_position(player);
    const uint32_t n = cap < TME_VOICES ? cap : TME_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const TmeVoiceState v = tme_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note >= 0 ? v.note + TME_NOTE_BASE : -1,
                                    .instrument = v.instrument,
                                    .order = (uint32_t)p.order,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult musicalenlightenment_tracker_info(void* user_data, const NuadePosition* at) {
    const TmePlayer* player = (const TmePlayer*)user_data;
    if (at->order >= (uint32_t)tme_order_count(player)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = TME_VOICES,
                                                .rows = TME_ROWS,
                                                .column_count = 6,
                                                .columns = { { "note", NuadeColumn_Note, 3, 0 },
                                                             { "inst", NuadeColumn_Hex, 2, 0 },
                                                             { "list", NuadeColumn_Hex, 2, 0 },
                                                             { "vol", NuadeColumn_Hex, 2, 0 },
                                                             { "flg", NuadeColumn_Hex, 2, 0 },
                                                             { "arg", NuadeColumn_Hex, 2, 0 } } } };
}

static NuadePatternCell musicalenlightenment_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const TmeCell c = tme_pattern_cell((const TmePlayer*)ctx, order, channel, row);
    const uint8_t note = c.note & 0x7F;
    if (note != 0 && note < 0x7E) {
        cell.value[0] = note - 1 + TME_NOTE_BASE;
    }
    if (c.instrument != 0) {
        cell.value[1] = c.instrument;
    }
    if (c.list != 0) {
        cell.value[2] = c.list;
    }
    if (c.volume != 0) {
        cell.value[3] = (uint8_t)c.volume;
    }
    if (c.flags != 0) {
        cell.value[4] = c.flags;
    }
    if (c.param != 0) {
        cell.value[5] = c.param;
    }
    return cell;
}

static uint32_t musicalenlightenment_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel,
                                                   int32_t row_lo, int32_t row_hi, NuadePatternCell* out,
                                                   uint32_t cap) {
    const TmePlayer* player = (const TmePlayer*)user_data;
    if (at->order >= (uint32_t)tme_order_count(player)) {
        return 0;
    }
    return nuade_tracker_fill(at, TME_VOICES, TME_ROWS, channel, row_lo, row_hi, out, cap, musicalenlightenment_cell,
                              user_data);
}

const NuadePlayer nuade_player_musicalenlightenment = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "musicalenlightenment",
    .name = "The Musical Enlightenment",
    .version = "1",
    .extensions = "tme",
    .probe = musicalenlightenment_probe,
    .open = musicalenlightenment_open,
    .close = musicalenlightenment_close,
    .start = musicalenlightenment_start,
    .tick = musicalenlightenment_tick,
    .metadata = musicalenlightenment_metadata,
    .position = musicalenlightenment_position,
    .voice_info = musicalenlightenment_voice_info,
    .tracker_info = musicalenlightenment_tracker_info,
    .tracker_cells = musicalenlightenment_tracker_cells,
};

TmePlayer* tme_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_musicalenlightenment) {
        return nullptr;
    }
    return (TmePlayer*)nuade_player_data(song);
}
