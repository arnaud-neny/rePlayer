// SPDX-License-Identifier: MIT
// Scott Johnston player: the replayer of the eagleplayers "ScottJohnston" and "SoundPlayer", the part that their
// Interrupt calls once per CIA timer tick.
//
// Four voices, each with its own track of the tune. Every tick the replayer first rewrites each voice's repeat
// registers and steps its volume fade, then starts the notes the last row asked for (the registers, then DMA on). Every
// sixth tick it moves the tracks one row on, reads each voice's cell, switches the DMA off for the voices that got a
// note and runs the commands. The eagleplayer's calls into DeliTracker (poke address, length, period, volume, DMA mask)
// are the Paula registers.

#include "nuade_paula_channel.h"
#include "sj_internal.h"
#include <nuade/players/scottjohnston.h>
#include <stdlib.h>
#include <string.h>

#define TICKS_PER_ROW SJ_TICKS_PER_ROW
#define VOLUME_MAX 0x3F

// The replayer's silent word, the first word of the sample arena.
#define SILENT_WORDS 1

// Paula

// Paula ignores bit 0 of AUDxLC; the arena starts on an even address.
static const int8_t* arena_pointer(const SjPlayer* player, uint32_t offset) {
    return (const int8_t*)(player->samples.arena + (offset & ~1u));
}

static void poke_repeat(const SjPlayer* player, SjVoice* voice, uint32_t start, uint16_t length_words) {
    voice->paula.lc = arena_pointer(player, start);
    voice->paula.len_words = length_words;
}

// Every tick: the repeat registers, the voice's own sample or the silent word.

static void write_repeat(const SjPlayer* player, SjVoice* voice) {
    if (voice->hold) {
        return;
    }
    if (voice->loop_length_words != 0) {
        poke_repeat(player, voice, voice->loop_start, voice->loop_length_words);
    } else {
        poke_repeat(player, voice, 0, SILENT_WORDS);
    }
}

static void step_fade(SjVoice* voice) {
    if (voice->fade_speed == 0) {
        return;
    }
    voice->fade_counter--;
    if (voice->fade_counter != 0) {
        return;
    }
    voice->fade_counter = voice->fade_speed;
    if (voice->fade_down) {
        if (voice->volume == 0) {
            voice->fade_speed = 0;
            return;
        }
        voice->volume--;
    } else {
        if (voice->volume == VOLUME_MAX) {
            voice->fade_speed = 0;
            return;
        }
        voice->volume++;
    }
    voice->paula.volume = (uint8_t)voice->volume;
}

// A note the last row asked for starts one tick later: address, length, volume, period, DMA on.

static void start_note(const SjPlayer* player, SjVoice* voice) {
    if (voice->sample_length_words == 0) {
        return;
    }
    voice->paula.lc = arena_pointer(player, voice->sample_start);
    voice->paula.len_words = voice->sample_length_words;
    voice->paula.volume = (uint8_t)voice->volume;
    voice->paula.period = voice->period;
    nuade_paula_channel_dma_on(&voice->paula);
    voice->sample_length_words = 0;
}

// The row's note: DMA off, the period, and the instrument's record copied into the voice.

static void trigger_note(SjPlayer* player, SjVoice* voice) {
    nuade_paula_channel_dma_off(&voice->paula);
    voice->period = sj_period_for_note(voice->cell.note);
    // Instrument 0 reads an empty record.
    SjSampleSlot slot = { 0 };
    if (voice->cell.instrument != 0) {
        slot = player->samples.slots[voice->cell.instrument - 1u];
    }
    voice->sample_start = slot.start;
    voice->sample_length_words = slot.length_words;
    voice->loop_start = slot.loop_start;
    voice->loop_length_words = slot.loop_length_words;
    voice->shown_period = voice->period;
    voice->shown_instrument = voice->cell.instrument;
    voice->cell.note = 0;
}

// The voice a "stop channel" command switches off. In the ScottJohnston replayer voice 0 hits the DMA bit of voice 1
// (it passes $12 for it, where the others pass their own bit).

static uint32_t stop_target(const SjPlayer* player, uint32_t voice_index) {
    return voice_index == 0 && !player->module.variant->stop_hits_own_voice ? 1 : voice_index;
}

static void run_command(SjPlayer* player, uint32_t voice_index, SjCommand command) {
    SjVoice* voice = &player->voices[voice_index];
    switch (command.kind) {
        case SjCommand_SetVolume:
            voice->volume = command.param;
            voice->paula.volume = command.param;
            break;
        case SjCommand_StopChannel:
            nuade_paula_channel_dma_off(&player->voices[stop_target(player, voice_index)].paula);
            break;
        case SjCommand_FadeUp:
        case SjCommand_FadeDown:
            voice->fade_down = command.kind == SjCommand_FadeDown;
            voice->fade_speed = command.param;
            voice->fade_counter = command.param;
            break;
        case SjCommand_HoldOn:
            voice->hold = true;
            break;
        case SjCommand_HoldOff:
            voice->hold = false;
            break;
        case SjCommand_Restart:
            sj_track_apply(&player->tracks[voice_index], command);
            player->looped = player->module.variant->restart_ends_song;
            break;
        default:
            sj_track_apply(&player->tracks[voice_index], command);
            break;
    }
}

// The tick that moves the tracks: the walk of every track, the notes, the commands.

static void run_row(SjPlayer* player) {
    for (uint32_t i = 0; i < SJ_VOICES; i++) {
        if (!sj_module_voice_is_armed(&player->module, i)) {
            continue;
        }
        SjCell cell = { 0 };
        bool read = false;
        if (!sj_track_step(&player->tracks[i], &player->module, i, &cell, &read)
            || (read && !sj_cell_is_modelled(&cell))) {
            player->broken = true;
            return;
        }
        if (read) {
            player->voices[i].cell = cell;
        }
    }
    for (uint32_t i = 0; i < SJ_VOICES; i++) {
        if (player->voices[i].cell.note != 0) {
            trigger_note(player, &player->voices[i]);
        }
    }
    for (uint32_t i = 0; i < SJ_VOICES; i++) {
        const uint8_t byte = player->voices[i].cell.command;
        player->voices[i].cell.command = 0;
        run_command(player, i, sj_decode_command(byte));
    }
}

SjTickResult sj_tick(SjPlayer* player, NuadeVoices* voices) {
    player->looped = false;
    if (!player->broken) {
        for (uint32_t i = 0; i < SJ_VOICES; i++) {
            write_repeat(player, &player->voices[i]);
            step_fade(&player->voices[i]);
        }
        for (uint32_t i = 0; i < SJ_VOICES; i++) {
            start_note(player, &player->voices[i]);
        }
        if (player->row_countdown != 0) {
            player->row_countdown--;
        } else {
            player->row_countdown = TICKS_PER_ROW - 1;
        }
        if (player->row_countdown == 0) {
            run_row(player);
        }
    }
    for (uint32_t i = 0; i < SJ_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, i, voices);
    }
    return (SjTickResult) { .length = player->tick_length, .looped = player->looped, .ended = player->broken };
}

// The state InitPlayer and InitSound leave: every voice has the silent word as its repeat sample (and, in the
// ScottJohnston replayer, DMA on), the tracks stand before the first row, and the countdown of the row stands where the
// init leaves it.

bool sj_start(SjPlayer* player, NuadeVoices* voices) {
    player->broken = false;
    player->row_countdown = player->module.variant->init_runs_tick ? TICKS_PER_ROW - 1 : 0;
    player->looped = false;
    player->tick_length = (uint32_t)player->module.timer + 1u; // a CIA timer counts down to 0 and one more
    for (uint32_t i = 0; i < SJ_VOICES; i++) {
        sj_track_init(&player->tracks[i]);
        SjVoice* voice = &player->voices[i];
        memset(voice, 0, sizeof(*voice));
        voice->shown_instrument = -1;
        poke_repeat(player, voice, 0, SILENT_WORDS);
        if (player->module.variant->init_dma_on) {
            nuade_paula_channel_dma_on(&voice->paula);
        }
        if (voices != nullptr) {
            nuade_voice_stop(voices, i);
            nuade_voice_set_pan(voices, i, (i == 0 || i == 3) ? -1.0f : 1.0f);
            nuade_paula_channel_sync(&voice->paula, i, voices);
        }
    }
    return true;
}

SjCreateResult sj_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    SjCreateResult result = { .status = SjStatus_InvalidData };
    SjPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        result.status = SjStatus_OutOfMemory;
        return result;
    }
    uint8_t* bytes = malloc(size);
    if (bytes == nullptr) {
        free(player);
        result.status = SjStatus_OutOfMemory;
        return result;
    }
    memcpy(bytes, data, size);
    player->module.data = bytes; // sj_destroy frees it
    SjLoadStatus loaded = SjLoad_Invalid;
    SjFormat format;
    if (sj_format_from_path(path, &format) && sj_module_parse(&player->module, format, bytes, size)
        && sj_tune_is_closed(&player->module)) {
        loaded = sj_samples_load(&player->samples, &player->module, path, io);
    }
    if (loaded != SjLoad_Ok) {
        sj_destroy(player);
        result.status = loaded == SjLoad_Missing       ? SjStatus_MissingInstruments
                        : loaded == SjLoad_OutOfMemory ? SjStatus_OutOfMemory
                                                       : SjStatus_InvalidData;
        return result;
    }
    result.player = player;
    result.status = SjStatus_Ok;
    return result;
}

void sj_destroy(SjPlayer* player) {
    if (player == nullptr) {
        return;
    }
    sj_samples_free(&player->samples);
    free((void*)player->module.data);
    free(player);
}

const char* sj_format_name(const SjPlayer* player) {
    return player->module.variant->name;
}

uint32_t sj_ticks_per_row(void) {
    return TICKS_PER_ROW;
}

SjVoiceState sj_voice_state(const SjPlayer* player, uint32_t voice) {
    if (voice >= SJ_VOICES) {
        return (SjVoiceState) { .period = 0, .instrument = -1 };
    }
    return (SjVoiceState) { .period = player->voices[voice].shown_period,
                            .instrument = player->voices[voice].shown_instrument };
}
