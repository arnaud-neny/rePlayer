// SPDX-License-Identifier: MIT
// DeltaMusic 1 as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/deltamusic1.h>
#include <stdio.h>

#define DELTAMUSIC1_VOICES 4

// The "ALL " mark together with part lengths that fit the file is specific enough to be sure.
static NuadeProbe deltamusic1_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return dm1_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult deltamusic1_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    Dm1Module* module = dm1_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid DeltaMusic 1 module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) { .voice_count = DELTAMUSIC1_VOICES,
                                    .subsong_count = (uint32_t)dm1_subsong_count(module),
                                    .default_subsong = 0,
                                    .timebase_hz = DM1_TICK_HZ };
    return result;
}

static void deltamusic1_close(void* user_data) {
    dm1_destroy((Dm1Module*)user_data);
}

static bool deltamusic1_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return dm1_start((Dm1Module*)user_data, (int)subsong, voices);
}

static NuadeTickResult deltamusic1_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    return (NuadeTickResult) { .length = 1,
                               .flags = dm1_tick((Dm1Module*)user_data) ? NuadeTick_Looped : NuadeTick_None };
}

static void deltamusic1_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "DeltaMusic 1");
}

// Voice 0 stands for the song; the other voices differ in NuadeVoiceInfo.
static NuadePositionResult deltamusic1_position(void* user_data) {
    const Dm1VoiceState v = dm1_voice_state((const Dm1Module*)user_data, 0);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)v.order,
                                                 .row = (uint32_t)v.row,
                                                 .rows = DM1_BLOCK_LINES,
                                                 .speed = (uint32_t)v.speed } };
}

static uint32_t deltamusic1_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const uint32_t n = cap < DELTAMUSIC1_VOICES ? cap : DELTAMUSIC1_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const Dm1VoiceState v = dm1_voice_state((const Dm1Module*)user_data, (int)i);
        out[i] = (NuadeVoiceInfo) {
            .note = v.note, .instrument = v.instrument, .order = (uint32_t)v.order, .row = (uint32_t)v.row
        };
    }
    return n;
}

static NuadeTrackerInfoResult deltamusic1_tracker_info(void* user_data, const NuadePosition* at) {
    if ((int)at->order >= dm1_order_count((const Dm1Module*)user_data)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = DELTAMUSIC1_VOICES,
                                                .rows = DM1_BLOCK_LINES,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3, DM1_NOTE_OCTAVE_SHIFT },
                                                             { "inst", NuadeColumn_Dec, 2 },
                                                             { "fx", NuadeColumn_Hex, 2 },
                                                             { "arg", NuadeColumn_Hex, 2 } } } };
}

static NuadePatternCell deltamusic1_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    const Dm1Cell c = dm1_cell((const Dm1Module*)ctx, (int)order, (int)channel, (int)row);
    NuadePatternCell cell = nuade_cell_empty();
    cell.value[0] = c.note;
    cell.value[1] = c.instrument;
    if (c.effect != 0) {
        cell.value[2] = c.effect;
        cell.value[3] = c.effect_arg;
    }
    return cell;
}

static uint32_t deltamusic1_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                          int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    return nuade_tracker_fill(at, DELTAMUSIC1_VOICES, DM1_BLOCK_LINES, channel, row_lo, row_hi, out, cap,
                              deltamusic1_cell, user_data);
}

const NuadePlayer nuade_player_deltamusic1 = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "deltamusic1",
    .name = "DeltaMusic 1",
    .version = "1",
    .extensions = "dm,dm1,dlm1", // rePlayer
    .probe = deltamusic1_probe,
    .open = deltamusic1_open,
    .close = deltamusic1_close,
    .start = deltamusic1_start,
    .tick = deltamusic1_tick,
    .metadata = deltamusic1_metadata,
    .position = deltamusic1_position,
    .voice_info = deltamusic1_voice_info,
    .tracker_info = deltamusic1_tracker_info,
    .tracker_cells = deltamusic1_tracker_cells,
};

Dm1Module* dm1_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_deltamusic1) {
        return nullptr;
    }
    return (Dm1Module*)nuade_player_data(song);
}
