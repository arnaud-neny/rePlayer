// SPDX-License-Identifier: MIT
// Jochen Hippel COSO as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_path.h"
#include "nuade_utils.h"
#include <nuade/players/hippel.h>
#include <stdio.h>
#include <string.h>

// Wanted Team's player takes the modules named "soc.name" or "name.soc"
#define WANTED_TEAM_EXTENSION "soc"

// Period table index 12 (856) is C-1.
#define HIP_NOTE_BASE 0

// UADE picks the player by the file name: the part before the first dot, else the part after the last one.
static bool is_wanted_team_name(const char* path) {
    if (path == nullptr) {
        return false;
    }
    const char* base = nuade_path_basename(path);
    const char* first_dot = strchr(base, '.');
    const size_t length = strlen(WANTED_TEAM_EXTENSION);
    if (first_dot != nullptr && (size_t)(first_dot - base) == length
        && nuade_strncasecmp(base, WANTED_TEAM_EXTENSION, length) == 0) {
        return true;
    }
    return !nuade_path_has_ext(path, "hipc") && nuade_path_has_ext(path, WANTED_TEAM_EXTENSION);
}

// As strict as open: a Wanted Team module the port refuses is left to the UADE player.
static NuadeProbe hippel_probe(const uint8_t* data, size_t size, const char* filename) {
    if (!is_wanted_team_name(filename)) {
        return hip_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
    }
    HipPlayer* player = hip_create_wanted_team(data, size);
    const bool accepted = player != nullptr;
    hip_destroy(player);
    return accepted ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult hippel_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    HipPlayer* player = is_wanted_team_name(source->path) ? hip_create_wanted_team(source->data, source->size)
                                                          : hip_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Jochen Hippel COSO module");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) { .voice_count = HIP_VOICES,
                                    .subsong_count = (uint32_t)hip_subsong_count(player),
                                    .default_subsong = 0,
                                    .timebase_hz = HIP_TIMEBASE_HZ };
    return result;
}

static void hippel_close(void* user_data) {
    hip_destroy((HipPlayer*)user_data);
}

static bool hippel_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return hip_start((HipPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult hippel_tick(void* user_data, NuadeVoices* voices) {
    const HipTickResult tick = hip_tick((HipPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = HIP_TICK_LENGTH, .flags = flags };
}

static void hippel_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Jochen Hippel COSO");
}

static uint32_t hippel_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const HipPlayer* player = (const HipPlayer*)user_data;
    const uint32_t n = cap < HIP_VOICES ? cap : HIP_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const HipVoiceState v = hip_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note >= 0 ? v.note + HIP_NOTE_BASE : -1, .instrument = v.instrument };
    }
    return n;
}

// The voices run their own patterns and speeds: one grid row is one sequencer step.
static uint32_t hippel_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return (uint32_t)hip_speed((const HipPlayer*)user_data);
}

const NuadePlayer nuade_player_hippel = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "hippel",
    .name = "Jochen Hippel COSO",
    .version = "1",
    .extensions = "hipc,soc",
    .probe = hippel_probe,
    .open = hippel_open,
    .close = hippel_close,
    .start = hippel_start,
    .tick = hippel_tick,
    .metadata = hippel_metadata,
    .voice_info = hippel_voice_info,
    .grid_ticks = hippel_grid_ticks,
};

HipPlayer* hip_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_hippel) {
        return nullptr;
    }
    return (HipPlayer*)nuade_player_data(song);
}
