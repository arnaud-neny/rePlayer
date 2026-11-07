// SPDX-License-Identifier: MIT
// InStereo! 1.0 as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/instereo1.h>
#include <stdio.h>

#define INSTEREO1_NUADE_VOICES 4

// Notes are period table indexes; index 49 (period 856) is C-1, so the column counts from index 1 and spells three
// octaves down.
#define INSTEREO1_NOTE_BASE 1
#define INSTEREO1_NOTE_STOP 0x7f
#define INSTEREO1_OCTAVE_SHIFT (-3)

// The module mark is specific enough to be sure.
static NuadeProbe instereo1_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return is1_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult instereo1_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    Is1Module* module = is1_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid InStereo! 1.0 module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) {
        .voice_count = INSTEREO1_NUADE_VOICES,
        .subsong_count = (uint32_t)is1_subsong_count(module),
        .default_subsong = 0,
        .timebase_hz = IS1_TICK_HZ,
    };
    return result;
}

static void instereo1_close(void* user_data) {
    is1_destroy((Is1Module*)user_data);
}

static bool instereo1_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return is1_start((Is1Module*)user_data, (int)subsong, voices);
}

static NuadeTickResult instereo1_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    return (NuadeTickResult) { .length = 1,
                               .flags = is1_tick((Is1Module*)user_data) ? NuadeTick_Looped : NuadeTick_None };
}

static void instereo1_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const Is1Module* module = (const Is1Module*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "InStereo! 1.0");
    if (is1_title(module)[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_TITLE, is1_title(module));
    }
}

static NuadePositionResult instereo1_position(void* user_data) {
    const Is1Position p = is1_position((const Is1Module*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.position,
                                                 .row = (uint32_t)p.row,
                                                 .rows = (uint32_t)p.rows,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t instereo1_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const Is1Module* module = (const Is1Module*)user_data;
    const Is1Position p = is1_position(module);
    const uint32_t n = cap < INSTEREO1_NUADE_VOICES ? cap : INSTEREO1_NUADE_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const Is1VoiceState v = is1_voice_state(module, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note != 0 ? v.note - INSTEREO1_NOTE_BASE : -1,
                                    .instrument = v.instrument != 0 ? v.instrument - 1 : -1,
                                    .order = (uint32_t)p.position,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult instereo1_tracker_info(void* user_data, const NuadePosition* at) {
    const Is1Module* module = (const Is1Module*)user_data;
    if ((int)at->order >= is1_position_count(module)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = INSTEREO1_NUADE_VOICES,
                                                .rows = at->rows != 0 ? at->rows : (uint32_t)is1_default_rows(module),
                                                .column_count = 5,
                                                .columns = { { "note", NuadeColumn_Note, 3, INSTEREO1_OCTAVE_SHIFT },
                                                             { "inst", NuadeColumn_Hex, 2, 0 },
                                                             { "arp", NuadeColumn_Hex, 1, 0 },
                                                             { "fx", NuadeColumn_Hex, 1, 0 },
                                                             { "arg", NuadeColumn_Hex, 2, 0 } } } };
}

static NuadePatternCell instereo1_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const Is1Cell c = is1_pattern_cell((const Is1Module*)ctx, (int)order, (int)channel, (int)row);
    if (c.note == INSTEREO1_NOTE_STOP) {
        cell.value[0] = NUADE_NOTE_CUT;
    } else if (c.note != 0) {
        cell.value[0] = c.note - INSTEREO1_NOTE_BASE;
    }
    if (c.instrument != 0) {
        cell.value[1] = c.instrument;
    }
    if (c.arpeggio != 0) {
        cell.value[2] = c.arpeggio;
    }
    if (c.effect != 0 || c.effect_arg != 0) {
        cell.value[3] = c.effect;
        cell.value[4] = c.effect_arg;
    }
    return cell;
}

static uint32_t instereo1_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                        int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const uint32_t rows = at->rows != 0 ? at->rows : (uint32_t)is1_default_rows((const Is1Module*)user_data);
    return nuade_tracker_fill(at, INSTEREO1_NUADE_VOICES, rows, channel, row_lo, row_hi, out, cap, instereo1_cell,
                              user_data);
}

const NuadePlayer nuade_player_instereo1 = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "instereo1",
    .name = "InStereo! 1.0",
    .version = "1",
    .extensions = "is,is10",
    .probe = instereo1_probe,
    .open = instereo1_open,
    .close = instereo1_close,
    .start = instereo1_start,
    .tick = instereo1_tick,
    .metadata = instereo1_metadata,
    .position = instereo1_position,
    .voice_info = instereo1_voice_info,
    .tracker_info = instereo1_tracker_info,
    .tracker_cells = instereo1_tracker_cells,
};

Is1Module* is1_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_instereo1) {
        return nullptr;
    }
    return (Is1Module*)nuade_player_data(song);
}
