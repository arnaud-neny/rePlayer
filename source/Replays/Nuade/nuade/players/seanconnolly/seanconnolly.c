// SPDX-License-Identifier: MIT
// Sean Connolly: the player's lifecycle around the replayers, as the eagleplayer's glue drives them.
//
// The eagleplayer's interrupt first looks at the module's voice-active flags: when all four are 0 it calls SongEnd and
// runs the init routine again, then it calls the module's interrupt routine. UADE plays on past SongEnd, so the song
// restarts at once.

#include "nuade_base.h"
#include "sc_internal.h"
#include "sc_v2.h"
#include "sc_v3.h"

#include <stdlib.h>
#include <string.h>

bool sco_probe(const uint8_t* data, size_t size) {
    ScoModule module;
    if (!sco_module_load(&module, data, size)) {
        return false;
    }
    sco_module_free(&module);
    return true;
}

ScoPlayer* sco_create(const uint8_t* data, size_t size) {
    ScoPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return nullptr;
    }
    if (!sco_module_load(&player->module, data, size)) {
        free(player);
        return nullptr;
    }
    return player;
}

void sco_destroy(ScoPlayer* player) {
    if (player != nullptr) {
        sco_module_free(&player->module);
        free(player);
    }
}

uint32_t sco_subsong_count(const ScoPlayer* player) {
    return player->module.subsong_count;
}

const char* sco_version_name(const ScoPlayer* player) {
    return player->module.version->name;
}

static bool is_sampler(const ScoPlayer* player) {
    return player->module.version->family == ScoFamily_Sampler;
}

static void replayer_init(ScoPlayer* player, bool first) {
    if (is_sampler(player)) {
        sc2_init(&player->sampler, &player->module, player->subsong, first, player->paula);
    } else {
        sc3_init(&player->sequencer, &player->module, player->subsong, player->paula);
    }
}

bool sco_start(ScoPlayer* player, uint32_t subsong, NuadeVoices* voices) {
    if (player == nullptr || subsong >= player->module.subsong_count) {
        return false;
    }
    player->subsong = subsong;
    sco_module_reset(&player->module);
    memset(player->paula, 0, sizeof(player->paula));
    memset(&player->sampler, 0, sizeof(player->sampler));
    replayer_init(player, true);
    if (voices != nullptr) {
        for (uint32_t v = 0; v < SCO_VOICES; v++) {
            nuade_voice_stop(voices, v);
        }
    }
    return true;
}

ScoTickResult sco_tick(ScoPlayer* player, NuadeVoices* voices) {
    ScoTickResult result = { 0 };
    const bool sampler = is_sampler(player);
    if (sampler ? sc2_ended(&player->sampler) : sc3_ended(&player->sequencer)) {
        result.looped = true;
        replayer_init(player, false);
    }
    if (sampler) {
        sc2_tick(&player->sampler, player->paula);
        result.looped = result.looped || player->sampler.song_end;
        result.ended = player->sampler.unmodeled || sco_module_var_read(&player->module);
    } else {
        sc3_tick(&player->sequencer, player->paula);
        result.looped = result.looped || player->sequencer.song_end;
        result.ended = player->sequencer.unmodeled || sco_module_var_read(&player->module);
    }
    for (uint32_t v = 0; v < SCO_VOICES; v++) {
        nuade_paula_channel_sync(&player->paula[v], v, voices);
    }
    return result;
}

ScoVoiceState sco_voice_state(const ScoPlayer* player, uint32_t voice) {
    if (is_sampler(player)) {
        const Sco2Voice* v = &player->sampler.voice[voice];
        return v->played ? (ScoVoiceState) { .period = v->note_period, .instrument = v->instrument }
                         : (ScoVoiceState) { .period = 0, .instrument = -1 };
    }
    const Sco3Voice* v = &player->sequencer.voice[voice];
    return v->played ? (ScoVoiceState) { .period = v->period, .instrument = v->instrument }
                     : (ScoVoiceState) { .period = 0, .instrument = -1 };
}
