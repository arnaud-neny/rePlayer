// SPDX-License-Identifier: MIT
// Jason C. Brooke player: translated from the UADE eagleplayer "Jason C. Brooke player" (Wanted Team).
//
// A module is the game's sound driver itself. Two kinds exist: the early driver (version B, 4 modules) starts with
// movem.l d0-d3/a0-a3,-(a7); the later one (version A, 14 modules) has its voice records and globals first, then an
// interrupt routine that disables the audio interrupts and points back at the module start. The eagleplayer scans the
// code for the interrupt and init routines and the tables, and calls them; this player finds the same tables and runs
// the replayer logic of each version in C (jb_a.c, jb_b.c).

#include "jb_internal.h"
#include <stdlib.h>
#include <string.h>

// JbVersion A's check: a movem within 26 words of 0xAC that saves the registers, switches the CIA LED filter, disables
// the audio interrupts and loads the module start pc-relative. Returns the offset of the movem or JB_NOT_FOUND.
static uint32_t find_version_a(const JbModule* m) {
    for (uint32_t at = 0xac; at <= 0xac + 2 * 25; at += 2) {
        if (jbm_u16(m, at) != 0x48e7) {
            continue;
        }
        const uint16_t registers = jbm_u16(m, at + 2);
        const bool match = (registers == 0xf8fc || registers == 0xf8f8) && jbm_u32(m, at + 4) == 0x08f90001
                           && jbm_u32(m, at + 8) == 0x00bfe001 && jbm_u32(m, at + 12) == 0x33fc0780
                           && jbm_u32(m, at + 16) == 0x00dff09a && jbm_u16(m, at + 20) == 0x47fa
                           && at + 22 + jbm_u16(m, at + 22) == 0x10000;
        return match ? at : JB_NOT_FOUND;
    }
    return JB_NOT_FOUND;
}

// JbVersion B's check: the first instructions of the interrupt routine
static bool is_version_b(const JbModule* m) {
    return jbm_u32(m, 0) == 0x48e7f0f0 && jbm_u32(m, 4) == 0x424047fa && jbm_u32(m, 8) == 0xfff84a2b;
}

bool jb_probe(const uint8_t* data, size_t size) {
    if (size < 16) {
        return false;
    }
    JbModule m = { .data = (uint8_t*)(uintptr_t)data, .size = (uint32_t)size };
    return is_version_b(&m) || find_version_a(&m) != JB_NOT_FOUND;
}

JbPlayer* jb_create(const uint8_t* data, size_t size) {
    if (!jb_probe(data, size)) {
        return nullptr;
    }
    JbPlayer* p = calloc(1, sizeof(*p));
    uint8_t* original = malloc(size);
    uint8_t* work = nuade_paula_padded_copy(data, size);
    if (p == nullptr || original == nullptr || work == nullptr) {
        free(p);
        free(original);
        free(work);
        return nullptr;
    }
    memcpy(original, data, size);
    p->original = (JbModule) { .data = original, .size = (uint32_t)size };
    p->work = (JbModule) { .data = work, .size = (uint32_t)size };

    bool located;
    if (is_version_b(&p->original)) {
        p->version = JbVersion_B;
        located = jb_b_locate(&p->original, &p->b.config);
        p->driver_name = "early driver (B)";
    } else {
        p->version = JbVersion_A;
        located = jb_a_locate(&p->original, find_version_a(&p->original), &p->a.config);
        const JbAConfig* c = &p->a.config;
        p->driver_name = c->new_syntax ? "driver A, new syntax" : c->pwm_noise ? "driver A, noise" : "driver A";
    }
    if (!located) {
        jb_destroy(p);
        return nullptr;
    }
    return p;
}

void jb_destroy(JbPlayer* player) {
    if (player != nullptr) {
        free(player->original.data);
        free(player->work.data);
        free(player);
    }
}

int jb_subsong_count(const JbPlayer* player) {
    return player->version == JbVersion_A ? player->a.config.subsong_count : player->b.config.subsong_count;
}

const char* jb_driver_name(const JbPlayer* player) {
    return player->driver_name;
}

bool jb_start(JbPlayer* player, int subsong) {
    if (subsong < 0 || subsong >= jb_subsong_count(player)) {
        return false;
    }
    memcpy(player->work.data, player->original.data, player->original.size);
    if (player->version == JbVersion_A) {
        jb_a_start(&player->a, &player->work, subsong);
    } else {
        jb_b_start(&player->b, &player->work, subsong);
    }
    return true;
}

JbTickResult jb_tick(JbPlayer* player, NuadeVoices* voices) {
    return player->version == JbVersion_A ? jb_a_tick(&player->a, &player->work, voices)
                                          : jb_b_tick(&player->b, &player->work, voices);
}

int jb_ticks_per_step(const JbPlayer* player) {
    if (player->version == JbVersion_B) {
        return player->b.tempo > 0 ? player->b.tempo : 1;
    }
    const int tempo = player->a.globals.tempo;
    return tempo > 0 ? (256 + tempo / 2) / tempo : 1;
}

JbVoiceState jb_voice_state(const JbPlayer* player, int voice) {
    if (voice < 0 || voice >= JB_VOICES) {
        return (JbVoiceState) { .note = -1, .sample = -1 };
    }
    if (player->version == JbVersion_A) {
        const JbAVoice* v = &player->a.voices[voice];
        return (JbVoiceState) { .note = v->note_started ? v->note : -1, .sample = v->sample };
    }
    const JbBVoice* v = &player->b.voices[voice];
    return (JbVoiceState) { .note = v->note_started ? v->note : -1, .sample = v->block };
}
