// SPDX-License-Identifier: MIT
// Jason Page player: lifecycle and the tick.
//
// The three replayers of the eagleplayer (one per module format) share this order of work: the sequencer, then every
// voice (queued instrument, script, pitch, envelope), and the Paula registers of the voices. Graftgold and Virocop
// write the registers of all four voices after all four have run; Realms writes each voice's right after it.
//
// The eagleplayer's fade of the master volume starts from full volume and is never changed afterwards, so the volume
// of a voice is its envelope level / 4, at most 63.

#include "jp_internal.h"

#include <stdlib.h>

bool jp_probe(const uint8_t* data, size_t size) {
    return jp_module_probe(data, size);
}

JpCreateResult jp_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    JpCreateResult result = { .status = JpStatus_InvalidData };
    JpPlayer* player = calloc(1, sizeof(JpPlayer));
    if (player == nullptr) {
        result.status = JpStatus_OutOfMemory;
        return result;
    }
    result.status = jp_module_load(player, data, size, path, io);
    if (result.status != JpStatus_Ok) {
        jp_module_free(player);
        free(player);
        return result;
    }
    result.player = player;
    return result;
}

void jp_destroy(JpPlayer* player) {
    if (player == nullptr) {
        return;
    }
    jp_module_free(player);
    free(player);
}

int jp_subsong_count(const JpPlayer* player) {
    return player->layout.subsongs;
}

// InitSound: the subsong's tables and the voices at the head of their track lists. Virocop and Realms also write 0 to
// the volume registers.

static void init_song(JpPlayer* player, int subsong) {
    jp_seq_reset(player, subsong);
    if (player->format != JpFormat_Graftgold) {
        for (int i = 0; i < JP_VOICES; i++) {
            player->voices[i].paula.volume = 0;
        }
    }
}

bool jp_start(JpPlayer* player, int subsong, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || subsong < 0 || subsong >= player->layout.subsongs) {
        return false;
    }
    jp_module_reset(player);
    player->looped = false;
    player->failed = false;
    for (int i = 0; i < JP_VOICES; i++) {
        jp_voice_initial_state(&player->voices[i], player->format, (uint8_t)i);
        nuade_voice_stop(voices, (uint32_t)i);
        nuade_voice_set_volume(voices, (uint32_t)i, 0.0f);
        // Paula channels 0 and 3 are left, 1 and 2 right
        nuade_voice_set_pan(voices, (uint32_t)i, (i == 0 || i == 3) ? -1.0f : 1.0f);
    }
    init_song(player, subsong);
    return true;
}

JpTickResult jp_tick(JpPlayer* player, NuadeVoices* voices) {
    player->looped = false;
    if (!player->failed) {
        const bool output_per_voice = player->format == JpFormat_Realms;
        jp_seq_step(player);
        for (int i = 0; i < JP_VOICES; i++) {
            jp_voice_tick(player, &player->voices[i]);
            if (output_per_voice) {
                jp_voice_output(player, &player->voices[i]);
            }
        }
        for (int i = 0; i < JP_VOICES && !output_per_voice; i++) {
            jp_voice_output(player, &player->voices[i]);
        }
        if (!player->active && !player->failed) {
            // The track lists ended the song: the eagleplayer reports the end and starts the subsong again.
            player->looped = true;
            init_song(player, player->subsong);
        }
    }
    for (int i = 0; i < JP_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, (uint32_t)i, voices);
    }
    return (JpTickResult) { .looped = player->looped, .ended = player->failed };
}

uint32_t jp_grid_ticks(const JpPlayer* player, int subsong) {
    if (player == nullptr || subsong < 0 || subsong >= player->layout.subsongs) {
        return 0;
    }
    const uint32_t index = (uint32_t)subsong & JP_SUBSONG_MASK;
    return jp_table_value(player, player->layout.speed_table, index) + 1u;
}

JpVoiceState jp_voice_state(const JpPlayer* player, int voice) {
    if (player == nullptr || voice < 0 || voice >= JP_VOICES) {
        return (JpVoiceState) { .note = -1, .instrument = -1 };
    }
    return (JpVoiceState) { .note = player->voices[voice].note, .instrument = player->voices[voice].instrument_shown };
}
