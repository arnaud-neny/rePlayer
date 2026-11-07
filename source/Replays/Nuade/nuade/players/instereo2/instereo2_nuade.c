// SPDX-License-Identifier: MIT
// InStereo! 2.0 as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include "nuade_utils.h"
#include <nuade/players/instereo2.h>
#include <stdio.h>

#define INSTEREO2_NUADE_VOICES 4

// Notes are period table indexes; index 49 (period 856) is C-1, so the column counts from index 1 and spells three
// octaves down.
#define INSTEREO2_NOTE_BASE 1
#define INSTEREO2_NOTE_STOP 0x7f
#define INSTEREO2_NOTE_NONE 0x80
#define INSTEREO2_OCTAVE_SHIFT (-3)

// The module mark is specific enough to be sure.
static NuadeProbe instereo2_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return is2_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult instereo2_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    Is2Module* module = is2_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid InStereo! 2.0 module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) {
        .voice_count = INSTEREO2_NUADE_VOICES,
        .subsong_count = (uint32_t)is2_subsong_count(module),
        .default_subsong = 0,
        .timebase_hz = IS2_TIMEBASE_HZ,
    };
    return result;
}

static void instereo2_close(void* user_data) {
    is2_destroy((Is2Module*)user_data);
}

static bool instereo2_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return is2_start((Is2Module*)user_data, (int)subsong, voices);
}

static NuadeTickResult instereo2_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    Is2Module* module = (Is2Module*)user_data;
    const bool looped = is2_tick(module);
    return (NuadeTickResult) { .length = is2_tick_length(module), .flags = looped ? NuadeTick_Looped : NuadeTick_None };
}

static bool instereo2_dump_state(void* user_data, const char* path) {
    return is2_set_dump_file((Is2Module*)user_data, path);
}

static void instereo2_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "InStereo! 2.0");
}

static NuadePositionResult instereo2_position(void* user_data) {
    const Is2Position p = is2_position((const Is2Module*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.position,
                                                 .row = (uint32_t)p.row,
                                                 .rows = (uint32_t)p.rows,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t instereo2_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const Is2Module* module = (const Is2Module*)user_data;
    const Is2Position p = is2_position(module);
    const uint32_t n = cap < INSTEREO2_NUADE_VOICES ? cap : INSTEREO2_NUADE_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const Is2VoiceState v = is2_voice_state(module, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note != 0 ? v.note - INSTEREO2_NOTE_BASE : -1,
                                    .instrument = v.instrument != 0 ? v.instrument - 1 : -1,
                                    .order = (uint32_t)p.position,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult instereo2_tracker_info(void* user_data, const NuadePosition* at) {
    const Is2Module* module = (const Is2Module*)user_data;
    if ((int)at->order >= is2_position_count(module)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = INSTEREO2_NUADE_VOICES,
                                                .rows = at->rows != 0 ? at->rows : (uint32_t)is2_default_rows(module),
                                                .column_count = 5,
                                                .columns = { { "note", NuadeColumn_Note, 3, INSTEREO2_OCTAVE_SHIFT },
                                                             { "inst", NuadeColumn_Hex, 2, 0 },
                                                             { "arp", NuadeColumn_Hex, 1, 0 },
                                                             { "fx", NuadeColumn_Hex, 1, 0 },
                                                             { "arg", NuadeColumn_Hex, 2, 0 } } } };
}

static NuadePatternCell instereo2_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const Is2Cell c = is2_pattern_cell((const Is2Module*)ctx, (int)order, (int)channel, (int)row);
    if (c.note == INSTEREO2_NOTE_STOP) {
        cell.value[0] = NUADE_NOTE_CUT;
    } else if (c.note != 0 && c.note != INSTEREO2_NOTE_NONE) {
        cell.value[0] = c.note - INSTEREO2_NOTE_BASE;
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

static uint32_t instereo2_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                        int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const uint32_t rows = at->rows != 0 ? at->rows : (uint32_t)is2_default_rows((const Is2Module*)user_data);
    return nuade_tracker_fill(at, INSTEREO2_NUADE_VOICES, rows, channel, row_lo, row_hi, out, cap, instereo2_cell,
                              user_data);
}

const NuadePlayer nuade_player_instereo2 = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "instereo2",
    .name = "InStereo! 2.0",
    .version = "1",
    .extensions = "is20",
    .probe = instereo2_probe,
    .open = instereo2_open,
    .close = instereo2_close,
    .start = instereo2_start,
    .tick = instereo2_tick,
    .metadata = instereo2_metadata,
    .position = instereo2_position,
    .voice_info = instereo2_voice_info,
    .tracker_info = instereo2_tracker_info,
    .tracker_cells = instereo2_tracker_cells,
    .dump_state = instereo2_dump_state,
};

Is2Module* is2_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_instereo2) {
        return nullptr;
    }
    return (Is2Module*)nuade_player_data(song);
}
