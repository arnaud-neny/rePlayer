// SPDX-License-Identifier: MIT
// Jesper Olsen player: module detection and the replayer versions behind one interface.

#include "jo_internal.h"
#include <stdlib.h>
#include <string.h>

// The replayer a module needs, by the checks of the eagleplayers
static bool detect(const JoModule* module, JoReplayer* replayer, const char** name, const JoAVersion** version_a) {
    *version_a = jo_a_detect(module);
    if (*version_a != nullptr) {
        *replayer = JoReplayer_A;
        *name = "replayer A (in the module)";
    } else if (jo_b_detect(module)) {
        *replayer = JoReplayer_B;
        *name = "replayer B";
    } else if (jo_c_detect(module)) {
        *replayer = JoReplayer_C;
        *name = "replayer C";
    } else {
        return false;
    }
    return true;
}

bool jo_probe(const uint8_t* data, size_t size) {
    if (size < 16 || size > JO_MAX_MODULE_SIZE) {
        return false;
    }
    const JoModule module = { .data = data, .size = (uint32_t)size };
    JoReplayer replayer;
    const char* name;
    const JoAVersion* version_a;
    return detect(&module, &replayer, &name, &version_a);
}

JoPlayer* jo_create(const uint8_t* data, size_t size) {
    if (!jo_probe(data, size)) {
        return nullptr;
    }
    JoPlayer* p = calloc(1, sizeof(*p));
    uint8_t* storage = nuade_paula_padded_copy(data, size);
    if (p == nullptr || storage == nullptr) {
        free(p);
        free(storage);
        return nullptr;
    }
    p->storage = storage;
    p->module = (JoModule) { .data = storage, .size = (uint32_t)size };
    detect(&p->module, &p->replayer, &p->replayer_name, &p->version_a);
    return p;
}

void jo_destroy(JoPlayer* player) {
    if (player != nullptr) {
        free(player->storage);
        free(player);
    }
}

int jo_subsong_count(const JoPlayer* player) {
    switch (player->replayer) {
        case JoReplayer_A:
            return jo_a_subsong_count(player->version_a, &player->module);
        case JoReplayer_B:
            return 1;
        case JoReplayer_C:
            break;
    }
    const int count = jo_c_subsong_count(&player->module);
    return count > 0 ? count : 1;
}

const char* jo_replayer_name(const JoPlayer* player) {
    return player->replayer_name;
}

bool jo_start(JoPlayer* player, int subsong) {
    if (subsong < 0 || subsong >= jo_subsong_count(player)) {
        return false;
    }
    memset(&player->state, 0, sizeof(player->state));
    switch (player->replayer) {
        case JoReplayer_A:
            jo_a_start(&player->state.a, player->version_a, &player->module, subsong);
            break;
        case JoReplayer_B:
            jo_b_start(&player->state.b, &player->module, subsong);
            break;
        case JoReplayer_C:
            jo_c_start(&player->state.c, &player->module, subsong);
            break;
    }
    return true;
}

JoTickResult jo_tick(JoPlayer* player, NuadeVoices* voices) {
    switch (player->replayer) {
        case JoReplayer_A:
            return jo_a_tick(&player->state.a, &player->module, voices);
        case JoReplayer_B:
            return jo_b_tick(&player->state.b, &player->module, voices);
        case JoReplayer_C:
            break;
    }
    return jo_c_tick(&player->state.c, &player->module, voices);
}

JoVoiceState jo_voice_state(const JoPlayer* player, int voice) {
    if (voice < 0 || voice >= JO_VOICES) {
        return (JoVoiceState) { .note = -1, .instrument = -1 };
    }
    switch (player->replayer) {
        case JoReplayer_A:
            return jo_a_voice_state(&player->state.a, voice);
        case JoReplayer_B:
            return jo_b_voice_state(&player->state.b, voice);
        case JoReplayer_C:
            break;
    }
    return jo_c_voice_state(&player->state.c, voice);
}
