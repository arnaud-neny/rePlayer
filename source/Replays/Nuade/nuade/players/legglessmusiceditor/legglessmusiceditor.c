// SPDX-License-Identifier: MIT
// Leggless Music Editor player: the replayer of the UADE eagleplayer "LegglessMusicEditor". Every tick it writes the
// repeat registers of the notes started in the tick before, runs the pitch slides, the vibrato, the arpeggios, the
// volume envelopes and the sample retriggers of the four voices, reads the next event of every voice whose duration ran
// out, generates the synth waves and switches DMA on for the voices that started a sample.

#include <nuade/players/legglessmusiceditor.h>

#include "lme_internal.h"
#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>
#include <string.h>

// The new note's AUDxVOL is written with the voice's table offset instead of the volume (8 per voice), so a note starts
// at volume 0, 8, 16 or 24 until the envelope of the next tick writes the real one.
#define VOLUME_QUIRK_PER_VOICE 8
#define VOLUME_MASK 0x7Fu
#define SLIDE_DELAY_NONE ((int16_t)-1)

bool lme_probe(const uint8_t* data, size_t size) {
    LmeModule module;
    return lme_module_parse(&module, data, size) && lme_module_validate(&module);
}

LmePlayer* lme_create(const uint8_t* data, size_t size) {
    LmePlayer* player = calloc(1, sizeof(*player));
    uint8_t* bytes = nuade_paula_padded_copy(data, size);
    if (player == nullptr || bytes == nullptr) {
        free(player);
        free(bytes);
        return nullptr;
    }
    player->bytes = bytes;
    if (!lme_module_parse(&player->module, bytes, size) || !lme_module_validate(&player->module)) {
        lme_destroy(player);
        return nullptr;
    }
    return player;
}

void lme_destroy(LmePlayer* player) {
    if (player != nullptr) {
        free(player->bytes);
        free(player);
    }
}

uint32_t lme_subsong_count(const LmePlayer* player) {
    return player->module.subsong_count;
}

// InitSound: every voice reads its first event in the first tick.

bool lme_start(LmePlayer* player, uint32_t subsong, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || subsong >= player->module.subsong_count) {
        return false;
    }
    player->ended = false;
    for (uint32_t i = 0; i < LME_VOICES; i++) {
        const uint32_t list = lme_position_list(&player->module, subsong, i);
        player->voices[i] = (LmeVoice) {
            .duration_ticks = 1, .list_start = list, .list_pos = list, .list_pending = true, .shown_instrument = -1
        };
        nuade_voice_stop(voices, i);
    }
    return true;
}

// The volume register: the master volume and balance scaling of the eagleplayer is the identity at full volume.

static void write_volume(LmeVoice* voice, uint16_t volume) {
    voice->paula.volume = (uint8_t)(volume & VOLUME_MASK);
}

// The deferred writes of the notes started in the tick before: the repeat of the sample.

static void write_repeats(LmePlayer* player) {
    for (uint32_t ch = 0; ch < LME_VOICES; ch++) {
        LmeVoice* voice = &player->voices[ch];
        if (voice->loop_pending) {
            voice->loop_pending = false;
            voice->paula.lc = voice->loop_lc;
            voice->paula.len_words = voice->loop_len_words;
        }
    }
}

// The pitch slides: a delay, then a period change per tick until the target.

static void run_slides(LmePlayer* player) {
    for (uint32_t ch = 0; ch < LME_VOICES; ch++) {
        LmeVoice* voice = &player->voices[ch];
        LmeSlideState* slide = &voice->slide;
        if (slide->delta == 0) {
            continue;
        }
        if (slide->delay >= 0) {
            slide->delay--;
            if (slide->delay >= 0) {
                continue;
            }
        }
        voice->period = (uint16_t)(voice->period + (uint16_t)slide->delta);
        voice->paula.period = voice->period;
        slide->steps--;
        if (slide->steps == 0) {
            voice->period = slide->target;
            voice->paula.period = slide->target;
            slide->delta = 0;
        }
    }
}

// The vibrato offset runs between its limits (it turns round on equality only); the arpeggio writes the period itself.

static void run_vibratos(LmePlayer* player) {
    for (uint32_t ch = 0; ch < LME_VOICES; ch++) {
        LmeVoice* voice = &player->voices[ch];
        LmeVibrato* vibrato = &voice->sound.vibrato;
        if (vibrato->step == 0) {
            continue;
        }
        if (vibrato->delay >= 0) {
            vibrato->delay--;
            if (vibrato->delay >= 0) {
                continue;
            }
        }
        if (vibrato->direction == 0) {
            vibrato->offset = (int16_t)(vibrato->offset + vibrato->step);
            if (vibrato->offset == vibrato->max) {
                vibrato->direction |= LME_VIBRATO_DOWN;
            }
        } else {
            vibrato->offset = (int16_t)(vibrato->offset - vibrato->step);
            if (vibrato->offset == vibrato->min) {
                vibrato->direction = (uint16_t)(vibrato->direction & ~LME_VIBRATO_DOWN);
            }
        }
        if (voice->arpeggio.length == 0) {
            voice->paula.period = (uint16_t)(voice->period + (uint16_t)vibrato->offset);
        }
    }
}

// The arpeggio steps to its next slot every `reload` ticks and adds the vibrato offset.

static void run_arpeggios(LmePlayer* player) {
    for (uint32_t ch = 0; ch < LME_VOICES; ch++) {
        LmeVoice* voice = &player->voices[ch];
        LmeArpeggioState* arpeggio = &voice->arpeggio;
        arpeggio->slots[0] = voice->period;
        if (arpeggio->length == 0) {
            continue;
        }
        arpeggio->counter--;
        if (arpeggio->counter != 0) {
            continue;
        }
        arpeggio->counter = arpeggio->reload;
        voice->paula.period = (uint16_t)(arpeggio->slots[arpeggio->index] + (uint16_t)voice->sound.vibrato.offset);
        arpeggio->index++;
        if (arpeggio->index == arpeggio->length) {
            arpeggio->index = 0;
        }
    }
}

// The volume envelope: attack to its maximum, hold, decay to its minimum. The volume register is written every tick.

static void run_envelopes(LmePlayer* player) {
    for (uint32_t ch = 0; ch < LME_VOICES; ch++) {
        LmeVoice* voice = &player->voices[ch];
        LmeEnvelope* envelope = &voice->sound.envelope;
        int16_t volume = (int16_t)voice->sound.volume;
        if (envelope->phase == LmeEnvelopePhase_Attack) {
            volume = (int16_t)(volume + envelope->attack_step);
            if (lme_difference_not_negative(volume, envelope->attack_max)) {
                volume = envelope->attack_max;
                envelope->phase = LmeEnvelopePhase_Hold;
            }
        } else if (envelope->phase == LmeEnvelopePhase_Hold) {
            envelope->hold--;
            if (envelope->hold < 0) {
                envelope->phase = LmeEnvelopePhase_Decay;
            }
        } else if (envelope->phase == LmeEnvelopePhase_Decay) {
            volume = (int16_t)(volume - envelope->decay_step);
            if (volume <= envelope->decay_min) {
                volume = envelope->decay_min;
                envelope->phase = LmeEnvelopePhase_Finished;
            }
        }
        write_volume(voice, (uint16_t)volume);
        voice->sound.volume = (uint16_t)volume;
    }
}

// The sample restarts every `period` ticks: DMA off, the sample registers again, DMA on at the end of the tick.

static void start_sample(LmeVoice* voice) {
    nuade_paula_channel_dma_off(&voice->paula);
    voice->paula.lc = voice->sound.start;
    voice->paula.len_words = voice->sound.length_words;
    voice->loop_pending = true;
    voice->loop_lc = voice->sound.loop;
    voice->loop_len_words = voice->sound.loop_words;
    voice->dma_start = true;
}

static void run_retriggers(LmePlayer* player) {
    for (uint32_t ch = 0; ch < LME_VOICES; ch++) {
        LmeVoice* voice = &player->voices[ch];
        LmeRetrigger* retrigger = &voice->sound.retrigger;
        if (retrigger->period == 0) {
            continue;
        }
        retrigger->counter--;
        if (retrigger->counter == 0) {
            retrigger->counter = retrigger->period;
            start_sample(voice);
        }
    }
}

// The volume, vibrato, envelope, wave and retrigger of an instrument record.

static void copy_state(LmeVoice* voice, const LmeInstrument* instrument) {
    voice->sound.volume = instrument->volume;
    voice->sound.vibrato = instrument->vibrato;
    voice->sound.envelope = instrument->envelope;
    voice->sound.wave = instrument->wave;
    voice->sound.retrigger = instrument->retrigger;
}

// The instrument command copies the whole record. A synth wave plays from the voice's buffer, looping over all of it.

static void select_instrument(LmePlayer* player, LmeVoice* voice, uint16_t index) {
    const LmeInstrument instrument = lme_instrument(&player->module, index);
    voice->instrument = index;
    voice->instrument_selected = true;
    voice->sound.length_words = instrument.length_words;
    copy_state(voice, &instrument);
    if (instrument.wave.type == LmeWaveType_Sample) {
        voice->sound.start = lme_sample_at(&player->module, instrument.sample_offset);
        voice->sound.loop = lme_sample_at(&player->module, instrument.loop_offset);
        voice->sound.loop_words = instrument.loop_words;
    } else {
        voice->sound.start = voice->wave;
        voice->sound.loop = voice->wave;
        voice->sound.loop_words = instrument.length_words;
    }
}

// A note that is not legato resets the volume, vibrato, envelope, wave and retrigger from the selected instrument.

static void reset_sound(LmePlayer* player, LmeVoice* voice) {
    const LmeInstrument instrument = lme_instrument(&player->module, voice->instrument);
    copy_state(voice, &instrument);
}

// The arpeggio command: the note's own period (kept up to date every tick) and the periods of its notes.

static void set_arpeggio(LmeVoice* voice, const LmeArpeggio* command) {
    LmeArpeggioState* arpeggio = &voice->arpeggio;
    arpeggio->index = 0;
    arpeggio->length = command->count;
    arpeggio->counter = 1;
    arpeggio->reload = command->reload;
    for (uint32_t i = 0; i + 1 < command->count; i++) {
        uint16_t period = 0;
        lme_note_period(command->notes[i], voice->transpose, &period);
        arpeggio->slots[i + 1] = period;
    }
}

// The slide command: the target period, the number of ticks and the delay. The change per tick is worked out from the
// period the note starts at.

static void set_slide(LmeVoice* voice, const LmeSlide* command, uint16_t start_period) {
    uint16_t target = 0;
    lme_note_period(command->note, voice->transpose, &target);
    voice->slide = (LmeSlideState) { .delta = (int16_t)(((int32_t)target - (int32_t)start_period) / command->steps),
                                     .steps = (uint16_t)command->steps,
                                     .target = target,
                                     .delay = (int16_t)command->delay };
}

// A note: the new period; unless legato the sample starts again, DMA off now and on at the end of the tick.

static void start_note(LmeVoice* voice, uint32_t ch, uint16_t period, bool legato) {
    voice->period = period;
    voice->paula.period = period;
    voice->shown_period = period;
    voice->shown_instrument = voice->instrument_selected ? voice->instrument : -1;
    if (legato) {
        return;
    }
    start_sample(voice);
    write_volume(voice, (uint16_t)(ch * VOLUME_QUIRK_PER_VOICE));
}

static void apply_event(LmePlayer* player, uint32_t ch, const LmeEvent* event) {
    LmeVoice* voice = &player->voices[ch];
    voice->duration_ticks = event->duration;
    if (event->stop) {
        nuade_paula_channel_dma_off(&voice->paula);
        return;
    }
    if (event->has_instrument) {
        select_instrument(player, voice, event->instrument);
    }
    if (event->has_arpeggio) {
        set_arpeggio(voice, &event->arpeggio);
    }
    if (!event->legato) {
        reset_sound(player, voice);
    }
    uint16_t period = 0;
    lme_note_period(event->note, voice->transpose, &period);
    if (event->has_slide) {
        set_slide(voice, &event->slide, period);
    }
    start_note(voice, ch, period, event->legato);
}

// The voice's position list reached its end mark. When all four have, the eagleplayer calls SongEnd.

static void list_ended(LmePlayer* player, uint32_t ch, bool* looped) {
    player->voices[ch].list_pending = false;
    for (uint32_t i = 0; i < LME_VOICES; i++) {
        if (player->voices[i].list_pending) {
            return;
        }
    }
    for (uint32_t i = 0; i < LME_VOICES; i++) {
        player->voices[i].list_pending = true;
    }
    *looped = true;
}

// The next pattern of the voice's position list; the list starts again after its end. False when it cannot be followed.

static bool fetch_pattern(LmePlayer* player, uint32_t ch, bool* looped) {
    LmeVoice* voice = &player->voices[ch];
    LmePosition position;
    if (!lme_read_position(&player->module, &voice->list_pos, &position)) {
        return false;
    }
    if (position.end) {
        list_ended(player, ch, looped);
        voice->list_pos = voice->list_start;
        if (!lme_read_position(&player->module, &voice->list_pos, &position) || position.end) {
            return false;
        }
    }
    voice->transpose = position.transpose;
    voice->pattern_pos = position.pattern_start;
    voice->in_pattern = true;
    return true;
}

// The next event of a voice whose duration ran out. Every event clears the arpeggio first.

static bool next_event(LmePlayer* player, uint32_t ch, bool* looped) {
    LmeVoice* voice = &player->voices[ch];
    LmeEvent event;
    voice->arpeggio.length = 0;
    LmeRead read = LmeRead_PatternEnd;
    if (voice->in_pattern) {
        read = lme_read_event(&player->module, &voice->pattern_pos, false, &event);
    }
    if (read == LmeRead_PatternEnd) {
        if (!fetch_pattern(player, ch, looped)) {
            return false;
        }
        read = lme_read_event(&player->module, &voice->pattern_pos, true, &event);
    }
    if (read != LmeRead_Event) {
        return false;
    }
    apply_event(player, ch, &event);
    return true;
}

// A synth wave is two runs of bytes in the voice's buffer: `width - 1` of one value, then the rest of the other.

static void fill_wave(LmeVoice* voice) {
    const LmeWave* wave = &voice->sound.wave;
    const int32_t total = 2 * (int32_t)voice->sound.length_words;
    const int32_t width = wave->width;
    const int32_t first = width >= 2 ? width - 1 : 0;
    const int32_t rest = total - width;
    const int32_t second = (width < 2 || rest >= 0) ? rest + 1 : 0;
    int32_t at = 0;
    for (int32_t i = 0; i < first && at < LME_WAVE_BYTES; i++) {
        voice->wave[at++] = (int8_t)wave->value_a;
    }
    for (int32_t i = 0; i < second && at < LME_WAVE_BYTES; i++) {
        voice->wave[at++] = (int8_t)wave->value_b;
    }
}

static void run_waves(LmePlayer* player) {
    for (uint32_t ch = 0; ch < LME_VOICES; ch++) {
        LmeVoice* voice = &player->voices[ch];
        if (voice->sound.wave.type == LmeWaveType_Sample) {
            continue;
        }
        lme_wave_step(&voice->sound.wave);
        fill_wave(voice);
    }
}

LmeTickResult lme_tick(LmePlayer* player, NuadeVoices* voices) {
    LmeTickResult result = { 0 };
    if (player->ended) {
        result.ended = true;
        return result;
    }
    write_repeats(player);
    for (uint32_t ch = 0; ch < LME_VOICES; ch++) {
        player->voices[ch].dma_start = false;
    }
    run_slides(player);
    run_vibratos(player);
    run_arpeggios(player);
    run_envelopes(player);
    run_retriggers(player);
    for (uint32_t ch = 0; ch < LME_VOICES; ch++) {
        LmeVoice* voice = &player->voices[ch];
        voice->duration_ticks--;
        if (voice->duration_ticks == 0 && !next_event(player, ch, &result.looped)) {
            player->ended = true;
            result.ended = true;
            return result;
        }
    }
    run_waves(player);
    for (uint32_t ch = 0; ch < LME_VOICES; ch++) {
        if (player->voices[ch].dma_start) {
            nuade_paula_channel_dma_on(&player->voices[ch].paula);
        }
        nuade_paula_channel_sync(&player->voices[ch].paula, ch, voices);
    }
    return result;
}

uint32_t lme_grid_ticks(const LmePlayer* player) {
    return player->module.grid_ticks;
}

LmeVoiceState lme_voice_state(const LmePlayer* player, int voice) {
    const LmeVoice* v = &player->voices[voice];
    return (LmeVoiceState) { .period = v->shown_period, .instrument = v->shown_instrument };
}
