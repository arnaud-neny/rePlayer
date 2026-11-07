// SPDX-License-Identifier: MIT
// Sonic Arranger as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/sonicarranger.h>
#include <stdio.h>

// Both the module mark and the compiled block's offset header are specific enough to be sure.
static NuadeProbe sonicarranger_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return sa_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult sonicarranger_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    SaModule* module = sa_create(source->data, source->size);
    if (module == nullptr || sa_subsong_count(module) <= 0) {
        sa_destroy(module);
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Sonic Arranger module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) { .voice_count = SA_VOICES,
                                    .subsong_count = (uint32_t)sa_subsong_count(module),
                                    .default_subsong = 0,
                                    .timebase_hz = SA_TIMEBASE_HZ };
    return result;
}

static void sonicarranger_close(void* user_data) {
    sa_destroy((SaModule*)user_data);
}

static bool sonicarranger_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return sa_start((SaModule*)user_data, (int)subsong, voices);
}

static NuadeTickResult sonicarranger_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    SaModule* module = (SaModule*)user_data;
    const bool looped = sa_tick(module);
    return (NuadeTickResult) { .length = sa_tick_length(module), .flags = looped ? NuadeTick_Looped : NuadeTick_None };
}

static void sonicarranger_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const SaModule* module = (const SaModule*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Sonic Arranger");
    const int count = sa_get_instrument_count(module);
    for (int i = 0; i < count; i++) {
        const char* name = sa_get_instrument_name(module, i);
        if (name != nullptr && name[0] != '\0') {
            sink->add_instrument(sink->ctx, (uint32_t)i, name);
        }
    }
}

// The notes are period table indexes; 49 is the 856 period of ProTracker's C-1. NuadeColumn_Note counts semitones from
// C-0, so the lowest note is value 0 and the column spells it three octaves down.
#define SONICARRANGER_NOTE_BASE (-1)
#define SONICARRANGER_NOTE_CUT 0x7f
#define SONICARRANGER_NOTE_RELEASE 0x80

static NuadePositionResult sonicarranger_position(void* user_data) {
    const SaPosition p = sa_position((const SaModule*)user_data);
    if (!p.valid) {
        return (NuadePositionResult) { 0 };
    }
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.position,
                                                 .row = (uint32_t)p.row,
                                                 .rows = (uint32_t)p.rows,
                                                 .tick = (uint32_t)p.tick,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t sonicarranger_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const SaModule* module = (const SaModule*)user_data;
    const SaPosition p = sa_position(module);
    const uint32_t n = cap < SA_VOICES ? cap : SA_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const SaVoiceState v = sa_voice_state(module, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note != 0 ? v.note + SONICARRANGER_NOTE_BASE : -1,
                                    .instrument = v.instrument != 0 ? v.instrument - 1 : -1,
                                    .order = (uint32_t)p.position,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

// The rows of the pattern a position asks for: the count it carries, else the subsong's; none outside the song.
static uint32_t sonicarranger_rows(const SaModule* module, const NuadePosition* at) {
    if ((int)at->order >= sa_get_num_positions(module)) {
        return 0;
    }
    return at->rows != 0 ? at->rows : (uint32_t)sa_track_rows(module);
}

static NuadeTrackerInfoResult sonicarranger_tracker_info(void* user_data, const NuadePosition* at) {
    const uint32_t rows = sonicarranger_rows((const SaModule*)user_data, at);
    if (rows == 0) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = SA_VOICES,
                                                .rows = rows,
                                                .column_count = 5,
                                                .columns = { { "note", NuadeColumn_Note, 3, -3 },
                                                             { "ins", NuadeColumn_Hex, 2 },
                                                             { "arp", NuadeColumn_Dec, 1 },
                                                             { "fx", NuadeColumn_Hex, 1 },
                                                             { "arg", NuadeColumn_Hex, 2 } } } };
}

static NuadePatternCell sonicarranger_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const SaCell c = sa_pattern_cell((const SaModule*)ctx, (int)order, (int)channel, (int)row);
    if (c.note == SONICARRANGER_NOTE_CUT) {
        cell.value[0] = NUADE_NOTE_CUT;
    } else if (c.note == SONICARRANGER_NOTE_RELEASE) {
        cell.value[0] = NUADE_NOTE_OFF;
    } else if (c.note != 0) {
        cell.value[0] = c.note + SONICARRANGER_NOTE_BASE;
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

static uint32_t sonicarranger_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                            int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const SaModule* module = (const SaModule*)user_data;
    return nuade_tracker_fill(at, SA_VOICES, sonicarranger_rows(module, at), channel, row_lo, row_hi, out, cap,
                              sonicarranger_cell, module);
}

const NuadePlayer nuade_player_sonicarranger = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "sonicarranger",
    .name = "Sonic Arranger",
    .version = "1",
    .extensions = "sa,sonic",
    .probe = sonicarranger_probe,
    .open = sonicarranger_open,
    .close = sonicarranger_close,
    .start = sonicarranger_start,
    .tick = sonicarranger_tick,
    .metadata = sonicarranger_metadata,
    .position = sonicarranger_position,
    .voice_info = sonicarranger_voice_info,
    .tracker_info = sonicarranger_tracker_info,
    .tracker_cells = sonicarranger_tracker_cells,
};

SaModule* sa_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_sonicarranger) {
        return nullptr;
    }
    return (SaModule*)nuade_player_data(song);
}
