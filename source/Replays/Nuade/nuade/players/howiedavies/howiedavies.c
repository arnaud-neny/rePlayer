// SPDX-License-Identifier: MIT
// Howie Davies player: the replayer of the module, one function per routine, as the eagleplayer's patches leave it. A
// tick first starts DMA on the voices (the routine at the header's second entry), then runs the play routine: every
// `speed` ticks the sequencer steps all voices, then the volume envelope of each voice.
//
// What the eagleplayer's patches change in the replayer: AUDxVOL goes through its volume filter (bits 0..6, full volume
// at the default balance), the DMA wait loop becomes a wait of 8 or 9 raster lines (the repeat registers are written
// after it), a sample address of 0 becomes the 4 zero bytes of the eagleplayer, and the position list commands that
// restart a voice clear its song end flag. The song ends when all four flags are clear.

#include <nuade/players/howiedavies.h>

#include "hd_internal.h"
#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>
#include <string.h>

#define HD_DEFAULT_SPEED 6u
#define HD_MASTER_VOLUME 0x40u
#define HD_ENVELOPE_END 0xFFFFu // the envelope tick count after the last step
#define HD_ENVELOPE_END_BYTE 0xFFu
#define HD_NOTE_LIMIT 0x80u // bytes from here on are commands
#define HD_SILENCE_WORDS 1u
#define HD_STEP_GUARD 4096u // commands one voice step may run before the data counts as a loop
#define HD_DRY_RUN_TICKS 90000u

// The volume a voice writes: the envelope's, held down by the voice's limit and in one version by the global limit.

static uint16_t limited_volume(const HdModule* module, const HdVoice* voice) {
    uint16_t volume = voice->volume;
    if (module->driver->volume_limit && volume >= voice->max_volume) {
        volume = voice->max_volume;
    }
    if (module->driver->master_limit && volume >= HD_MASTER_VOLUME) {
        volume = HD_MASTER_VOLUME;
    }
    return volume;
}

// A byte of a stream; the cursor moves on. Past the hunk the data is out of the model.

static uint8_t next_byte(HdPlayer* player, uint32_t* cursor) {
    uint8_t value = 0;
    if (!hd_byte(&player->module, *cursor, &value)) {
        player->fault = true;
    }
    (*cursor)++;
    return value;
}

// The init routine for one voice: record fields and registers.

static bool init_voice(const HdModule* module, HdVoice* voice, uint32_t subsong, uint32_t index) {
    *voice = (HdVoice) { .sample = hd_silence(),
                         .sample_words = HD_SILENCE_WORDS,
                         .repeat = hd_silence(),
                         .repeat_words = HD_SILENCE_WORDS,
                         .volume = HD_MASTER_VOLUME,
                         .max_volume = HD_MASTER_VOLUME,
                         .envelope_start = HD_NO_POINTER,
                         .envelope_pos = module->default_envelope,
                         .envelope_ticks = 1,
                         .pattern_start = module->default_stream,
                         .pattern_pos = module->default_stream,
                         .instrument = -1 };
    voice->paula.lc = hd_silence();
    voice->paula.len_words = HD_SILENCE_WORDS;
    if (!hd_voice_list(module, subsong, index, &voice->list_start)) {
        return false;
    }
    voice->list_pos = voice->list_start;
    return true;
}

bool hd_start(HdPlayer* player, uint32_t subsong, NuadeVoices* voices) {
    if (player == nullptr || subsong >= player->module.subsong_count) {
        return false;
    }
    player->speed = HD_DEFAULT_SPEED;
    player->speed_ticks = HD_DEFAULT_SPEED;
    player->fault = false;
    for (uint32_t i = 0; i < HD_VOICES; i++) {
        player->voice_on[i] = true;
        player->song_flag[i] = true;
        if (!init_voice(&player->module, &player->voices[i], subsong, i)) {
            return false;
        }
        if (voices != nullptr) {
            nuade_voice_stop(voices, i);
        }
    }
    return true;
}

// The DMA routine: DMA on for every voice of the mask, then, after the raster wait, the repeat of each of them.

static void dma_routine(HdPlayer* player) {
    for (uint32_t i = 0; i < HD_VOICES; i++) {
        if (player->voice_on[i]) {
            nuade_paula_channel_dma_on(&player->voices[i].paula);
        }
    }
    for (uint32_t i = 0; i < HD_VOICES; i++) {
        HdVoice* voice = &player->voices[i];
        if (player->voice_on[i]) {
            const bool from_instrument = player->module.driver->loop_from_instrument;
            voice->paula.lc = from_instrument ? voice->repeat : hd_silence();
            voice->paula.len_words = from_instrument ? voice->repeat_words : HD_SILENCE_WORDS;
        }
    }
}

// The next envelope step: the slope per tick and for how many ticks, 0xFF ends the envelope.

static void envelope_step(HdPlayer* player, HdVoice* voice) {
    uint32_t cursor = voice->envelope_pos;
    if (cursor == HD_NO_POINTER) {
        player->fault = true;
        return;
    }
    voice->envelope_slope = (int8_t)next_byte(player, &cursor);
    const uint8_t ticks = next_byte(player, &cursor);
    voice->envelope_ticks = ticks == HD_ENVELOPE_END_BYTE ? HD_ENVELOPE_END : ticks;
    voice->envelope_pos = cursor;
}

// The envelope of one voice, once per tick.

static void envelope_routine(HdPlayer* player, HdVoice* voice) {
    if (player->module.driver->envelope == HdEnvelope_WriteThenStep) {
        if (voice->envelope_ticks == HD_ENVELOPE_END) {
            return;
        }
        voice->volume = (uint16_t)(voice->volume + voice->envelope_slope);
        voice->paula.volume = (uint8_t)limited_volume(&player->module, voice);
        voice->envelope_ticks--;
        if (voice->envelope_ticks == 0) {
            envelope_step(player, voice);
        }
        return;
    }
    if (voice->envelope_ticks != HD_ENVELOPE_END) {
        voice->volume = (uint16_t)(voice->volume + voice->envelope_slope);
        voice->envelope_ticks--;
        if (voice->envelope_ticks == 0) {
            envelope_step(player, voice);
            if (voice->envelope_ticks == HD_ENVELOPE_END) {
                return;
            }
        }
    }
    voice->paula.volume = (uint8_t)limited_volume(&player->module, voice);
}

// Clears the end flag of a voice that restarts its position list. When the fourth flag falls the eagleplayer calls
// SongEnd and sets all flags again.

static void clear_song_flag(HdPlayer* player, uint32_t index, bool* looped) {
    player->song_flag[index] = false;
    for (uint32_t i = 0; i < HD_VOICES; i++) {
        if (player->song_flag[i]) {
            return;
        }
    }
    for (uint32_t i = 0; i < HD_VOICES; i++) {
        player->song_flag[i] = true;
    }
    *looped = true;
}

// A note byte: the period, the rest to the next note and the registers (DMA off, sample, period, volume). The DMA
// routine of the next tick starts the sound.

static void start_note(HdPlayer* player, HdVoice* voice, uint8_t note, uint32_t* cursor) {
    uint16_t period;
    if (!hd_period(&player->module, note, &period)) {
        player->fault = true;
        return;
    }
    voice->rest_ticks = (int8_t)next_byte(player, cursor);
    voice->pattern_pos = *cursor;
    nuade_paula_channel_dma_off(&voice->paula);
    voice->envelope_pos = voice->envelope_start;
    voice->volume = voice->base_volume;
    voice->envelope_ticks = 1;
    voice->envelope_slope = 0;
    voice->paula.lc = voice->sample;
    voice->paula.period = period;
    voice->paula.len_words = voice->sample_words;
    voice->paula.volume = (uint8_t)limited_volume(&player->module, voice);
}

// Where the step of a voice goes on after a command.

typedef enum HdFlow {
    HdFlow_Pattern, // read the next pattern byte
    HdFlow_List,    // read the next position list byte
    HdFlow_Return,  // the step ends
} HdFlow;

// The instrument command: loads the sample record whose number follows.

static void load_instrument(HdPlayer* player, HdVoice* voice, uint32_t* cursor) {
    const uint8_t number = next_byte(player, cursor);
    HdSample sample;
    uint32_t envelope;
    if (!hd_sample(&player->module, number, &sample) || !hd_envelope(&player->module, sample.envelope, &envelope)) {
        player->fault = true;
        return;
    }
    voice->sample = sample.start;
    voice->sample_words = sample.words;
    voice->repeat = sample.loop;
    voice->repeat_words = sample.loop_words;
    voice->volume = sample.volume;
    voice->base_volume = sample.volume;
    voice->envelope_start = envelope;
    voice->envelope_pos = envelope;
    voice->instrument = number;
    voice->pattern_pos = *cursor;
}

// The position list command that goes back: its operand (and a byte more in two versions) before the operand.

static void list_loop_back(HdPlayer* player, HdVoice* voice, uint32_t* cursor) {
    const uint32_t operand = *cursor;
    const uint8_t back = next_byte(player, cursor);
    const uint32_t distance = (uint32_t)back + player->module.driver->list_loop_extra;
    if (distance > operand) {
        player->fault = true;
        return;
    }
    voice->list_pos = operand - distance;
}

// One command of a pattern or of the position list. `cursor` is behind the command byte.

static HdFlow run_command(HdPlayer* player, HdVoice* voice, uint32_t index, HdHandler handler, uint32_t* cursor,
                          bool* looped) {
    switch (handler) {
        case HdHandler_Repeat:
            // The count is a signed word: below 0 the pattern is finished
            voice->repeats_left--;
            if (voice->repeats_left >= 0) {
                voice->pattern_pos = voice->pattern_start;
                return HdFlow_Pattern;
            }
            return HdFlow_List;
        case HdHandler_Instrument:
            load_instrument(player, voice, cursor);
            return HdFlow_Pattern;
        case HdHandler_Speed:
            player->speed = next_byte(player, cursor);
            voice->pattern_pos = *cursor;
            return HdFlow_Pattern;
        case HdHandler_Delay:
            voice->rest_ticks = (int8_t)next_byte(player, cursor);
            voice->pattern_pos = *cursor;
            return HdFlow_Return;
        case HdHandler_MaxVolume:
            voice->max_volume = next_byte(player, cursor);
            voice->pattern_pos = *cursor;
            return HdFlow_Pattern;
        case HdHandler_ListRestart:
            voice->list_pos = voice->list_start;
            clear_song_flag(player, index, looped);
            return HdFlow_List;
        case HdHandler_ListLoopBack:
            clear_song_flag(player, index, looped);
            list_loop_back(player, voice, cursor);
            return HdFlow_List;
        case HdHandler_ListRestartOff:
            voice->list_pos = voice->list_start;
            clear_song_flag(player, index, looped);
            player->voice_on[index] = false;
            return HdFlow_List;
        case HdHandler_Return:
            return HdFlow_Return;
    }
    return HdFlow_Return;
}

// The next position list byte: a number starts that pattern for count + 1 plays, a command byte runs a command.

static HdFlow list_entry(HdPlayer* player, HdVoice* voice, uint32_t index, bool* looped) {
    uint32_t cursor = voice->list_pos;
    const uint8_t entry = next_byte(player, &cursor);
    if (entry < HD_NOTE_LIMIT) {
        uint32_t pattern;
        if (!hd_pattern(&player->module, entry, &pattern)) {
            player->fault = true;
            return HdFlow_Return;
        }
        voice->pattern_pos = pattern;
        voice->pattern_start = pattern;
        voice->repeats_left = next_byte(player, &cursor);
        voice->list_pos = cursor;
        return HdFlow_Pattern;
    }
    HdHandler handler;
    const HdModule* module = &player->module;
    if (!hd_handler(module, module->init + module->driver->list_command_table, entry, &handler)) {
        player->fault = true;
        return HdFlow_Return;
    }
    return run_command(player, voice, index, handler, &cursor, looped);
}

// The sequencer step of one voice: counts the rest down and, when it is over, reads pattern bytes until a note.

static void sequencer_voice(HdPlayer* player, uint32_t index, bool* looped) {
    HdVoice* voice = &player->voices[index];
    voice->rest_ticks = (int8_t)(voice->rest_ticks - 1);
    if (voice->rest_ticks >= 0) {
        return;
    }
    HdFlow flow = HdFlow_Pattern;
    for (uint32_t guard = 0; guard < HD_STEP_GUARD && !player->fault; guard++) {
        if (flow == HdFlow_Return) {
            return;
        }
        if (flow == HdFlow_List) {
            flow = list_entry(player, voice, index, looped);
            continue;
        }
        uint32_t cursor = voice->pattern_pos;
        const uint8_t value = next_byte(player, &cursor);
        if (value < HD_NOTE_LIMIT) {
            start_note(player, voice, value, &cursor);
            return;
        }
        HdHandler handler;
        const HdModule* module = &player->module;
        if (!hd_handler(module, module->init + module->driver->command_table, value, &handler)) {
            player->fault = true;
            return;
        }
        flow = run_command(player, voice, index, handler, &cursor, looped);
    }
    player->fault = true;
}

// The play routine: the sequencer step every `speed` ticks, then the envelopes. The mask the voices run with is the
// one at the start of each loop.

static void play_routine(HdPlayer* player, bool* looped) {
    uint16_t ticks = (uint16_t)(player->speed_ticks + 1);
    if (ticks >= player->speed) {
        bool mask[HD_VOICES];
        memcpy(mask, player->voice_on, sizeof(mask));
        for (uint32_t i = 0; i < HD_VOICES; i++) {
            if (mask[i]) {
                sequencer_voice(player, i, looped);
            }
        }
        ticks = 0;
    }
    player->speed_ticks = ticks;
    for (uint32_t i = 0; i < HD_VOICES; i++) {
        if (player->voice_on[i]) {
            envelope_routine(player, &player->voices[i]);
        }
    }
}

static HdTickResult run_tick(HdPlayer* player) {
    HdTickResult result = { 0 };
    if (player->fault) {
        result.ended = true;
        return result;
    }
    dma_routine(player);
    play_routine(player, &result.looped);
    bool any_voice = false;
    for (uint32_t i = 0; i < HD_VOICES; i++) {
        any_voice = any_voice || player->voice_on[i];
    }
    result.ended = player->fault || !any_voice;
    return result;
}

HdTickResult hd_tick(HdPlayer* player, NuadeVoices* voices) {
    const HdTickResult result = run_tick(player);
    for (uint32_t i = 0; i < HD_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, i, voices);
    }
    return result;
}

// Plays every subsong to its first end (or HD_DRY_RUN_TICKS ticks) without a mixer: the data it reaches must stay
// inside what the port models.

static bool dry_run(HdPlayer* player) {
    for (uint32_t subsong = 0; subsong < player->module.subsong_count; subsong++) {
        if (!hd_start(player, subsong, nullptr)) {
            return false;
        }
        for (uint32_t tick = 0; tick < HD_DRY_RUN_TICKS; tick++) {
            const HdTickResult result = run_tick(player);
            if (player->fault) {
                return false;
            }
            if (result.looped || result.ended) {
                break;
            }
        }
    }
    return true;
}

HdPlayer* hd_create(const uint8_t* data, size_t size) {
    HdPlayer* player = calloc(1, sizeof(*player));
    uint8_t* bytes = malloc(size);
    if (player == nullptr || bytes == nullptr) {
        free(player);
        free(bytes);
        return nullptr;
    }
    memcpy(bytes, data, size);
    player->bytes = bytes;
    if (!hd_module_parse(&player->module, bytes, size) || !dry_run(player)) {
        hd_destroy(player);
        return nullptr;
    }
    return player;
}

void hd_destroy(HdPlayer* player) {
    if (player != nullptr) {
        hd_module_free(&player->module);
        free(player->bytes);
        free(player);
    }
}

bool hd_probe(const uint8_t* data, size_t size) {
    HdPlayer* player = hd_create(data, size);
    hd_destroy(player);
    return player != nullptr;
}

uint32_t hd_subsong_count(const HdPlayer* player) {
    return player->module.subsong_count;
}

void hd_text(const HdPlayer* player, HdText which, char* out, size_t cap) {
    if (cap == 0) {
        return;
    }
    const HdModule* module = &player->module;
    size_t length = 0;
    uint8_t value;
    for (uint32_t at = module->texts[which]; length + 1 < cap && hd_byte(module, at, &value) && value != 0; at++) {
        out[length++] = (char)value;
    }
    out[length] = '\0';
}

uint32_t hd_grid_ticks(const HdPlayer* player) {
    (void)player;
    return HD_DEFAULT_SPEED;
}

HdVoiceState hd_voice_state(const HdPlayer* player, uint32_t voice) {
    const HdVoice* v = &player->voices[voice];
    return (HdVoiceState) { .period = v->paula.period, .instrument = v->instrument };
}
