// SPDX-License-Identifier: MIT
// Face The Music (FTM) as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/facethemusic.h>
#include <stdio.h>

// Track notes count from C-1 = 1; NuadeColumn_Note counts semitones from C-0. 35 releases the note.
#define FACETHEMUSIC_NOTE_BASE 11
#define FACETHEMUSIC_NOTE_RELEASE 35

// The FTM mark together with a consistent header is specific enough to be sure.
static NuadeProbe facethemusic_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return ftm_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult facethemusic_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    FtmModule* module = ftm_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Face The Music module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) {
        .voice_count = (uint32_t)ftm_channel_count(module),
        .subsong_count = (uint32_t)ftm_subsong_count(module),
        .default_subsong = 0,
        .timebase_hz = FTM_TIMEBASE_HZ,
    };
    return result;
}

static void facethemusic_close(void* user_data) {
    ftm_destroy((FtmModule*)user_data);
}

static bool facethemusic_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return ftm_start((FtmModule*)user_data, (int)subsong, voices);
}

static NuadeTickResult facethemusic_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    FtmModule* module = (FtmModule*)user_data;
    const bool looped = ftm_tick(module);
    return (NuadeTickResult) { .length = ftm_tick_length(module), .flags = looped ? NuadeTick_Looped : NuadeTick_None };
}

static void facethemusic_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const FtmModule* module = (const FtmModule*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Face The Music");
    if (ftm_title(module)[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_TITLE, ftm_title(module));
    }
    if (ftm_artist(module)[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_ARTIST, ftm_artist(module));
    }
    const int count = ftm_get_instrument_count(module);
    for (int i = 0; i < count; i++) {
        const char* name = ftm_get_instrument_name(module, i);
        if (name[0] != '\0') {
            sink->add_instrument(sink->ctx, (uint32_t)i, name);
        }
    }
}

static NuadePositionResult facethemusic_position(void* user_data) {
    const FtmModule* module = (const FtmModule*)user_data;
    const FtmPosition p = ftm_position(module);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.measure,
                                                 .row = (uint32_t)p.row,
                                                 .tick = (uint32_t)p.tick,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t facethemusic_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const FtmModule* module = (const FtmModule*)user_data;
    const FtmPosition p = ftm_position(module);
    const uint32_t count = (uint32_t)ftm_channel_count(module);
    const uint32_t n = cap < count ? cap : count;
    for (uint32_t i = 0; i < n; i++) {
        const FtmVoiceState v = ftm_voice_state(module, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note != 0 ? v.note + FACETHEMUSIC_NOTE_BASE : -1,
                                    .instrument = v.sample,
                                    .order = (uint32_t)p.measure,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult facethemusic_tracker_info(void* user_data, const NuadePosition* at) {
    const FtmModule* module = (const FtmModule*)user_data;
    if ((int)at->order >= ftm_get_num_measures(module)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = (uint32_t)ftm_channel_count(module),
                                                .rows = (uint32_t)ftm_get_rows_per_measure(module),
                                                .column_count = 3,
                                                .columns = { { "note", NuadeColumn_Note, 3 },
                                                             { "fx", NuadeColumn_Hex, 1 },
                                                             { "arg", NuadeColumn_Dec, 2 } } } };
}

static NuadePatternCell facethemusic_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    const FtmModule* module = (const FtmModule*)ctx;
    NuadePatternCell cell = nuade_cell_empty();
    const int track = ftm_voice_track(module, (int)channel);
    if (track < 0) {
        return cell;
    }
    uint8_t note = 0;
    uint8_t effect = 0;
    uint16_t arg = 0;
    ftm_get_cell(module, track, (int)(order * (uint32_t)ftm_get_rows_per_measure(module) + row), &note, &effect, &arg);
    if (note == FACETHEMUSIC_NOTE_RELEASE) {
        cell.value[0] = NUADE_NOTE_OFF;
    } else if (note != 0) {
        cell.value[0] = note + FACETHEMUSIC_NOTE_BASE;
    }
    if (effect != 0 || arg != 0) {
        cell.value[1] = effect;
        cell.value[2] = arg;
    }
    return cell;
}

static uint32_t facethemusic_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                           int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const FtmModule* module = (const FtmModule*)user_data;
    return nuade_tracker_fill(at, (uint32_t)ftm_channel_count(module), (uint32_t)ftm_get_rows_per_measure(module),
                              channel, row_lo, row_hi, out, cap, facethemusic_cell, module);
}

const NuadePlayer nuade_player_facethemusic = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "facethemusic",
    .name = "Face The Music",
    .version = "1",
    .extensions = "ftm",
    .probe = facethemusic_probe,
    .open = facethemusic_open,
    .close = facethemusic_close,
    .start = facethemusic_start,
    .tick = facethemusic_tick,
    .metadata = facethemusic_metadata,
    .position = facethemusic_position,
    .voice_info = facethemusic_voice_info,
    .tracker_info = facethemusic_tracker_info,
    .tracker_cells = facethemusic_tracker_cells,
};

FtmModule* ftm_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_facethemusic) {
        return nullptr;
    }
    return (FtmModule*)nuade_player_data(song);
}
