// SPDX-License-Identifier: MIT
// Oktalyzer as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/oktalyzer.h>
#include <stdio.h>

// The module mark is specific enough to be sure.
static NuadeProbe oktalyzer_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return okt_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult oktalyzer_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    OktModule* module = okt_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Oktalyzer module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) { .voice_count = (uint32_t)okt_channel_count(module),
                                    .subsong_count = 1,
                                    .default_subsong = 0,
                                    .timebase_hz = OKT_TIMEBASE_HZ };
    return result;
}

static void oktalyzer_close(void* user_data) {
    okt_destroy((OktModule*)user_data);
}

static bool oktalyzer_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return okt_start((OktModule*)user_data, (int)subsong, voices);
}

static NuadeTickResult oktalyzer_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    return (NuadeTickResult) { .length = OKT_TICK_LENGTH,
                               .flags = okt_tick((OktModule*)user_data) ? NuadeTick_Looped : NuadeTick_None };
}

static void oktalyzer_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const OktModule* module = (const OktModule*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Oktalyzer");
    const int count = okt_get_instrument_count(module);
    for (int i = 0; i < count; i++) {
        const char* name = okt_instrument_name(module, i);
        if (name[0] != '\0') {
            sink->add_instrument(sink->ctx, (uint32_t)i, name);
        }
    }
}

// The pattern notes count from C-1 = 1; NuadeColumn_Note counts semitones from C-0.
#define OKTALYZER_NOTE_BASE 11

static NuadePositionResult oktalyzer_position(void* user_data) {
    const OktPosition p = okt_position((const OktModule*)user_data);
    if (!p.valid) {
        return (NuadePositionResult) { 0 };
    }
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.song_pos,
                                                 .pattern = (uint32_t)p.pattern,
                                                 .row = (uint32_t)p.row,
                                                 .tick = (uint32_t)p.tick,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t oktalyzer_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const OktModule* module = (const OktModule*)user_data;
    const OktPosition p = okt_position(module);
    const uint32_t count = (uint32_t)okt_channel_count(module);
    const uint32_t n = cap < count ? cap : count;
    for (uint32_t i = 0; i < n; i++) {
        const OktVoiceState v = okt_voice_state(module, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note != 0 ? v.note + OKTALYZER_NOTE_BASE : -1,
                                    .instrument = v.note != 0 ? v.sample : -1,
                                    .order = (uint32_t)p.song_pos,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult oktalyzer_tracker_info(void* user_data, const NuadePosition* at) {
    const OktModule* module = (const OktModule*)user_data;
    const int pattern = okt_order_pattern(module, (int)at->order);
    if (pattern < 0) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = (uint32_t)okt_channel_count(module),
                                                .rows = (uint32_t)okt_get_pattern_rows(module, pattern),
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3 },
                                                             { "smp", NuadeColumn_Hex, 2 },
                                                             { "fx", NuadeColumn_Dec, 2 },
                                                             { "arg", NuadeColumn_Hex, 2 } } } };
}

static NuadePatternCell oktalyzer_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    const OktModule* module = (const OktModule*)ctx;
    NuadePatternCell cell = nuade_cell_empty();
    uint8_t note, sample, effect, arg;
    okt_get_cell(module, okt_order_pattern(module, (int)order), (int)row, (int)channel, &note, &sample, &effect, &arg);
    if (note != 0) {
        cell.value[0] = note + OKTALYZER_NOTE_BASE;
        cell.value[1] = sample;
    }
    if (effect != 0 || arg != 0) {
        cell.value[2] = effect;
        cell.value[3] = arg;
    }
    return cell;
}

static uint32_t oktalyzer_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                        int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const OktModule* module = (const OktModule*)user_data;
    const int pattern = okt_order_pattern(module, (int)at->order);
    const uint32_t rows = pattern < 0 ? 0 : (uint32_t)okt_get_pattern_rows(module, pattern);
    return nuade_tracker_fill(at, (uint32_t)okt_channel_count(module), rows, channel, row_lo, row_hi, out, cap,
                              oktalyzer_cell, module);
}

const NuadePlayer nuade_player_oktalyzer = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "oktalyzer",
    .name = "Oktalyzer",
    .version = "1",
    .extensions = "okt,okta",
    .probe = oktalyzer_probe,
    .open = oktalyzer_open,
    .close = oktalyzer_close,
    .start = oktalyzer_start,
    .tick = oktalyzer_tick,
    .metadata = oktalyzer_metadata,
    .position = oktalyzer_position,
    .voice_info = oktalyzer_voice_info,
    .tracker_info = oktalyzer_tracker_info,
    .tracker_cells = oktalyzer_tracker_cells,
};

OktModule* okt_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_oktalyzer) {
        return nullptr;
    }
    return (OktModule*)nuade_player_data(song);
}
