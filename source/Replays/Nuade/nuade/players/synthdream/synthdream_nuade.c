// SPDX-License-Identifier: MIT
// Synth Dream as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/synthdream.h>
#include <stdio.h>

#define GRID_TICKS 6

static NuadeProbe synthdream_probe(const uint8_t* data, size_t size, const char* filename) {
    if (!sdr_name_matches(filename)) {
        return NuadeProbe_Unsupported;
    }
    return sdr_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult synthdream_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const SdrCreateResult created = sdr_create(source->data, source->size, source->path, source->io);
    if (created.player == nullptr) {
        const char* message = "not a Synth Dream module";
        switch (created.status) {
            case SdrStatus_OutOfMemory:
                result.error.status = NuadeStatus_OutOfMemory;
                break;
            case SdrStatus_MissingSamples:
                result.error.status = NuadeStatus_IoError;
                message = "the \"smp.\" sample file is missing";
                break;
            case SdrStatus_Unmodelled:
                result.error.status = NuadeStatus_Unsupported;
                message = "the song reads memory this player does not model";
                break;
            case SdrStatus_Ok:
            case SdrStatus_InvalidData:
                result.error.status = NuadeStatus_InvalidData;
                break;
        }
        snprintf(result.error.message, sizeof(result.error.message), "%s", message);
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) { .voice_count = SDR_VOICES,
                                    .subsong_count = (uint32_t)sdr_subsong_count(created.player),
                                    .default_subsong = 0,
                                    .timebase_hz = SDR_TIMEBASE_HZ };
    return result;
}

static void synthdream_close(void* user_data) {
    sdr_destroy((SdrPlayer*)user_data);
}

static bool synthdream_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    (void)voices;
    return sdr_start((SdrPlayer*)user_data, (int)subsong);
}

static NuadeTickResult synthdream_tick(void* user_data, NuadeVoices* voices) {
    const SdrTickResult tick = sdr_tick((SdrPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = SDR_TICK_LENGTH, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void synthdream_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Synth Dream");
}

static uint32_t synthdream_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const SdrPlayer* player = (const SdrPlayer*)user_data;
    const uint32_t count = cap < SDR_VOICES ? cap : SDR_VOICES;
    for (uint32_t i = 0; i < count; i++) {
        const SdrVoiceState v = sdr_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return count;
}

static uint32_t synthdream_grid_ticks(void* user_data, uint32_t subsong) {
    (void)user_data;
    (void)subsong;
    return GRID_TICKS;
}

const NuadePlayer nuade_player_synthdream = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "synthdream",
    .name = "Synth Dream",
    .version = "1",
    .extensions = "sdr",
    .probe = synthdream_probe,
    .open = synthdream_open,
    .close = synthdream_close,
    .start = synthdream_start,
    .tick = synthdream_tick,
    .metadata = synthdream_metadata,
    .voice_info = synthdream_voice_info,
    .grid_ticks = synthdream_grid_ticks,
};

SdrPlayer* sdr_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_synthdream) {
        return nullptr;
    }
    return (SdrPlayer*)nuade_player_data(song);
}
