// SPDX-License-Identifier: MIT
// Paul Shields player: one interrupt of the replayer. The channels whose notes started in the last tick get their DMA
// first, then the voices are stepped, and the loop pointers go in for the channels that just started.

#include "ps_internal.h"
#include <stdlib.h>
#include <string.h>

PshPlayer* psh_create(const uint8_t* data, size_t size) {
    PshPlayer* p = calloc(1, sizeof(*p));
    if (p == nullptr) {
        return nullptr;
    }
    if (!psh_module_load(&p->module, data, size)) {
        free(p);
        return nullptr;
    }
    return p;
}

void psh_destroy(PshPlayer* p) {
    if (p == nullptr) {
        return;
    }
    psh_module_free(&p->module);
    free(p);
}

// The voices at song start: they read the idle stream, whose only command is the next pattern, in their first tick.

bool psh_start(PshPlayer* p, NuadeVoices* voices) {
    if (p == nullptr || voices == nullptr) {
        return false;
    }
    const PshModule* m = &p->module;
    memset(p->voices, 0, sizeof(p->voices));
    memset(p->paula, 0, sizeof(p->paula));
    p->song_end = false;
    for (uint32_t i = 0; i < PSH_VOICES; i++) {
        PshVoice* v = &p->voices[i];
        v->active = true;
        v->volume_current = m->voices[i].volume;
        v->volume_target = m->voices[i].volume;
        v->order_restart = m->voices[i].order_list;
        v->order_next = v->order_restart;
        v->stream = (int32_t)m->stream_idle;
        v->duration = 1;
        v->pattern_repeats = 1;
        v->length_words = 1;
        v->loop_length_words = 1;
        v->sample = m->idle_sample;
        v->loop = m->idle_sample;
        v->pitch_restart = (int32_t)m->envelope_idle;
        v->pitch_next = (int32_t)m->envelope_idle;
        v->volume_restart = (int32_t)m->envelope_idle;
        v->volume_next = (int32_t)m->envelope_idle;
        v->display_note = -1;
        v->display_instrument = -1;
        nuade_voice_stop(voices, i);
    }
    return true;
}

PshTickResult psh_tick(PshPlayer* p, NuadeVoices* voices) {
    bool repeat_now[PSH_VOICES];
    p->song_end = false;
    for (uint32_t i = 0; i < PSH_VOICES; i++) {
        PshVoice* v = &p->voices[i];
        if (v->dma_pending) {
            nuade_paula_channel_dma_on(&p->paula[i]);
        }
        repeat_now[i] = v->repeat_pending;
        v->dma_pending = false;
        v->repeat_pending = false;
    }
    void (*const step_voice)(PshPlayer*, uint32_t)
        = p->module.layout->kind == PshKind_Old ? psh_step_voice_old : psh_step_voice;
    for (int i = PSH_VOICES - 1; i >= 0; i--) {
        step_voice(p, (uint32_t)i);
    }
    // The replayer waits for the DMA to start, then writes the repeat of the channels that started.
    for (uint32_t i = 0; i < PSH_VOICES; i++) {
        if (repeat_now[i]) {
            p->paula[i].lc = psh_chip_pointer(&p->module, p->voices[i].loop);
            p->paula[i].len_words = p->voices[i].loop_length_words;
        }
        nuade_paula_channel_sync(&p->paula[i], i, voices);
    }
    bool ended = true;
    for (uint32_t i = 0; i < PSH_VOICES; i++) {
        ended = ended && !p->voices[i].active;
    }
    return (PshTickResult) { .looped = p->song_end, .ended = ended };
}

PshVoiceState psh_voice_state(const PshPlayer* p, int voice) {
    if (voice < 0 || voice >= PSH_VOICES) {
        return (PshVoiceState) { .note = -1, .instrument = -1 };
    }
    return (PshVoiceState) { .note = p->voices[voice].display_note, .instrument = p->voices[voice].display_instrument };
}
