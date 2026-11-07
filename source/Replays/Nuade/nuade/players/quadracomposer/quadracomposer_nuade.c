// SPDX-License-Identifier: MIT
// Quadra Composer (EMOD) as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/quadracomposer.h>
#include <stdio.h>
#include <stdlib.h>

#define QUADRACOMPOSER_SAMPLE_SLOTS 256

typedef struct QuadraComposerNuade {
    QcModule* module;
} QuadraComposerNuade;

// The FORM/EMODEMIC header with format version 1 is specific enough to be sure.
static NuadeProbe quadracomposer_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return qc_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult quadracomposer_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    QuadraComposerNuade* nuade = (QuadraComposerNuade*)calloc(1, sizeof(QuadraComposerNuade));
    if (nuade != nullptr) {
        nuade->module = qc_create(source->data, source->size);
    }
    if (nuade == nullptr || nuade->module == nullptr) {
        free(nuade);
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Quadra Composer module");
        return result;
    }
    result.user_data = nuade;
    result.info = (NuadeSongInfo) {
        .voice_count = QC_CHANNELS, .subsong_count = 1, .default_subsong = 0, .timebase_hz = QC_TIMEBASE_HZ
    };
    return result;
}

static void quadracomposer_close(void* user_data) {
    QuadraComposerNuade* nuade = (QuadraComposerNuade*)user_data;
    qc_destroy(nuade->module);
    free(nuade);
}

static bool quadracomposer_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return qc_start(((QuadraComposerNuade*)user_data)->module, (int)subsong, voices);
}

static NuadeTickResult quadracomposer_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    QcModule* module = ((QuadraComposerNuade*)user_data)->module;
    const bool looped = qc_tick(module);
    return (NuadeTickResult) { .length = qc_tick_length(module), .flags = looped ? NuadeTick_Looped : NuadeTick_None };
}

static void quadracomposer_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const QcModule* module = ((const QuadraComposerNuade*)user_data)->module;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Quadra Composer");
    if (qc_title(module)[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_TITLE, qc_title(module));
    }
    for (uint32_t i = 0; i < QUADRACOMPOSER_SAMPLE_SLOTS; i++) {
        const char* name = qc_sample_name(module, (int)i);
        if (name[0] != '\0') {
            sink->add_instrument(sink->ctx, i, name);
        }
    }
}

// The pattern notes count from C-1 = 0; NuadeColumn_Note counts semitones from C-0.
#define QUADRACOMPOSER_NOTE_BASE 12

static NuadePositionResult quadracomposer_position(void* user_data) {
    const QcPosition p = qc_position(((const QuadraComposerNuade*)user_data)->module);
    if (!p.valid) {
        return (NuadePositionResult) { 0 };
    }
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.position,
                                                 .pattern = (uint32_t)p.pattern,
                                                 .row = (uint32_t)p.row,
                                                 .tick = (uint32_t)p.tick,
                                                 .speed = (uint32_t)p.speed } };
}

static uint32_t quadracomposer_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const QcModule* module = ((const QuadraComposerNuade*)user_data)->module;
    const QcPosition p = qc_position(module);
    const uint32_t n = cap < QC_CHANNELS ? cap : QC_CHANNELS;
    for (uint32_t i = 0; i < n; i++) {
        const QcVoiceState v = qc_voice_state(module, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note != 0 ? v.note - 1 + QUADRACOMPOSER_NOTE_BASE : -1,
                                    .instrument = v.sample != 0 ? v.sample - 1 : -1,
                                    .order = (uint32_t)p.position,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static uint32_t quadracomposer_pattern_rows(const QcModule* module, uint32_t order) {
    const int pattern = qc_position_pattern(module, (int)order);
    return pattern < 0 ? 0 : (uint32_t)qc_get_pattern_rows(module, pattern);
}

static NuadeTrackerInfoResult quadracomposer_tracker_info(void* user_data, const NuadePosition* at) {
    const QcModule* module = ((const QuadraComposerNuade*)user_data)->module;
    const uint32_t rows = quadracomposer_pattern_rows(module, at->order);
    if (rows == 0) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = QC_CHANNELS,
                                                .rows = rows,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3 },
                                                             { "smp", NuadeColumn_Hex, 2 },
                                                             { "fx", NuadeColumn_Hex, 1 },
                                                             { "arg", NuadeColumn_Hex, 2 } } } };
}

static NuadePatternCell quadracomposer_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    const QcModule* module = (const QcModule*)ctx;
    NuadePatternCell cell = nuade_cell_empty();
    uint8_t sample, effect, arg;
    int8_t note;
    qc_get_cell(module, qc_position_pattern(module, (int)order), (int)row, (int)channel, &sample, &note, &effect, &arg);
    if (note >= 0) {
        cell.value[0] = note + QUADRACOMPOSER_NOTE_BASE;
    }
    if (sample != 0) {
        cell.value[1] = sample;
    }
    if (effect != 0 || arg != 0) {
        cell.value[2] = effect;
        cell.value[3] = arg;
    }
    return cell;
}

static uint32_t quadracomposer_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                             int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const QcModule* module = ((const QuadraComposerNuade*)user_data)->module;
    return nuade_tracker_fill(at, QC_CHANNELS, quadracomposer_pattern_rows(module, at->order), channel, row_lo, row_hi,
                              out, cap, quadracomposer_cell, module);
}

const NuadePlayer nuade_player_quadracomposer = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "quadracomposer",
    .name = "Quadra Composer",
    .version = "1",
    .extensions = "emod,qc",
    .probe = quadracomposer_probe,
    .open = quadracomposer_open,
    .close = quadracomposer_close,
    .start = quadracomposer_start,
    .tick = quadracomposer_tick,
    .metadata = quadracomposer_metadata,
    .position = quadracomposer_position,
    .voice_info = quadracomposer_voice_info,
    .tracker_info = quadracomposer_tracker_info,
    .tracker_cells = quadracomposer_tracker_cells,
};

QcModule* qc_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_quadracomposer) {
        return nullptr;
    }
    return ((QuadraComposerNuade*)nuade_player_data(song))->module;
}
