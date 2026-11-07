// SPDX-License-Identifier: MIT
// Maximum Effect as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/maximumeffect.h>
#include <stdio.h>

static NuadeProbe maximumeffect_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return mxe_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult maximumeffect_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const MxeCreateResult created = mxe_create(source->data, source->size, source->path, source->io);
    if (created.player == nullptr) {
        const char* message = "not a Maximum Effect song";
        result.error.status = NuadeStatus_InvalidData;
        if (created.status == MxeStatus_OutOfMemory) {
            result.error.status = NuadeStatus_OutOfMemory;
            message = "out of memory";
        } else if (created.status == MxeStatus_MissingSamples) {
            result.error.status = NuadeStatus_IoError;
            message = "the \"SMP.\" sample file is missing";
        }
        snprintf(result.error.message, sizeof(result.error.message), "%s", message);
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) { .voice_count = MXE_VOICES,
                                    .subsong_count = mxe_subsong_count(created.player),
                                    .default_subsong = 0,
                                    .timebase_hz = MXE_TIMEBASE_HZ };
    return result;
}

static void maximumeffect_close(void* user_data) {
    mxe_destroy((MxePlayer*)user_data);
}

static bool maximumeffect_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return mxe_start((MxePlayer*)user_data, subsong, voices);
}

static NuadeTickResult maximumeffect_tick(void* user_data, NuadeVoices* voices) {
    const MxeTickResult tick = mxe_tick((MxePlayer*)user_data, voices);
    const uint32_t flags = tick.looped ? NuadeTick_Looped : NuadeTick_None;
    return (NuadeTickResult) { .length = MXE_TICK_LENGTH, .flags = flags };
}

static void maximumeffect_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Maximum Effect");
}

static uint32_t maximumeffect_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const MxePlayer* player = (const MxePlayer*)user_data;
    const uint32_t n = cap < MXE_VOICES ? cap : MXE_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const MxeVoiceState v = mxe_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = nuade_period_to_note((uint32_t)v.period), .instrument = v.instrument };
    }
    return n;
}

static uint32_t maximumeffect_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return mxe_grid_ticks((const MxePlayer*)user_data);
}

const NuadePlayer nuade_player_maximumeffect = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "maximumeffect",
    .name = "Maximum Effect",
    .version = "1",
    .extensions = "max",
    .probe = maximumeffect_probe,
    .open = maximumeffect_open,
    .close = maximumeffect_close,
    .start = maximumeffect_start,
    .tick = maximumeffect_tick,
    .metadata = maximumeffect_metadata,
    .voice_info = maximumeffect_voice_info,
    .grid_ticks = maximumeffect_grid_ticks,
};

MxePlayer* mxe_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_maximumeffect) {
        return nullptr;
    }
    return (MxePlayer*)nuade_player_data(song);
}
