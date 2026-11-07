// SPDX-License-Identifier: MIT
// Silmarils player: InitPlayer (the module, its samples, the main file, the script's first run), InitSound (a
// subsong's start) and the tick. The tune comes from the module's script, which sets the samples up; see sil_script.c.

#include "sil_internal.h"

#include "nuade_file_index.h"
#include "nuade_path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIL_MAIN_SAMPLES 2 // a module with this many sample blocks or fewer takes its samples from a main file
// Script statements and stream bytes one open (or one start) may spend; the open draws on one budget for the module
#define SIL_BUDGET (1u << 25)
#define SIL_SKIP_SUBSONG 2
#define SIL_SKIP_SIGNATURE 0x03B0FF02 // the long before the stream of the subsong whose stream starts further on
#define SIL_SKIP_DISTANCE 0x3B0
#define SIL_STREAM_SIGNATURE_OFFSET 2
#define SIL_ROW_UNITS 25 // wait units of one row of the pattern view: the usual wait of a note

static SilCreateResult create_player(const uint8_t* data, size_t size, const char* path, const NuadeIo* io,
                                     bool check_only);

SilProbe sil_probe(const uint8_t* data, size_t size) {
    const SilCreateResult created = create_player(data, size, nullptr, nullptr, true);
    sil_destroy(created.player);
    if (created.player != nullptr) {
        return SilProbe_Yes;
    }
    return created.status == SilStatus_MissingMain ? SilProbe_NeedsMain : SilProbe_No;
}

// ExtLoad: the main file is the song's file name up to its first '_', in the song's directory.

static bool load_main(const char* path, const NuadeIo* io, uint8_t** data, uint32_t* size) {
    char dir[1024] = { 0 };
    char name[512];
    const char* base = path != nullptr ? nuade_path_basename(path) : nullptr;
    if (base == nullptr || io == nullptr || strlen(base) + 1 > sizeof(name)) {
        return false;
    }
    const char* underscore = strchr(base, '_');
    if (underscore == nullptr || underscore == base) {
        return false;
    }
    memcpy(name, base, (size_t)(underscore - base));
    name[underscore - base] = '\0';
    if (!nuade_path_parent(path, dir, sizeof(dir))) {
        snprintf(dir, sizeof(dir), "."); // a bare file name: the current directory
    }
    const char* dirs[] = { dir };
    NuadeFileIndex* index = nuade_file_index_create(io, dirs, nuade_sizeof_array(dirs));
    const char* url = index != nullptr ? nuade_file_index_find(index, name) : nullptr;
    bool ok = false;
    if (url != nullptr) {
        const NuadeIoReadResult file = NuadeIo_read_url_to_memory(io, url);
        if (file.data != nullptr) {
            if (file.size <= UINT32_MAX / 2) {
                *data = malloc(file.size != 0 ? file.size : 1);
                if (*data != nullptr) {
                    memcpy(*data, file.data, file.size);
                    *size = (uint32_t)file.size;
                    ok = true;
                }
            }
            NuadeIo_free_url_to_memory(io, file.data);
        }
    }
    nuade_file_index_destroy(index);
    return ok;
}

// InitSound for one subsong, from the memory InitPlayer left. False when the script fails or chooses no music.

static bool start_subsong(SilPlayer* p, uint32_t subsong) {
    memcpy(p->memory.data, p->initial, p->memory.size);
    p->script = p->script_initial;
    SilScript* script = &p->script;
    script->music = 0;
    bool ok = true;
    if (p->has_main) {
        ok = sil_script_run_main(script, &p->main) && sil_script_run_song(script, p->flag, (uint16_t)subsong)
             && sil_script_continue(script);
    } else {
        ok = sil_script_run_song(script, p->flag, (uint16_t)subsong);
    }
    if (!ok || script->music == 0) {
        return false;
    }
    uint32_t stream = script->music;
    if (subsong == SIL_SKIP_SUBSONG
        && sil_u32(&p->memory, stream - SIL_STREAM_SIGNATURE_OFFSET) == SIL_SKIP_SIGNATURE) {
        stream += SIL_SKIP_DISTANCE;
    }
    if (!sil_in_memory(&p->memory, stream, 1)) {
        return false;
    }
    if (p->patch.active) {
        sil_put8(&p->memory, p->patch.address, 0);
    }
    sil_music_start(&p->music, &p->memory, script->slots, stream, script->tempo, script->slot_parameters);
    return true;
}

// InitPlayer's first run of the script: the samples of a module that has its own. A script that starts with a sample
// statement (looked for that far from the start of the file, prefix or not), or that names no sample, runs again with
// the flag set.

static bool run_first_script(SilPlayer* p) {
    SilScript* script = &p->script_initial;
    if (!sil_script_run_song(script, 0, 0)) {
        return false;
    }
    if (sil_script_starts_with_sample(script) || sil_script_slots_unset(script)) {
        p->flag = 1;
        if (!sil_script_run_song(script, p->flag, 0)) {
            return false;
        }
    }
    sil_script_note_parameters(script);
    if (p->has_main) {
        p->flag = 1;
    }
    return true;
}

static bool lay_out(SilPlayer* p, const uint8_t* data, uint32_t size, const uint8_t* main, uint32_t main_size) {
    const uint32_t main_start = (size + 1) & ~1u;
    if (!sil_memory_init(&p->memory, main_start + (p->has_main ? main_size : 0))) {
        return false;
    }
    memcpy(p->memory.data, data, size);
    p->song.start = 0;
    p->song.size = size;
    if (p->has_main && main != nullptr) {
        memcpy(p->memory.data + main_start, main, main_size);
        p->main.start = main_start;
        p->main.size = main_size;
    }
    return true;
}

// With `check_only` the main file is not looked for: a module that needs one gets as far as everything that is
// checked without it and then ends with SilStatus_MissingMain and no player.
static SilCreateResult create_player(const uint8_t* data, size_t size, const char* path, const NuadeIo* io,
                                     bool check_only) {
    SilCreateResult result = { .status = SilStatus_InvalidData };
    SilContainer song_container;
    if (size > UINT32_MAX / 2 || !sil_container_parse(data, (uint32_t)size, true, &song_container)) {
        return result;
    }
    SilPlayer* p = calloc(1, sizeof(SilPlayer));
    uint8_t* main_data = nullptr;
    uint32_t main_size = 0;
    if (p == nullptr) {
        result.status = SilStatus_OutOfMemory;
        return result;
    }
    p->has_main = song_container.sample_blocks <= SIL_MAIN_SAMPLES;
    if (p->has_main && !check_only && !load_main(path, io, &main_data, &main_size)) {
        sil_destroy(p);
        result.status = SilStatus_MissingMain;
        return result;
    }
    bool ok = !p->has_main || check_only || sil_container_parse(main_data, main_size, false, &p->main.container);
    ok = ok && lay_out(p, data, (uint32_t)size, main_data, main_size);
    free(main_data);
    if (!ok) {
        sil_destroy(p);
        return result;
    }
    p->song.container = song_container;
    const bool main_loaded = p->has_main && !check_only;
    if ((song_container.sample_blocks > 0 && !sil_image_unpack(&p->memory, &p->song))
        || (main_loaded && p->main.container.sample_blocks > 0 && !sil_image_unpack(&p->memory, &p->main))) {
        sil_destroy(p);
        return result;
    }
    p->patch = sil_patch_find(&p->memory, &p->song);
    p->budget = SIL_BUDGET;
    sil_script_init(&p->script_initial, &p->memory, &p->song, &p->budget);
    ok = run_first_script(p);
    if (ok && check_only && p->has_main) {
        sil_destroy(p);
        result.status = SilStatus_MissingMain;
        return result;
    }
    p->initial = malloc(p->memory.size);
    if (!ok || p->initial == nullptr) {
        sil_destroy(p);
        return result;
    }
    memcpy(p->initial, p->memory.data, p->memory.size);
    // Every subsong must start, and its music must repeat its state within what the port models
    p->rates = calloc(song_container.music_blocks, sizeof(uint16_t));
    for (uint32_t subsong = 0; p->rates != nullptr && subsong < song_container.music_blocks; subsong++) {
        if (!start_subsong(p, subsong) || !sil_music_validate(&p->music, &p->budget)) {
            sil_destroy(p);
            return result;
        }
        p->rates[subsong] = p->music.rate;
    }
    if (p->rates == nullptr) {
        sil_destroy(p);
        result.status = SilStatus_OutOfMemory;
        return result;
    }
    result.player = p;
    result.status = SilStatus_Ok;
    return result;
}

SilCreateResult sil_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    return create_player(data, size, path, io, false);
}

void sil_destroy(SilPlayer* player) {
    if (player == nullptr) {
        return;
    }
    sil_memory_free(&player->memory);
    free(player->initial);
    free(player->rates);
    free(player);
}

uint32_t sil_subsong_count(const SilPlayer* player) {
    return player->song.container.music_blocks;
}

uint32_t sil_grid_ticks(const SilPlayer* player, uint32_t subsong) {
    if (subsong >= player->song.container.music_blocks || player->rates[subsong] == 0) {
        return 0;
    }
    // A wait is due when the counter, which starts at units * 256, is at most one rate step: one tick less
    const uint32_t rate = player->rates[subsong];
    const uint32_t ticks = (SIL_ROW_UNITS * 256 + rate - 1) / rate - 1;
    return ticks != 0 ? ticks : 1;
}

bool sil_start(SilPlayer* player, uint32_t subsong, NuadeVoices* voices) {
    player->budget = SIL_BUDGET;
    if (subsong >= player->song.container.music_blocks || !start_subsong(player, subsong)) {
        return false;
    }
    for (uint32_t i = 0; i < SIL_VOICES; i++) {
        nuade_voice_stop(voices, i);
    }
    return true;
}

SilTickResult sil_tick(SilPlayer* player, NuadeVoices* voices) {
    SilTickResult result = { 0 };
    if (!sil_music_tick(&player->music, voices, &result.looped)) {
        result.ended = true;
        return result;
    }
    sil_music_sync(&player->music, voices);
    return result;
}

SilVoiceState sil_voice_state(const SilPlayer* player, int voice) {
    if (voice < 0 || voice >= SIL_USED_VOICES) {
        return (SilVoiceState) { .note = -1, .instrument = -1 };
    }
    const SilChannel* c = &player->music.channels[voice];
    return (SilVoiceState) { .note = c->note, .instrument = c->instrument };
}
