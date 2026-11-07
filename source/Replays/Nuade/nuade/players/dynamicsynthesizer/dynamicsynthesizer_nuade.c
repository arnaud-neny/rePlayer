// SPDX-License-Identifier: MIT
// Dynamic Synthesizer modules as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/dynamicsynthesizer.h>
#include <stdio.h>

#define MODULE_PREFIX "dns"

static NuadeProbe dynamicsynthesizer_probe(const uint8_t* data, size_t size, const char* filename) {
    return dns_probe(data, size, filename) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult dynamicsynthesizer_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const DnsCreateResult created = dns_create(source->data, source->size, source->path, source->io);
    if (created.player == nullptr) {
        result.error.status
            = created.status == DnsStatus_OutOfMemory ? NuadeStatus_OutOfMemory : NuadeStatus_InvalidData;
        if (created.status == DnsStatus_MissingSamples) {
            result.error.status = NuadeStatus_IoError;
            snprintf(result.error.message, sizeof(result.error.message), "the \"smp.\" sample file is missing");
        } else {
            snprintf(result.error.message, sizeof(result.error.message), "not a known Dynamic Synthesizer module");
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) { .voice_count = DNS_VOICES,
                                    .subsong_count = (uint32_t)dns_subsong_count(created.player),
                                    .default_subsong = 0,
                                    .timebase_hz = DNS_TIMEBASE_HZ };
    return result;
}

static void dynamicsynthesizer_close(void* user_data) {
    dns_destroy((DnsPlayer*)user_data);
}

static bool dynamicsynthesizer_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return dns_start((DnsPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult dynamicsynthesizer_tick(void* user_data, NuadeVoices* voices) {
    const DnsTickResult tick = dns_tick((DnsPlayer*)user_data, voices);
    return (NuadeTickResult) { .length = tick.length, .flags = tick.looped ? NuadeTick_Looped : NuadeTick_None };
}

static void dynamicsynthesizer_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Dynamic Synthesizer");
}

static uint32_t dynamicsynthesizer_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const DnsPlayer* player = (const DnsPlayer*)user_data;
    const uint32_t count = cap < DNS_VOICES ? cap : DNS_VOICES;
    for (uint32_t i = 0; i < count; i++) {
        const DnsVoiceState v = dns_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return count;
}

static uint32_t dynamicsynthesizer_grid_ticks(void* user_data, uint32_t subsong) {
    return dns_grid_ticks((const DnsPlayer*)user_data, (int)subsong);
}

const NuadePlayer nuade_player_dynamicsynthesizer = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "dynamicsynthesizer",
    .name = "Dynamic Synthesizer",
    .version = "1",
    .extensions = MODULE_PREFIX,
    .probe = dynamicsynthesizer_probe,
    .open = dynamicsynthesizer_open,
    .close = dynamicsynthesizer_close,
    .start = dynamicsynthesizer_start,
    .tick = dynamicsynthesizer_tick,
    .metadata = dynamicsynthesizer_metadata,
    .voice_info = dynamicsynthesizer_voice_info,
    .grid_ticks = dynamicsynthesizer_grid_ticks,
};

DnsPlayer* dns_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_dynamicsynthesizer) {
        return nullptr;
    }
    return (DnsPlayer*)nuade_player_data(song);
}
