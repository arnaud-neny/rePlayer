// SPDX-License-Identifier: MIT
// Speedy System player, translated from the UADE eagleplayer "SpeedySystem" (see ss_internal.h for the file).
//
// A song is a list of positions of 16 rows; a row holds one three byte cell for each of the four voices. The module's
// speed byte gives the ticks of a row (speed + 1). Once per tick: (1) on the first tick of a row every voice reads its
// cell: a note stops the voice, starts a sample and sets the attack and release delays; an effect sets a period slide;
// the DMA of the voices that play is switched on and, for a sample that has a repeat part, the repeat is written after
// the DMA start; (2) every voice that plays runs its slide and its envelope: the volume rises in steps of 8 to 64 (one
// step every delay + 1 ticks), holds for the release delay, then falls in steps of 8 to 0, which stops the voice. A
// release delay of 0 holds the voice at full volume for good.

#include <nuade/players/speedysystem.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_tracker.h"
#include "ss_internal.h"
#include <stdlib.h>

#define MAX_VOLUME 64
#define VOLUME_STEP 8
#define EFFECT_SLIDE_ADD 0x50 // the period grows by the amount each tick
#define EFFECT_SLIDE_SUB 0x51
#define HOLD_LENGTH_WORDS 4

// Eight bytes of zero: what a voice plays at the end of its sample once the replayer has pointed the repeat at it.
static const int8_t k_silence[HOLD_LENGTH_WORDS * 2] = { 0 };

bool ss_probe(const uint8_t* data, size_t size) {
    SsModule module;
    return ss_module_parse(&module, data, size);
}

SsPlayer* ss_create(const uint8_t* data, size_t size) {
    SsPlayer* p = calloc(1, sizeof(SsPlayer));
    uint8_t* image = nuade_paula_padded_copy(data, size);
    if (p != nullptr && image != nullptr) {
        if (ss_module_parse(&p->module, image, size)) {
            p->image = image;
            return p;
        }
    }
    free(p);
    free(image);
    return nullptr;
}

void ss_destroy(SsPlayer* player) {
    if (player != nullptr) {
        free(player->image);
    }
    free(player);
}

// InitSound: the variables are 0 (the first tick starts a row at the first position).

bool ss_start(SsPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    *player = (SsPlayer) { .image = player->image, .module = player->module };
    for (unsigned i = 0; i < SS_VOICES; i++) {
        player->voices[i] = (SsVoice) { .note = -1, .instrument = -1 };
        nuade_paula_channel_dma_off(&player->paula[i]);
        nuade_voice_stop(voices, i);
    }
    return true;
}

uint32_t ss_grid_ticks(const SsPlayer* player) {
    return (uint32_t)player->module.speed + 1;
}

SsVoiceState ss_voice_state(const SsPlayer* player, int voice) {
    const SsVoice* v = &player->voices[voice];
    return (SsVoiceState) { .note = v->note, .instrument = v->instrument };
}

// A note: the voice stops, then the delays are set and the sample starts. AUDxLC/AUDxLEN are the DMA start's; the
// repeat part, if any, follows after the DMA start.

static void start_note(SsPlayer* p, unsigned index, SsCell cell) {
    SsVoice* voice = &p->voices[index];
    NuadePaulaChannel* channel = &p->paula[index];
    voice->active = false;
    nuade_paula_channel_dma_off(channel);

    const uint16_t period = ss_note_period(cell);
    voice->period = period;
    voice->effect = SsEffect_None;
    const SsNoteStart start = ss_cell_start(cell);
    if (start.kind == SsStart_Silent) {
        return;
    }
    const SsDelays delays = ss_cell_delays(cell);
    voice->attack_delay = delays.attack;
    voice->release_delay = delays.release;
    if (start.kind != SsStart_Sample) {
        return;
    }

    const SsSample* sample = &p->module.samples[start.sample];
    voice->attacking = true;
    voice->releasing = true;
    channel->period = period;
    channel->lc = (const int8_t*)p->module.data + sample->start;
    channel->len_words = sample->start_len_words;
    voice->repeat_pending = sample->has_repeat;
    voice->sample = start.sample;
    voice->volume = 0;
    voice->active = true;
    voice->delay_counter = 0;
    voice->note = nuade_period_to_note(period);
    voice->instrument = (int)start.sample;
}

static void start_effect(SsVoice* voice, SsCell cell) {
    switch (cell.first) {
        case EFFECT_SLIDE_ADD:
            voice->effect = SsEffect_PeriodAdd;
            break;
        case EFFECT_SLIDE_SUB:
            voice->effect = SsEffect_PeriodSub;
            break;
        default:
            voice->effect = SsEffect_None;
            break;
    }
    voice->effect_amount = (uint16_t)((cell.second << 8) | cell.third);
}

// True for the end cell of voice 0: the song restarts and the other voices skip the row.

static bool run_cell(SsPlayer* p, unsigned voice, SsCell cell) {
    switch (ss_cell_kind(cell)) {
        case SsCell_End:
            return voice == 0;
        case SsCell_Effect:
            start_effect(&p->voices[voice], cell);
            return false;
        case SsCell_Note:
            start_note(p, voice, cell);
            return false;
        case SsCell_Skip:
            return false;
    }
    return false;
}

// The DMA of every voice that plays is switched on; a channel that already runs is not restarted.

static void start_dma(SsPlayer* p) {
    for (unsigned i = 0; i < SS_VOICES; i++) {
        if (p->voices[i].active) {
            nuade_paula_channel_dma_on(&p->paula[i]);
        }
    }
}

// A voice without a release delay holds at full volume: the channel's repeat points at 8 bytes of zero, so the sample
// plays once.

static void hold_at_sample_end(SsPlayer* p, unsigned voice) {
    p->paula[voice].lc = k_silence;
    p->paula[voice].len_words = HOLD_LENGTH_WORDS;
}

static void run_effect(SsPlayer* p, unsigned index) {
    SsVoice* voice = &p->voices[index];
    switch (voice->effect) {
        case SsEffect_PeriodAdd:
            voice->period = (uint16_t)(voice->period + voice->effect_amount);
            break;
        case SsEffect_PeriodSub:
            voice->period = (uint16_t)(voice->period - voice->effect_amount);
            break;
        case SsEffect_None:
            return;
    }
    p->paula[index].period = voice->period;
}

// The volume rises by 8 every attack delay + 1 ticks (at once to 64 with no delay).

static void run_attack(SsVoice* voice) {
    if (voice->delay_counter != 0) {
        voice->delay_counter--;
        return;
    }
    if (voice->attack_delay != 0) {
        voice->delay_counter = voice->attack_delay;
        voice->volume += VOLUME_STEP;
        if (voice->volume != MAX_VOLUME) {
            return;
        }
    }
    voice->attacking = false;
    voice->volume = MAX_VOLUME;
    voice->delay_counter = voice->release_delay;
}

// After the release delay the volume falls by 8 every release delay + 1 ticks; at 0 the voice stops.

static void run_release(SsPlayer* p, unsigned index) {
    SsVoice* voice = &p->voices[index];
    if (!voice->releasing) {
        return;
    }
    if (voice->delay_counter != 0) {
        voice->delay_counter--;
        return;
    }
    voice->delay_counter = voice->release_delay;
    voice->volume -= VOLUME_STEP;
    if ((int16_t)voice->volume < 1) {
        voice->releasing = false;
        voice->volume = 0;
        voice->active = false;
        nuade_paula_channel_dma_off(&p->paula[index]);
    }
}

static void update_voices(SsPlayer* p) {
    for (unsigned i = 0; i < SS_VOICES; i++) {
        SsVoice* voice = &p->voices[i];
        if (!voice->active) {
            continue;
        }
        run_effect(p, i);
        if (voice->attacking) {
            run_attack(voice);
        } else if (voice->release_delay == 0) {
            hold_at_sample_end(p, i);
            continue; // the volume is not written
        } else {
            run_release(p, i);
        }
        p->paula[i].volume = (uint8_t)p->voices[i].volume;
    }
}

// The repeat part of an IFF sample, written after the DMA start.

static void write_repeats(SsPlayer* p) {
    for (unsigned i = 0; i < SS_VOICES; i++) {
        SsVoice* voice = &p->voices[i];
        if (voice->repeat_pending) {
            const SsSample* sample = &p->module.samples[voice->sample];
            p->paula[i].lc = (const int8_t*)p->module.data + sample->repeat;
            p->paula[i].len_words = sample->repeat_len_words;
            voice->repeat_pending = false;
        }
    }
}

static void restart_song(SsPlayer* p) {
    p->position = 0;
    p->row = 0;
    p->tick_counter = 0;
}

// The first tick of a row. Returns true when the song restarted.

static bool run_row(SsPlayer* p) {
    p->tick_counter = p->module.speed;
    SsCell rows[SS_ROWS][SS_VOICES];
    bool restarted = !ss_expand_position(&p->module, p->position, rows);
    for (unsigned voice = 0; voice < SS_VOICES && !restarted; voice++) {
        restarted = run_cell(p, voice, rows[p->row][voice]);
    }
    if (!restarted && ++p->row == SS_ROWS) {
        p->row = 0;
        p->position++;
        restarted = p->position == 0; // the position byte wrapped
    }
    if (restarted) {
        restart_song(p);
    }
    start_dma(p);
    update_voices(p);
    write_repeats(p);
    return restarted;
}

SsTickResult ss_tick(SsPlayer* player, NuadeVoices* voices) {
    SsTickResult result = { 0 };
    if (player->tick_counter != 0) {
        player->tick_counter--;
        update_voices(player);
    } else {
        result.looped = run_row(player);
    }
    for (unsigned i = 0; i < SS_VOICES; i++) {
        nuade_paula_channel_sync(&player->paula[i], i, voices);
    }
    return result;
}
