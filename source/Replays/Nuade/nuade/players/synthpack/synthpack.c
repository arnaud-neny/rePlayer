// SPDX-License-Identifier: MIT
// Synth Pack player: loading the song and its sample set, and the tick.

#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_utils.h"
#include "sp_longs.h"
#include "sp_words.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SP_SONG_PREFIX "osp."
#define SP_SET_PREFIX "smp."
#define SP_PREFIX_LENGTH 4
#define SP_DEFAULT_SET "smp.set"

static const SpRules* sp_layout_rules(SpLayout layout) {
    return layout == SpLayout_Longs ? &sp_long_rules : &sp_word_rules;
}

bool sp_probe(const uint8_t* data, size_t size) {
    SpLayout layout;
    if (!sp_song_layout(data, size, &layout)) {
        return false;
    }
    SpMemory song;
    if (!sp_memory_init(&song, data, size)) {
        return false;
    }
    const bool ok = sp_validate(sp_layout_rules(layout), &song, nullptr, sp_count_subsongs(&song));
    sp_memory_free(&song);
    return ok;
}

// The sample set is "smp.<name>" for a song "osp.<name>". Any other song name makes the eagleplayer try the name
// without its last character; and a set that is not found there is "smp.set" in the song's directory.

static bool sp_read_set_file(NuadeFileIndex* index, const NuadeIo* io, const char* name, SpMemory* set) {
    const char* url = nuade_file_index_find(index, name);
    if (url == nullptr) {
        return false;
    }
    const NuadeIoReadResult file = NuadeIo_read_url_to_memory(io, url);
    if (file.data == nullptr) {
        return false;
    }
    const bool ok = sp_memory_init(set, file.data, (size_t)file.size);
    NuadeIo_free_url_to_memory(io, file.data);
    return ok;
}

static bool sp_load_set(const char* path, const NuadeIo* io, SpMemory* set) {
    if (path == nullptr || io == nullptr) {
        return false;
    }
    char dir[1024];
    char name[256];
    if (!nuade_path_parent(path, dir, sizeof(dir))) {
        snprintf(dir, sizeof(dir), "."); // a bare file name: the current directory
    }
    const char* base = nuade_path_basename(path);
    const size_t length = strlen(base);
    if (length + sizeof(SP_SET_PREFIX) >= sizeof(name)) {
        return false;
    }
    if (nuade_strncasecmp(base, SP_SONG_PREFIX, SP_PREFIX_LENGTH) == 0) {
        snprintf(name, sizeof(name), SP_SET_PREFIX "%s", base + SP_PREFIX_LENGTH);
    } else {
        snprintf(name, sizeof(name), "%.*s", (int)(length - 1), base);
    }
    const char* dirs[] = { dir };
    NuadeFileIndex* index = nuade_file_index_create(io, dirs, nuade_sizeof_array(dirs));
    if (index == nullptr) {
        return false;
    }
    const bool found = (name[0] != '\0' && sp_read_set_file(index, io, name, set))
                       || sp_read_set_file(index, io, SP_DEFAULT_SET, set);
    nuade_file_index_destroy(index);
    return found;
}

SpCreateResult sp_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    SpCreateResult result = { 0 };
    SpLayout layout;
    if (!sp_song_layout(data, size, &layout)) {
        result.status = SpStatus_InvalidData;
        return result;
    }
    SpPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        result.status = SpStatus_OutOfMemory;
        return result;
    }
    player->layout = layout;
    if (!sp_memory_init(&player->song, data, size)) {
        sp_destroy(player);
        result.status = SpStatus_OutOfMemory;
        return result;
    }
    if (!sp_load_set(path, io, &player->set)) {
        sp_destroy(player);
        result.status = SpStatus_MissingSamples;
        return result;
    }
    player->subsong_count = sp_count_subsongs(&player->song);
    const bool valid = (layout == SpLayout_Words || sp_set_has_magic(&player->set))
                       && sp_validate(sp_layout_rules(layout), &player->song, &player->set, player->subsong_count);
    if (!valid) {
        sp_destroy(player);
        result.status = SpStatus_InvalidData;
        return result;
    }
    result.player = player;
    return result;
}

void sp_destroy(SpPlayer* player) {
    if (player == nullptr) {
        return;
    }
    sp_memory_free(&player->song);
    sp_memory_free(&player->set);
    free(player);
}

int sp_subsong_count(const SpPlayer* player) {
    return player->subsong_count;
}

bool sp_start(SpPlayer* player, int subsong, NuadeVoices* voices) {
    if (subsong < 0 || subsong >= player->subsong_count) {
        return false;
    }
    player->stopped = false;
    if (player->layout == SpLayout_Longs) {
        sp_long_start(&player->longs, &player->song, &player->set, &player->hardware, subsong);
    } else {
        sp_word_start(&player->words, &player->song, &player->set, &player->hardware, subsong);
    }
    sp_hardware_sync(&player->hardware, voices);
    return true;
}

SpTickResult sp_tick(SpPlayer* player, NuadeVoices* voices) {
    SpSongEvents events;
    bool silent;
    if (player->layout == SpLayout_Longs) {
        sp_long_tick(&player->longs, &player->hardware);
        events = player->longs.events;
        silent = sp_long_silent(&player->longs);
    } else {
        sp_word_tick(&player->words, &player->hardware);
        events = player->words.events;
        silent = sp_word_silent(&player->words);
    }
    sp_hardware_sync(&player->hardware, voices);
    player->stopped = player->stopped || events.stopped;
    return (SpTickResult) { .looped = events.song_end && !events.stopped, .ended = player->stopped && silent };
}

uint32_t sp_grid_ticks(const SpPlayer* player) {
    (void)player;
    return SP_DEFAULT_SPEED;
}

SpVoiceState sp_voice_state(const SpPlayer* player, int voice) {
    if (voice < 0 || voice >= SP_VOICES) {
        return (SpVoiceState) { .note = -1, .instrument = -1 };
    }
    const SpVoice* v = player->layout == SpLayout_Longs ? sp_long_channel_voice(&player->longs, (unsigned)voice)
                                                        : sp_word_channel_voice(&player->words, (unsigned)voice);
    return (SpVoiceState) { .note = v->note, .instrument = v->note >= 0 ? v->instrument : -1 };
}
