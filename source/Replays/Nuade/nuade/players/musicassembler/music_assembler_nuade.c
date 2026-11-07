// SPDX-License-Identifier: MIT
// Music Assembler as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include <nuade/players/musicassembler.h>
#include <stdio.h>

#define MUSICASSEMBLER_VOICES 4

// The file is 68k code: the probe matches the init and play function layout, which is specific enough to be sure.
static NuadeProbe musicassembler_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return ma_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult musicassembler_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    MaModule* module = ma_create(source->data, source->size);
    if (module == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a valid Music Assembler module");
        return result;
    }
    result.user_data = module;
    result.info = (NuadeSongInfo) { .voice_count = MUSICASSEMBLER_VOICES,
                                    .subsong_count = (uint32_t)ma_subsong_count(module),
                                    .default_subsong = 0,
                                    .timebase_hz = MA_TICK_HZ };
    return result;
}

static void musicassembler_close(void* user_data) {
    ma_destroy((MaModule*)user_data);
}

static bool musicassembler_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return ma_start((MaModule*)user_data, (int)subsong, voices);
}

static NuadeTickResult musicassembler_tick(void* user_data, NuadeVoices* voices) {
    (void)voices;
    const MaTickResult tick = ma_tick((MaModule*)user_data);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = 1, .flags = flags };
}

static uint32_t musicassembler_grid_ticks(void* user_data, uint32_t subsong) {
    return (uint32_t)ma_start_speed((const MaModule*)user_data, (int)subsong);
}

static uint32_t musicassembler_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const MaModule* module = (const MaModule*)user_data;
    const uint32_t n = cap < MUSICASSEMBLER_VOICES ? cap : MUSICASSEMBLER_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        int note = -1;
        int instrument = -1;
        if (!ma_voice_started(module, (int)i, &note, &instrument)) {
            note = -1;
            instrument = -1;
        }
        out[i] = (NuadeVoiceInfo) { .note = note, .instrument = instrument };
    }
    return n;
}

static void musicassembler_metadata(void* user_data, const NuadeMetadataSink* sink) {
    const MaModule* module = (const MaModule*)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Music Assembler");
    for (int i = 0; i < ma_sample_count(module); i++) {
        const char* name = ma_sample_name(module, i);
        if (name[0] != '\0') {
            sink->add_instrument(sink->ctx, (uint32_t)i, name);
        }
    }
}

const NuadePlayer nuade_player_musicassembler = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "musicassembler",
    .name = "Music Assembler",
    .version = "1",
    .extensions = "ma",
    .probe = musicassembler_probe,
    .open = musicassembler_open,
    .close = musicassembler_close,
    .start = musicassembler_start,
    .tick = musicassembler_tick,
    .metadata = musicassembler_metadata,
    .voice_info = musicassembler_voice_info,
    .grid_ticks = musicassembler_grid_ticks,
};

MaModule* ma_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_musicassembler) {
        return nullptr;
    }
    return (MaModule*)nuade_player_data(song);
}
