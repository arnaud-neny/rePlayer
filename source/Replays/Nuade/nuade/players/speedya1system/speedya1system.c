// SPDX-License-Identifier: MIT
// Speedy A1 System player, translated from the UADE eagleplayer "SpeedyA1System" (see sa_internal.h for the file).
//
// A song is a list of positions of 16 rows; the order list names a pattern for each voice and a row holds one three
// byte cell for each. The header gives the ticks of a row (speed + 1). Once per tick: (1) on the first tick of a row
// every voice reads its cell: a note stops the voice and starts a sample (the notes of the row start together: the DMA
// of those voices is switched on after the last cell); an effect sets a period slide or a volume; (2) every voice
// writes the repeat of its sample, runs its slide and its envelope and writes its volume. A note with an envelope byte
// rises to volume 64 in steps of 8, then falls to 0 in steps of 8 (one step every delay + 1 ticks, a delay from the
// nibbles of the envelope byte); a note without one plays its sample once at volume 64.

#include <nuade/players/speedya1system.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_tracker.h"
#include "sa_internal.h"
#include <stdlib.h>

#define MAX_VOLUME 64
#define VOLUME_STEP 8
#define EFFECT_PERIOD_ADD 0x50
#define EFFECT_PERIOD_SUB 0x51
#define EFFECT_ATTACK_VOLUME 0x52
#define SILENCE_LENGTH_WORDS 5

// Ten bytes of zero: what a voice plays after its sample.
static const int8_t k_silence[SILENCE_LENGTH_WORDS * 2] = { 0 };

bool sa1_probe(const uint8_t* data, size_t size) {
    Sa1Module module;
    return sa1_module_parse(&module, data, size);
}

Sa1Player* sa1_create(const uint8_t* data, size_t size) {
    Sa1Player* p = calloc(1, sizeof(Sa1Player));
    uint8_t* image = nuade_paula_padded_copy(data, size);
    if (p != nullptr && image != nullptr) {
        if (sa1_module_parse(&p->module, image, size)) {
            p->image = image;
            return p;
        }
    }
    free(p);
    free(image);
    return nullptr;
}

void sa1_destroy(Sa1Player* player) {
    if (player != nullptr) {
        free(player->image);
    }
    free(player);
}

// InitSound: the variables are 0, the song starts at the header's start position.

bool sa1_start(Sa1Player* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    *player = (Sa1Player) { .image = player->image, .module = player->module };
    player->position = player->module.start_position;
    for (unsigned i = 0; i < SA1_VOICES; i++) {
        player->voices[i] = (Sa1Voice) { .note = -1, .instrument = -1 };
        nuade_paula_channel_dma_off(&player->paula[i]);
        nuade_voice_stop(voices, i);
    }
    return true;
}

uint32_t sa1_grid_ticks(const Sa1Player* player) {
    return player->module.speed + 1;
}

Sa1VoiceState sa1_voice_state(const Sa1Player* player, int voice) {
    const Sa1Voice* v = &player->voices[voice];
    return (Sa1VoiceState) { .note = v->note, .instrument = v->instrument };
}

static const int8_t* sample_data(const Sa1Player* p, uint32_t offset) {
    return (const int8_t*)p->module.data + offset;
}

// A note with an envelope starts at volume 0 and rises; with a high nibble of 0 it starts at 64 and only falls.

static void start_envelope(Sa1Voice* voice, Sa1Cell cell) {
    const Sa1Delays delays = sa1_cell_delays(cell);
    voice->attack_delay = delays.attack;
    voice->release_delay = delays.release;
    if (delays.attack == 0) {
        voice->envelope = Sa1Envelope_Release;
        voice->volume = MAX_VOLUME;
        voice->counter = voice->release_delay;
    } else {
        voice->envelope = Sa1Envelope_Attack;
        voice->volume = 0;
        voice->counter = voice->attack_delay;
    }
}

// A note: the channel's DMA is switched off, AUDxLC/AUDxLEN/AUDxPER are set for the DMA start the row ends with.

static void start_note(Sa1Player* p, unsigned index, Sa1Cell cell) {
    const uint16_t period = sa1_note_period(cell.first);
    if (period == 0 || cell.third >= SA1_SAMPLES) {
        return;
    }
    Sa1Voice* voice = &p->voices[index];
    NuadePaulaChannel* channel = &p->paula[index];
    const Sa1Sample* sample = &p->module.samples[cell.third];
    nuade_paula_channel_dma_off(channel);

    if (cell.second == 0) {
        voice->envelope = Sa1Envelope_Off;
        voice->volume = MAX_VOLUME;
        voice->in_sample = false;
        channel->lc = sample_data(p, sample->start);
        channel->len_words = sample->plain_len_words;
    } else {
        start_envelope(voice, cell);
        voice->in_sample = true;
        voice->sample = cell.third;
        channel->lc = sample_data(p, sample->start);
        channel->len_words = sample->start_len_words;
    }
    voice->period = period;
    channel->period = period;
    voice->dma_pending = true;
    voice->note = nuade_period_to_note(period);
    voice->instrument = (int)cell.third;
}

static void start_effect(Sa1Voice* voice, Sa1Cell cell) {
    switch (cell.first) {
        case EFFECT_PERIOD_ADD:
            voice->effect = Sa1Effect_PeriodAdd;
            break;
        case EFFECT_PERIOD_SUB:
            voice->effect = Sa1Effect_PeriodSub;
            break;
        case EFFECT_ATTACK_VOLUME:
            voice->effect = Sa1Effect_AttackVolume;
            break;
        default:
            voice->effect = Sa1Effect_None;
            break;
    }
    voice->effect_amount = (uint16_t)((cell.second << 8) | cell.third);
}

// The channels of the notes of the row start together.

static void start_dma(Sa1Player* p) {
    for (unsigned i = 0; i < SA1_VOICES; i++) {
        if (p->voices[i].dma_pending) {
            nuade_paula_channel_dma_on(&p->paula[i]);
            p->voices[i].dma_pending = false;
        }
    }
}

// A voice without a sample to repeat plays silence after its sample; the repeat part of a sample with an envelope
// follows the DMA start.

static void write_repeat(Sa1Player* p, unsigned index) {
    const Sa1Voice* voice = &p->voices[index];
    NuadePaulaChannel* channel = &p->paula[index];
    const Sa1Sample* sample = &p->module.samples[voice->sample];
    if (voice->envelope == Sa1Envelope_Off || !voice->in_sample) {
        channel->lc = k_silence;
        channel->len_words = SILENCE_LENGTH_WORDS;
    } else if (sample->has_repeat) {
        channel->lc = sample_data(p, sample->repeat);
        channel->len_words = sample->repeat_len_words;
    }
}

// After the release the voice is silent: its envelope is off.

static void run_release(Sa1Voice* voice) {
    if (voice->counter != 0) {
        voice->counter--;
    } else if (voice->volume != 0) {
        voice->volume -= VOLUME_STEP;
        voice->counter = (uint16_t)(voice->release_delay - 1);
    } else {
        voice->envelope = Sa1Envelope_Off;
    }
}

// The volume rises by 8 every attack delay + 1 ticks; at 64 the release starts in the same tick.

static void run_attack(Sa1Voice* voice) {
    if (voice->counter != 0) {
        voice->counter--;
    } else if (voice->volume != MAX_VOLUME) {
        voice->volume += VOLUME_STEP;
        voice->counter = (uint16_t)(voice->attack_delay - 1);
    } else {
        voice->envelope = Sa1Envelope_Release;
        voice->counter = (uint16_t)(voice->release_delay - 1);
        run_release(voice);
    }
}

static void run_envelope(Sa1Voice* voice) {
    switch (voice->envelope) {
        case Sa1Envelope_Attack:
            run_attack(voice);
            break;
        case Sa1Envelope_Release:
            run_release(voice);
            break;
        case Sa1Envelope_Off:
            break;
    }
}

// The slide writes the period every tick, of a voice that plays or not.

static void run_effect(Sa1Player* p, unsigned index) {
    Sa1Voice* voice = &p->voices[index];
    switch (voice->effect) {
        case Sa1Effect_PeriodAdd:
            voice->period = (uint16_t)(voice->period + voice->effect_amount);
            p->paula[index].period = voice->period;
            run_envelope(voice);
            break;
        case Sa1Effect_PeriodSub:
            voice->period = (uint16_t)(voice->period - voice->effect_amount);
            p->paula[index].period = voice->period;
            run_envelope(voice);
            break;
        case Sa1Effect_AttackVolume:
            voice->volume = voice->attack_delay;
            break;
        case Sa1Effect_None:
            run_envelope(voice);
            break;
    }
}

static void update_voices(Sa1Player* p) {
    for (unsigned i = 0; i < SA1_VOICES; i++) {
        write_repeat(p, i);
        run_effect(p, i);
        p->paula[i].volume = (uint8_t)p->voices[i].volume;
    }
}

// SongEnd: back to the start position, the effects are cleared.

static void end_song(Sa1Player* p) {
    p->position = p->module.start_position;
    p->row = 0;
    for (unsigned i = 0; i < SA1_VOICES; i++) {
        p->voices[i].effect = Sa1Effect_None;
    }
}

// The first tick of a row. Returns true when the song ended. A cell that ends the song skips the other voices (and the
// effects of this tick); the song also ends when the position reaches the header's end position.

static bool run_row(Sa1Player* p) {
    p->tick_counter = p->module.speed;
    bool ended = false;
    for (unsigned voice = 0; voice < SA1_VOICES && !ended; voice++) {
        Sa1Cell cell;
        const Sa1Fetch fetch = sa1_fetch_cell(&p->module, p->position, p->row, voice, &cell);
        if (fetch != Sa1Fetch_Cell) {
            ended = true;
            end_song(p);
        } else if (cell.first >= SA1_EFFECT_FIRST) {
            start_effect(&p->voices[voice], cell);
        } else {
            start_note(p, voice, cell);
        }
    }
    start_dma(p);
    update_voices(p);
    if (!ended && ++p->row == SA1_ROWS) {
        p->row = 0;
        p->position++;
        if (p->position == p->module.end_position) {
            ended = true;
            end_song(p);
        }
    }
    return ended;
}

Sa1TickResult sa1_tick(Sa1Player* player, NuadeVoices* voices) {
    Sa1TickResult result = { 0 };
    if (player->tick_counter != 0) {
        player->tick_counter--;
        update_voices(player);
    } else {
        result.looped = run_row(player);
    }
    for (unsigned i = 0; i < SA1_VOICES; i++) {
        nuade_paula_channel_sync(&player->paula[i], i, voices);
    }
    return result;
}
