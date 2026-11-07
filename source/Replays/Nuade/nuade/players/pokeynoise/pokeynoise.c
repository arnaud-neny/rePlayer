// SPDX-License-Identifier: MIT
// Pokeynoise player: the eagleplayer's InitSound and Interrupt. A subsong starts by running the song's init routine (at
// the address in RUNAD, $02E0) with the subsong number in A; the routine registers its vertical blank routine with
// SETVBV. Every tick increments the 24 bit jiffy clock at $12..$14, runs that routine, and maps the POKEY registers it
// left to the four Paula channels: a channel restarts its waveform when the waveform changes, otherwise only the
// period and volume are written.

#include <nuade/players/pokeynoise.h>

#include "nuade_base.h"
#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include "pn_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// DeliTracker's 50 Hz CIA timer, which the eagleplayer keeps.
#define PN_TICK_LENGTH 14187u

// The 6502 addresses the player uses: the run address that holds the init routine and the jiffy clock.
#define PN_RUNAD 0x2E0u
#define PN_RTCLOK 0x12u

// Atari binaries start with 0xFFFF; a segment header is four bytes.
#define PN_MIN_SIZE 6u

#define PN_PREFIX "pn."
#define PN_INFO_SUFFIX ".info"
#define PN_INFO_MAX_SIZE (1u << 20)

bool pn_probe(const uint8_t* data, size_t size, const char* filename) {
    return size >= PN_MIN_SIZE && data[0] == 0xFF && data[1] == 0xFF && filename != nullptr
           && nuade_strncasecmp(nuade_path_basename(filename), PN_PREFIX, sizeof(PN_PREFIX) - 1) == 0;
}

// The ".info" file is the song's file name with ".info" appended, in the song's directory.

static bool load_info(PnPlayer* player, const char* path, const NuadeIo* io) {
    char dir[1024] = { 0 };
    char name[512];
    const char* base = nuade_path_basename(path);
    if (strlen(base) + sizeof(PN_INFO_SUFFIX) > sizeof(name)) {
        return false;
    }
    if (!nuade_path_parent(path, dir, sizeof(dir))) {
        snprintf(dir, sizeof(dir), "."); // a bare file name: the current directory
    }
    snprintf(name, sizeof(name), "%s" PN_INFO_SUFFIX, base);
    const char* dirs[] = { dir };
    NuadeFileIndex* index = nuade_file_index_create(io, dirs, nuade_sizeof_array(dirs));
    const char* url = index != nullptr ? nuade_file_index_find(index, name) : nullptr;
    bool ok = false;
    if (url != nullptr) {
        const NuadeIoReadResult file = NuadeIo_read_url_to_memory(io, url);
        if (file.data != nullptr && file.size <= PN_INFO_MAX_SIZE) {
            player->info = malloc(file.size ? (size_t)file.size : 1);
            if (player->info != nullptr) {
                memcpy(player->info, file.data, (size_t)file.size);
                player->info_size = (size_t)file.size;
                ok = true;
            }
        }
        if (file.data != nullptr) {
            NuadeIo_free_url_to_memory(io, file.data);
        }
    }
    nuade_file_index_destroy(index);
    return ok;
}

PnPlayer* pn_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    if (!pn_probe(data, size, path) || io == nullptr) {
        return nullptr;
    }
    PnPlayer* player = calloc(1, sizeof(PnPlayer));
    if (player == nullptr) {
        return nullptr;
    }
    player->song = malloc(size);
    player->waves = nuade_paula_padded_copy(pn_wave_data, PN_WAVE_BYTES);
    if (player->song == nullptr || player->waves == nullptr || !load_info(player, path, io)) {
        pn_destroy(player);
        return nullptr;
    }
    memcpy(player->song, data, size);
    player->song_size = size;
    const PnInfo info = { .data = player->info, .size = player->info_size };
    const PnToolType type = pn_info_find(&info, "TYPE");
    if (!pn_tool_type_is(&type, "POKEY")) {
        pn_destroy(player);
        return nullptr;
    }
    const PnToolType correction = pn_info_find(&info, "AUDIOCORRECT");
    player->rh_correction = pn_tool_type_is(&correction, "RH");
    player->range = pn_info_song_range(&info);
    return player;
}

void pn_destroy(PnPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->song);
    free(player->info);
    free(player->waves);
    free(player);
}

uint32_t pn_subsong_count(const PnPlayer* player) {
    return (uint32_t)player->range.last - player->range.first + 1;
}

uint32_t pn_default_subsong(const PnPlayer* player) {
    return (uint32_t)player->range.default_song - player->range.first;
}

static void reset_voice(PnVoice* voice) {
    *voice = (PnVoice) { .wave = PN_SILENT_WAVE, .length_words = PN_SILENT_WORDS, .wave_changed = true };
}

bool pn_start(PnPlayer* player, uint32_t subsong, NuadeVoices* voices) {
    if (player == nullptr || subsong >= pn_subsong_count(player)) {
        return false;
    }
    pn_machine_reset(&player->machine);
    pn_machine_load(&player->machine, player->song, player->song_size);
    const uint16_t init_address = pn_machine_u16(&player->machine, PN_RUNAD);
    const uint8_t song_number = (uint8_t)(player->range.first + subsong);
    if (pn_machine_run(&player->machine, init_address, song_number, 0, 0) != PnRun_Done) {
        return false;
    }
    player->play_address = pn_machine_u16(&player->machine, PN_VVBLKI);
    for (uint32_t i = 0; i < PN_VOICES; i++) {
        reset_voice(&player->voices[i]);
        if (voices != nullptr) {
            nuade_voice_stop(voices, i);
        }
    }
    return true;
}

// The jiffy clock counts up in three bytes, high byte first.

static void advance_clock(PnMachine* machine) {
    for (uint32_t i = 3; i-- > 0;) {
        if (++machine->ram[PN_RTCLOK + i] != 0) {
            return;
        }
    }
}

// What a new sound changes in the voice record: a different waveform restarts the channel (and with it the period).

static void record_sound(PnVoice* voice, PnSound sound) {
    if (sound.wave != voice->wave) {
        voice->wave_changed = true;
        voice->period_changed = true;
        voice->wave = sound.wave;
        voice->length_words = sound.length_words;
    } else {
        voice->wave_changed = false;
    }
    if (sound.period != voice->period) {
        voice->period = sound.period;
        voice->period_changed = true;
    }
    voice->volume = sound.volume;
}

// The channel's registers for the record. A new waveform stops the channel's DMA, loads the registers and starts it
// again; the repeat is the same waveform.

static void write_voice(const PnPlayer* player, PnVoice* voice) {
    NuadePaulaChannel* paula = &voice->paula;
    if (voice->wave_changed) {
        // (The original first sets AUDxPER to 1 so that the old sample runs out quickly, and waits for the channel to
        // stop.)
        nuade_paula_channel_dma_off(paula);
        paula->lc = (const int8_t*)(player->waves + voice->wave);
        paula->len_words = voice->length_words;
        paula->period = voice->period;
        paula->volume = voice->volume;
        nuade_paula_channel_dma_on(paula);
        paula->lc = (const int8_t*)(player->waves + voice->wave);
        paula->len_words = voice->length_words;
    }
    if (voice->period_changed) {
        paula->period = voice->period;
        voice->period_changed = false;
    }
    paula->volume = voice->volume;
}

PnTickResult pn_tick(PnPlayer* player, NuadeVoices* voices) {
    advance_clock(&player->machine);
    const PnRunResult result = pn_machine_run(&player->machine, player->play_address, 0, 0, 0);
    if (result == PnRun_Done) {
        for (uint32_t i = 0; i < PN_VOICES; i++) {
            record_sound(&player->voices[i], pn_pokey_sound(&player->machine, i, player->rh_correction));
        }
        for (uint32_t i = 0; i < PN_VOICES; i++) {
            write_voice(player, &player->voices[i]);
        }
    }
    if (voices != nullptr) {
        for (uint32_t i = 0; i < PN_VOICES; i++) {
            nuade_paula_channel_sync(&player->voices[i].paula, i, voices);
        }
    }
    return (PnTickResult) { .length = PN_TICK_LENGTH };
}

PnVoiceState pn_voice_state(const PnPlayer* player, uint32_t voice) {
    const PnVoice* v = &player->voices[voice];
    return (PnVoiceState) { .period = v->period, .volume = v->volume };
}
