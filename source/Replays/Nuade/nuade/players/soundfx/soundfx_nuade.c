// SPDX-License-Identifier: MIT
// SoundFX 2.0 as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/soundfx.h>
#include <stdio.h>

// The module mark is specific enough to be sure.
static NuadeProbe soundfx_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return sfx_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult soundfx_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    SfxModule* module = sfx_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid SoundFX module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) {
        .voice_count = SFX_CHANNELS, .subsong_count = 1, .default_subsong = 0, .timebase_hz = SFX_TICK_HZ
    };
    return result;
}

static void soundfx_close(void* user_data) {
    sfx_destroy((SfxModule*)user_data);
}

static bool soundfx_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return sfx_start((SfxModule*)user_data, (int)subsong, voices);
}

static NuadeTickResult soundfx_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    SfxModule* module = (SfxModule*)user_data;
    const bool looped = sfx_tick(module);
    return (NuadeTickResult) { .length = sfx_tick_length(module), .flags = looped ? NuadeTick_Looped : NuadeTick_None };
}

static void soundfx_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const SfxModule* module = (const SfxModule*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, sfx_format_name(module));
    for (uint32_t i = 0; i < SFX_INSTRUMENTS; i++) {
        const char* name = sfx_instrument_name(module, (int)i);
        if (name[0] != '\0') {
            sink->add_instrument(sink->ctx, i, name);
        }
    }
}

// Commands in the period field show as effect F with the low byte of the command as its argument.
#define SOUNDFX_NUADE_MARKER_EFFECT 0xf

static NuadePositionResult soundfx_position(void* user_data) {
    const SfxPosition p = sfx_position((const SfxModule*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position
                                   = { .mode = NuadeScroll_Rows,
                                       .order = p.position,
                                       .row = p.row,
                                       .speed = p.speed,
                                       .rows = sfx_pattern_rows((const SfxModule*)user_data, (int)p.position) } };
}

static uint32_t soundfx_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const SfxModule* module = (const SfxModule*)user_data;
    const SfxPosition p = sfx_position(module);
    const uint32_t n = cap < SFX_CHANNELS ? cap : SFX_CHANNELS;
    for (uint32_t i = 0; i < n; i++) {
        const SfxVoiceState v = sfx_voice_state(module, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note(v.period),
                                    .instrument = v.sample != 0 ? v.sample - 1 : -1,
                                    .order = p.position,
                                    .row = p.row };
    }
    return n;
}

static NuadeTrackerInfoResult soundfx_tracker_info(void* user_data, const NuadePosition* at) {
    const SfxModule* module = (const SfxModule*)user_data;
    if (at->order >= sfx_position_count(module)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) { .valid = true,
                                      .info
                                      = { .channels = SFX_CHANNELS,
                                          .rows = at->rows != 0 ? at->rows : sfx_pattern_rows(module, (int)at->order),
                                          .column_count = 4,
                                          .columns = { { "note", NuadeColumn_Note, 3 },
                                                       { "inst", NuadeColumn_Hex, 2 },
                                                       { "fx", NuadeColumn_Hex, 1 },
                                                       { "arg", NuadeColumn_Hex, 2 } } } };
}

static NuadePatternCell soundfx_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const SfxCell c = sfx_pattern_cell((const SfxModule*)ctx, (int)order, (int)channel, (int)row);
    if (c.special == SFX_SPECIAL_STOP) {
        cell.value[0] = NUADE_NOTE_CUT;
    } else if (c.special == 0) {
        cell.value[0] = nuade_period_to_note(c.period);
    }
    if (c.sample != 0 && c.special != SFX_SPECIAL_PIC) {
        cell.value[1] = c.sample;
    }
    if (c.special != 0 && c.special != SFX_SPECIAL_STOP) {
        cell.value[2] = SOUNDFX_NUADE_MARKER_EFFECT;
        cell.value[3] = c.special & 0xff;
    } else if (c.effect != 0 || c.effect_arg != 0) {
        cell.value[2] = c.effect;
        cell.value[3] = c.effect_arg;
    }
    return cell;
}

static uint32_t soundfx_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                      int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const SfxModule* module = (const SfxModule*)user_data;
    if (at->order >= sfx_position_count(module)) {
        return 0;
    }
    const uint32_t rows = at->rows != 0 ? at->rows : sfx_pattern_rows(module, (int)at->order);
    return nuade_tracker_fill(at, SFX_CHANNELS, rows, channel, row_lo, row_hi, out, cap, soundfx_cell, user_data);
}

const NuadePlayer nuade_player_soundfx = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "soundfx",
    .name = "SoundFX",
    .version = "1",
    .extensions = "sfx",
    .probe = soundfx_probe,
    .open = soundfx_open,
    .close = soundfx_close,
    .start = soundfx_start,
    .tick = soundfx_tick,
    .metadata = soundfx_metadata,
    .position = soundfx_position,
    .voice_info = soundfx_voice_info,
    .tracker_info = soundfx_tracker_info,
    .tracker_cells = soundfx_tracker_cells,
};

SfxModule* sfx_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_soundfx) {
        return nullptr;
    }
    return (SfxModule*)nuade_player_data(song);
}
