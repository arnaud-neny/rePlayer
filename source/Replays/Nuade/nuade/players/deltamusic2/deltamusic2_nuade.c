// SPDX-License-Identifier: MIT
// Delta Music 2 as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include "nuade_utils.h"
#include <nuade/players/deltamusic2.h>
#include <stdio.h>

#define DELTAMUSIC2_VOICES 4

// The ".FNL" mark is specific enough to be sure.
static NuadeProbe deltamusic2_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return dm2_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult deltamusic2_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    Dm2Module* module = dm2_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Delta Music 2 module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) { .voice_count = DELTAMUSIC2_VOICES,
                                    .subsong_count = (uint32_t)dm2_subsong_count(module),
                                    .default_subsong = 0,
                                    .timebase_hz = DM2_TICK_HZ };
    return result;
}

static void deltamusic2_close(void* user_data) {
    dm2_destroy((Dm2Module*)user_data);
}

static bool deltamusic2_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return dm2_start((Dm2Module*)user_data, (int)subsong, voices);
}

static NuadeTickResult deltamusic2_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    return (NuadeTickResult) { .length = 1,
                               .flags = dm2_tick((Dm2Module*)user_data) ? NuadeTick_Looped : NuadeTick_None };
}

static bool deltamusic2_dump_state(void* user_data, const char* path) {
    return dm2_set_dump_file((Dm2Module*)user_data, path);
}

static void deltamusic2_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Delta Music 2");
}

// Voice 0 stands for the song; the other voices differ in NuadeVoiceInfo.
static NuadePositionResult deltamusic2_position(void* user_data) {
    const Dm2VoiceState v = dm2_voice_state((const Dm2Module*)user_data, 0);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)v.order,
                                                 .row = (uint32_t)v.row,
                                                 .rows = DM2_BLOCK_ROWS,
                                                 .speed = (uint32_t)v.speed } };
}

static uint32_t deltamusic2_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const uint32_t n = cap < DELTAMUSIC2_VOICES ? cap : DELTAMUSIC2_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const Dm2VoiceState v = dm2_voice_state((const Dm2Module*)user_data, (int)i);
        out[i] = (NuadeVoiceInfo) {
            .note = v.note, .instrument = v.instrument, .order = (uint32_t)v.order, .row = (uint32_t)v.row
        };
    }
    return n;
}

static NuadeTrackerInfoResult deltamusic2_tracker_info(void* user_data, const NuadePosition* at) {
    if ((int)at->order >= dm2_order_count((const Dm2Module*)user_data)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = DELTAMUSIC2_VOICES,
                                                .rows = DM2_BLOCK_ROWS,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3, DM2_NOTE_OCTAVE_SHIFT },
                                                             { "inst", NuadeColumn_Dec, 2 },
                                                             { "fx", NuadeColumn_Hex, 2 },
                                                             { "arg", NuadeColumn_Hex, 2 } } } };
}

static NuadePatternCell deltamusic2_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    const Dm2Cell c = dm2_cell((const Dm2Module*)ctx, (int)order, (int)channel, (int)row);
    NuadePatternCell cell = nuade_cell_empty();
    cell.value[0] = c.note;
    cell.value[1] = c.instrument;
    if (c.effect != 0) {
        cell.value[2] = c.effect;
        cell.value[3] = c.effect_arg;
    }
    return cell;
}

static uint32_t deltamusic2_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                          int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    return nuade_tracker_fill(at, DELTAMUSIC2_VOICES, DM2_BLOCK_ROWS, channel, row_lo, row_hi, out, cap,
                              deltamusic2_cell, user_data);
}

const NuadePlayer nuade_player_deltamusic2 = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "deltamusic2",
    .name = "Delta Music 2",
    .version = "1",
    .extensions = "dm2",
    .probe = deltamusic2_probe,
    .open = deltamusic2_open,
    .close = deltamusic2_close,
    .start = deltamusic2_start,
    .tick = deltamusic2_tick,
    .metadata = deltamusic2_metadata,
    .position = deltamusic2_position,
    .voice_info = deltamusic2_voice_info,
    .tracker_info = deltamusic2_tracker_info,
    .tracker_cells = deltamusic2_tracker_cells,
    .dump_state = deltamusic2_dump_state,
};

Dm2Module* dm2_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_deltamusic2) {
        return nullptr;
    }
    return (Dm2Module*)nuade_player_data(song);
}
