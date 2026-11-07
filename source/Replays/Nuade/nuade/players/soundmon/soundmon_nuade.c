// SPDX-License-Identifier: MIT
// BP SoundMon 1.1 / 2.2 as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/soundmon.h>
#include <stdio.h>

#define SOUNDMON_NUADE_VOICES 4
#define SOUNDMON_INSTRUMENTS 15

// The module mark is specific enough to be sure.
static NuadeProbe soundmon_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return sm_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult soundmon_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    SmModule* module = sm_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid SoundMon module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) {
        .voice_count = SOUNDMON_NUADE_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = SM_TICK_HZ
    };
    return result;
}

static void soundmon_close(void* user_data) {
    sm_destroy((SmModule*)user_data);
}

static bool soundmon_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return sm_start((SmModule*)user_data, (int)subsong, voices);
}

static NuadeTickResult soundmon_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    return (NuadeTickResult) { .length = 1,
                               .flags = sm_tick((SmModule*)user_data) ? NuadeTick_Looped : NuadeTick_None };
}

static void soundmon_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const SmModule* module = (const SmModule*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, sm_format_name(module));
    if (sm_title(module)[0] != '\0') {
        sink->set_tag(sink->ctx, NUADE_META_TITLE, sm_title(module));
    }
    for (uint32_t i = 0; i < SOUNDMON_INSTRUMENTS; i++) {
        const char* name = sm_get_instrument_name(module, (int)i);
        if (name[0] != '\0') {
            sink->add_instrument(sink->ctx, i, name);
        }
    }
}

// The track notes count from C-1 = 1; NuadeColumn_Note counts semitones from C-0.
#define SOUNDMON_NOTE_BASE 11

static NuadePositionResult soundmon_position(void* user_data) {
    const SmPosition p = sm_position((const SmModule*)user_data);
    return (NuadePositionResult) {
        .valid = true,
        .position
        = { .mode = NuadeScroll_Rows, .order = (uint32_t)p.step, .row = (uint32_t)p.row, .speed = (uint32_t)p.speed }
    };
}

static uint32_t soundmon_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const SmModule* module = (const SmModule*)user_data;
    const SmPosition p = sm_position(module);
    const uint32_t n = cap < SOUNDMON_NUADE_VOICES ? cap : SOUNDMON_NUADE_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const SmVoiceState v = sm_voice_state(module, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note != 0 ? v.note + SOUNDMON_NOTE_BASE : -1,
                                    .instrument = v.instrument != 0 ? v.instrument - 1 : -1,
                                    .order = (uint32_t)p.step,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult soundmon_tracker_info(void* user_data, const NuadePosition* at) {
    if ((int)at->order >= sm_step_count((const SmModule*)user_data)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info = { .channels = SOUNDMON_NUADE_VOICES,
                                                .rows = SM_ROWS,
                                                .column_count = 4,
                                                .columns = { { "note", NuadeColumn_Note, 3 },
                                                             { "inst", NuadeColumn_Hex, 1 },
                                                             { "fx", NuadeColumn_Hex, 1 },
                                                             { "arg", NuadeColumn_Hex, 2 } } } };
}

static NuadePatternCell soundmon_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const SmCell c = sm_pattern_cell((const SmModule*)ctx, (int)order, (int)channel, (int)row);
    if (c.note != 0) {
        cell.value[0] = c.note + SOUNDMON_NOTE_BASE;
    }
    if (c.instrument != 0) {
        cell.value[1] = c.instrument;
    }
    if (c.effect != 0 || c.effect_arg != 0) {
        cell.value[2] = c.effect;
        cell.value[3] = c.effect_arg;
    }
    return cell;
}

static uint32_t soundmon_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                       int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    return nuade_tracker_fill(at, SOUNDMON_NUADE_VOICES, SM_ROWS, channel, row_lo, row_hi, out, cap, soundmon_cell,
                              user_data);
}

const NuadePlayer nuade_player_soundmon = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "soundmon",
    .name = "BP SoundMon",
    .version = "1",
    .extensions = "bp,bp2,bp3",
    .probe = soundmon_probe,
    .open = soundmon_open,
    .close = soundmon_close,
    .start = soundmon_start,
    .tick = soundmon_tick,
    .metadata = soundmon_metadata,
    .position = soundmon_position,
    .voice_info = soundmon_voice_info,
    .tracker_info = soundmon_tracker_info,
    .tracker_cells = soundmon_tracker_cells,
};

SmModule* sm_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_soundmon) {
        return nullptr;
    }
    return (SmModule*)nuade_player_data(song);
}
