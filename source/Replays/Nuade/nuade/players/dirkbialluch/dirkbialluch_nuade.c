// SPDX-License-Identifier: MIT
// Dirk Bialluch as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/dirkbialluch.h>
#include <stdio.h>

static NuadeProbe dirkbialluch_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return db_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult dirkbialluch_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    const DbCreateResult created = db_create(source->data, source->size, source->path, source->io);
    if (created.player == nullptr) {
        switch (created.status) {
            case DbStatus_MissingSamples:
                result.error.status = NuadeStatus_IoError;
                snprintf(result.error.message, sizeof(result.error.message),
                         "the \"smp.\" sample file next to the \"tpu.\" song is missing");
                break;
            case DbStatus_OutOfMemory:
                result.error.status = NuadeStatus_OutOfMemory;
                snprintf(result.error.message, sizeof(result.error.message), "out of memory");
                break;
            default:
                result.error.status = NuadeStatus_InvalidData;
                snprintf(result.error.message, sizeof(result.error.message), "not a Dirk Bialluch song");
                break;
        }
        return result;
    }
    result.user_data = created.player;
    result.info = (NuadeSongInfo) { .voice_count = DB_VOICES,
                                    .subsong_count = (uint32_t)db_subsong_count(created.player),
                                    .default_subsong = 0,
                                    .timebase_hz = DB_TIMEBASE_HZ };
    return result;
}

static void dirkbialluch_close(void* user_data) {
    db_destroy((DbPlayer*)user_data);
}

static bool dirkbialluch_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return db_start((DbPlayer*)user_data, (int)subsong, voices);
}

static NuadeTickResult dirkbialluch_tick(void* user_data, NuadeVoices* voices) {
    const DbTickResult tick = db_tick((DbPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = DB_TICK_LENGTH, .flags = flags };
}

static void dirkbialluch_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Dirk Bialluch");
}

static uint32_t dirkbialluch_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const DbPlayer* player = (const DbPlayer*)user_data;
    const uint32_t n = cap < DB_VOICES ? cap : DB_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const DbVoiceState v = db_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note, .instrument = v.instrument };
    }
    return n;
}

static uint32_t dirkbialluch_grid_ticks(void* user_data, uint32_t subsong) {
    (void)subsong;
    return db_grid_ticks((const DbPlayer*)user_data);
}

const NuadePlayer nuade_player_dirkbialluch = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "dirkbialluch",
    .name = "Dirk Bialluch",
    .version = "1",
    .extensions = "tpu",
    .probe = dirkbialluch_probe,
    .open = dirkbialluch_open,
    .close = dirkbialluch_close,
    .start = dirkbialluch_start,
    .tick = dirkbialluch_tick,
    .metadata = dirkbialluch_metadata,
    .voice_info = dirkbialluch_voice_info,
    .grid_ticks = dirkbialluch_grid_ticks,
};

DbPlayer* db_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_dirkbialluch) {
        return nullptr;
    }
    return (DbPlayer*)nuade_player_data(song);
}
