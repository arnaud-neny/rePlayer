// SPDX-License-Identifier: MIT
// Silmarils as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/silmarils.h>
#include <stdio.h>

static NuadeProbe silmarils_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    switch (sil_probe(data, size)) {
        case SilProbe_Yes:
            return NuadeProbe_Supported;
        case SilProbe_NeedsMain:
            return NuadeProbe_Unsure;
        case SilProbe_No:
            break;
    }
    return NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult silmarils_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const SilCreateResult created = sil_create(source->data, source->size, source->path, source->io);
    if (created.player == nullptr) {
        switch (created.status) {
            case SilStatus_OutOfMemory:
                result.error.status = NuadeStatus_OutOfMemory;
                snprintf(result.error.message, sizeof(result.error.message), "out of memory");
                break;
            case SilStatus_MissingMain:
                result.error.status = NuadeStatus_IoError;
                snprintf(result.error.message, sizeof(result.error.message), "the main file of the module is missing");
                break;
            case SilStatus_InvalidData:
            case SilStatus_Ok:
                result.error.status = NuadeStatus_InvalidData;
                snprintf(result.error.message, sizeof(result.error.message), "not a Silmarils module the port models");
                break;
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) { .voice_count = SIL_VOICES,
                                    .subsong_count = sil_subsong_count(created.player),
                                    .default_subsong = 0,
                                    .timebase_hz = SIL_TIMEBASE_HZ };
    return result;
}

static void silmarils_close(void* user_data) {
    sil_destroy((SilPlayer*)user_data);
}

static bool silmarils_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return sil_start((SilPlayer*)user_data, subsong, voices);
}

static NuadeTickResult silmarils_tick(void* user_data, NuadeVoices* voices) {
    const SilTickResult tick = sil_tick((SilPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = SIL_TICK_LENGTH,
                               .flags = (tick.looped ? NuadeTick_Looped : NuadeTick_None)
                                        | (tick.ended ? NuadeTick_Ended : NuadeTick_None) };
}

static void silmarils_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Silmarils");
}

static uint32_t silmarils_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const SilPlayer* player = (const SilPlayer*)user_data;
    const uint32_t n = cap < SIL_VOICES ? cap : SIL_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const SilVoiceState v = sil_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static uint32_t silmarils_grid_ticks(void* user_data, uint32_t subsong) {
    return sil_grid_ticks((const SilPlayer*)user_data, subsong);
}

const NuadePlayer nuade_player_silmarils = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "silmarils",
    .name = "Silmarils",
    .version = "1",
    .extensions = "mok",
    .probe = silmarils_probe,
    .open = silmarils_open,
    .close = silmarils_close,
    .start = silmarils_start,
    .tick = silmarils_tick,
    .metadata = silmarils_metadata,
    .voice_info = silmarils_voice_info,
    .grid_ticks = silmarils_grid_ticks,
};

SilPlayer* sil_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_silmarils) {
        return nullptr;
    }
    return (SilPlayer*)nuade_player_data(song);
}
