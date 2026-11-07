// SPDX-License-Identifier: MIT
// Maximum Effect: the replayer. Every voice plays a position list of patterns; a pattern event is an optional note,
// any number of commands and a duration. The eagleplayer's interrupt first runs the four voices, then switches the
// DMA of the voices that started a note on, then writes the repeat of every voice.

#include "mxe_internal.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>
#include <string.h>

// The timer word of a voice that reads its next event at the next tick (it counts down to a negative number).
#define TIMER_NEXT_EVENT 0x8000u
#define VOLUME_MAX 0x40
#define ARPEGGIO_START 0xFF // the index counts up before it is used
#define VIBRATO_PHASE_MASK 0x7E
#define VIBRATO_SHIFT_MASK 63 // register shift count
#define VIBRATO_SCALE 7
#define WORD_BITS 16

bool mxe_probe(const uint8_t* data, size_t size) {
    MxeModule module;
    return mxe_module_parse(&module, data, size) && mxe_module_validate(&module);
}

MxeCreateResult mxe_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    MxeCreateResult result = { .status = MxeStatus_InvalidData };
    MxePlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        result.status = MxeStatus_OutOfMemory;
        return result;
    }
    player->bytes = malloc(size);
    if (player->bytes == nullptr) {
        mxe_destroy(player);
        result.status = MxeStatus_OutOfMemory;
        return result;
    }
    memcpy(player->bytes, data, size);
    if (!mxe_module_parse(&player->module, player->bytes, size) || !mxe_module_validate(&player->module)) {
        mxe_destroy(player);
        return result;
    }
    const MxeLoad loaded = mxe_load_sample_file(path, io, &player->samples);
    if (loaded != MxeLoad_Loaded) {
        mxe_destroy(player);
        result.status = loaded == MxeLoad_Missing ? MxeStatus_MissingSamples : MxeStatus_InvalidData;
        return result;
    }
    if (!mxe_samples_check(&player->samples, &player->module)) {
        mxe_destroy(player);
        return result;
    }
    result.player = player;
    result.status = MxeStatus_Ok;
    return result;
}

void mxe_destroy(MxePlayer* player) {
    if (player == nullptr) {
        return;
    }
    mxe_samples_free(&player->samples);
    free(player->bytes);
    free(player);
}

uint32_t mxe_subsong_count(const MxePlayer* player) {
    return player->module.subsong_count;
}

// InitSound: the record's voices take the hardware voices in order. The eagleplayer then marks the hardware voices
// that run: it shifts one bit per assigned voice into the top of a byte and shifts the byte right by 4, so a record
// with fewer than four voices runs the last hardware voices, not the first. Hardware voices that run without a list
// of their own do nothing: the original reads address 0 as their list, but only after 32769 ticks (the timer word of an
// unused voice counts down from 0 to 0x7FFF), 655 s into the song.

bool mxe_start(MxePlayer* player, uint32_t subsong, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || subsong >= player->module.subsong_count) {
        return false;
    }
    const MxeRecord record = mxe_record(&player->module, subsong);
    player->one_shot = record.one_shot;
    memset(player->voices, 0, sizeof(player->voices));
    uint32_t assigned = 0;
    for (uint32_t i = 0; i < MXE_VOICES; i++) {
        player->song_end_initial[i] = record.voice_used[i];
        if (!record.voice_used[i]) {
            continue;
        }
        MxeVoice* voice = &player->voices[assigned++];
        voice->list_start = record.list[i];
        voice->timer = TIMER_NEXT_EVENT;
    }
    for (uint32_t hw = 0; hw < MXE_VOICES; hw++) {
        MxeVoice* voice = &player->voices[hw];
        voice->shown_instrument = -1;
        voice->active = hw >= MXE_VOICES - assigned && hw < assigned;
        voice->song_end_pending = player->song_end_initial[hw];
        nuade_voice_stop(voices, hw);
    }
    return true;
}

// The sample address Paula uses (bit 0 of AUDxLC does not exist); null past the end of the sample file.

static const int8_t* sample_at(const MxePlayer* player, uint32_t offset) {
    return offset <= player->samples.size ? (const int8_t*)(player->samples.bytes + (offset & ~1u)) : nullptr;
}

// A voice reached the end of its list and starts again. SongEnd is called when every voice that had a list has done so
// since the last call.

static void note_list_restart(MxePlayer* player, uint32_t hw, bool* looped) {
    player->voices[hw].song_end_pending = false;
    for (uint32_t i = 0; i < MXE_VOICES; i++) {
        if (player->voices[i].song_end_pending) {
            return;
        }
    }
    for (uint32_t i = 0; i < MXE_VOICES; i++) {
        player->voices[i].song_end_pending = player->song_end_initial[i];
    }
    *looped = true;
}

// The next list entry: a new pattern, or the end mark, which stops a one-shot voice and sends the others back to the
// entry it names. False when the voice stopped.

static bool next_pattern(MxePlayer* player, uint32_t hw, bool* looped) {
    MxeVoice* voice = &player->voices[hw];
    MxeListEntry entry = mxe_list_entry(&player->module, voice->list_start + voice->position);
    if (entry.end) {
        if (player->one_shot) {
            voice->active = false;
            return false;
        }
        voice->position = entry.value;
        entry = mxe_list_entry(&player->module, voice->list_start + voice->position);
        note_list_restart(player, hw, looped);
    }
    voice->pattern_pos = entry.pattern;
    voice->events_left = entry.value;
    voice->position += MXE_LIST_ENTRY_BYTES;
    return true;
}

// The volume slide that the vibrato, the slide and the portamento end with: the volume stays within 0 to 64.

static void slide_volume(MxeVoice* voice, int8_t step) {
    if (step == 0) {
        return;
    }
    const int16_t volume = (int16_t)(voice->volume + step);
    voice->volume = (uint16_t)(volume < 0 ? 0 : (volume > VOLUME_MAX ? VOLUME_MAX : volume));
}

// The sine, shifted left by a register shift count (a word shifted by 16 or more is 0), then scaled down.

static void run_vibrato(MxeVoice* voice) {
    MxeEffectParams* params = &voice->params;
    const int16_t sine = mxe_sine(params->vibrato.phase & VIBRATO_PHASE_MASK);
    const uint32_t shift = params->vibrato.depth_shift & VIBRATO_SHIFT_MASK;
    const uint16_t shifted = shift >= WORD_BITS ? 0 : (uint16_t)((uint32_t)(uint16_t)sine << shift);
    const int16_t swing = (int16_t)((int16_t)shifted >> VIBRATO_SCALE);
    voice->period = (uint16_t)(voice->base_period + swing);
    params->vibrato.phase = (uint8_t)((params->vibrato.phase & VIBRATO_PHASE_MASK) + params->vibrato.speed);
    slide_volume(voice, params->vibrato.volume_slide);
}

static void run_portamento(MxeVoice* voice) {
    const MxeEffectParams* params = &voice->params;
    const int16_t target = (int16_t)mxe_period(params->portamento.target_offset);
    int16_t period = (int16_t)(voice->period + params->portamento.speed);
    if (params->portamento.speed >= 0 ? period > target : period < target) {
        period = target;
    }
    voice->period = (uint16_t)period;
    slide_volume(voice, params->portamento.volume_slide);
}

static void run_arpeggio(MxeVoice* voice) {
    MxeEffectParams* params = &voice->params;
    uint8_t index = (uint8_t)(params->arpeggio.index + 1);
    if (index >= MXE_ARPEGGIO_NOTES) {
        index = 0;
    }
    params->arpeggio.index = index;
    voice->period = mxe_period(params->arpeggio.note_offset[index]);
}

static void run_effect(MxeVoice* voice) {
    switch (voice->effect) {
        case MxeEffect_None:
            break;
        case MxeEffect_Vibrato:
            run_vibrato(voice);
            break;
        case MxeEffect_VolumeSlide:
            slide_volume(voice, voice->params.slide.volume_slide);
            break;
        case MxeEffect_Portamento:
            run_portamento(voice);
            break;
        case MxeEffect_Arpeggio:
            run_arpeggio(voice);
            break;
    }
}

// A note: the volume (unless the note keeps it), the period, and the sample, which restarts the channel. The repeat
// registers are written after the DMA is back on (write_repeats).

static void start_note(MxePlayer* player, MxeVoice* voice, const MxeItem* note) {
    const MxeInstrument instrument = mxe_instrument(&player->samples, note->instrument);
    if (!note->keep_volume) {
        voice->volume = instrument.volume;
    }
    voice->period = mxe_period(note->note_offset);
    nuade_paula_channel_dma_off(&voice->paula);
    voice->paula.lc = sample_at(player, instrument.start);
    voice->paula.len_words = instrument.length_words;
    voice->repeat_lc = sample_at(player, instrument.loop_start);
    voice->repeat_words = instrument.loop_words;
    voice->dma_start = true;
    voice->shown_period = voice->period;
    voice->shown_instrument = note->instrument;
}

// A command. The vibrato, the portamento and the arpeggio keep running in the following ticks of the event; the vibrato
// and the portamento also act at once.

static void run_command(MxeVoice* voice, const MxeItem* item) {
    MxeEffectParams* params = &voice->params;
    switch (item->command) {
        case MxeCommand_Nop:
        case MxeCommand_FilterOff: // the power LED line of the CIA: the filter is not modelled
        case MxeCommand_FilterOn:
        case MxeCommand_Count:
            break;
        case MxeCommand_SetVolume:
            voice->volume = item->param;
            break;
        case MxeCommand_VibratoRestart:
        case MxeCommand_Vibrato:
            if (item->command == MxeCommand_VibratoRestart) {
                params->vibrato.phase = 0;
            }
            voice->effect = MxeEffect_Vibrato;
            params->vibrato.speed = item->param;
            params->vibrato.depth_shift = item->extra[0];
            params->vibrato.volume_slide = (int8_t)item->extra[1];
            voice->base_period = voice->period;
            run_vibrato(voice);
            break;
        case MxeCommand_VolumeSlide:
            voice->effect = MxeEffect_VolumeSlide;
            params->slide.volume_slide = (int8_t)item->param;
            break;
        case MxeCommand_Portamento:
            voice->effect = MxeEffect_Portamento;
            params->portamento.target_offset = item->param;
            params->portamento.speed = (int8_t)item->extra[0];
            params->portamento.volume_slide = (int8_t)item->extra[1];
            run_portamento(voice);
            break;
        case MxeCommand_Arpeggio:
            voice->effect = MxeEffect_Arpeggio;
            params->arpeggio.note_offset[0] = item->param;
            params->arpeggio.note_offset[1] = item->extra[0];
            params->arpeggio.note_offset[2] = item->extra[1];
            params->arpeggio.index = ARPEGGIO_START;
            break;
    }
}

// One event of the current pattern: up to the duration, which ends it. False when the pattern cannot be read (the
// walk at open rules that out).

static bool run_event(MxePlayer* player, MxeVoice* voice) {
    bool first = true;
    for (;;) {
        MxeItem item;
        if (mxe_read_item(&player->module, &voice->pattern_pos, first, &item) != MxeRead_Item) {
            return false;
        }
        first = false;
        switch (item.kind) {
            case MxeItem_Note:
                start_note(player, voice, &item);
                break;
            case MxeItem_Command:
                run_command(voice, &item);
                break;
            case MxeItem_Duration:
                voice->timer = item.duration;
                voice->events_left--;
                return true;
        }
    }
}

// The registers a voice that ran writes: the period and the volume (the eagleplayer's master volume and balance are
// the identity at full volume).

static void write_registers(MxeVoice* voice) {
    voice->paula.period = voice->period;
    voice->paula.volume = (uint8_t)voice->volume;
}

// One voice in one tick: while an event lasts only its effect runs; when it is over the next event is read.

static void run_voice(MxePlayer* player, uint32_t hw, bool* looped) {
    MxeVoice* voice = &player->voices[hw];
    voice->timer--;
    if ((int16_t)voice->timer < 0) {
        if (voice->effect != MxeEffect_None) {
            run_effect(voice);
            write_registers(voice);
        }
        return;
    }
    voice->effect = MxeEffect_None;
    if (voice->events_left == 0 && !next_pattern(player, hw, looped)) {
        return;
    }
    if (!run_event(player, voice)) {
        voice->active = false;
        return;
    }
    write_registers(voice);
}

// The end of the interrupt: DMA on for the voices that started a note (they latch the sample), then every voice gets
// its repeat.

static void write_repeats(MxePlayer* player) {
    for (uint32_t hw = 0; hw < MXE_VOICES; hw++) {
        MxeVoice* voice = &player->voices[hw];
        if (voice->dma_start) {
            nuade_paula_channel_dma_on(&voice->paula);
        }
        voice->paula.lc = voice->repeat_lc;
        voice->paula.len_words = voice->repeat_words;
    }
}

MxeTickResult mxe_tick(MxePlayer* player, NuadeVoices* voices) {
    MxeTickResult result = { 0 };
    for (uint32_t hw = 0; hw < MXE_VOICES; hw++) {
        player->voices[hw].dma_start = false;
    }
    for (uint32_t hw = 0; hw < MXE_VOICES; hw++) {
        if (player->voices[hw].active) {
            run_voice(player, hw, &result.looped);
        }
    }
    write_repeats(player);
    for (uint32_t hw = 0; hw < MXE_VOICES; hw++) {
        nuade_paula_channel_sync(&player->voices[hw].paula, hw, voices);
    }
    return result;
}

uint32_t mxe_grid_ticks(const MxePlayer* player) {
    return player->module.grid_ticks;
}

MxeVoiceState mxe_voice_state(const MxePlayer* player, int voice) {
    const MxeVoice* v = &player->voices[voice];
    return (MxeVoiceState) { .period = v->shown_period, .instrument = v->shown_instrument };
}
