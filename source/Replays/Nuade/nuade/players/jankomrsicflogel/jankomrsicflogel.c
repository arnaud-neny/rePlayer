// SPDX-License-Identifier: MIT
// Janko Mrsic-Flogel player: the replayer of the module as the eagleplayer's patches leave it. Every `tick_divisor`
// ticks it steps three voices. A voice reads events from its note stream when its counter runs out: a duration, then
// what its mode adds (a note that loads one of two waveforms, or plays with the current one, a byte to skip, or DMA
// off). The voices loop a short waveform all the time; a note sets the period and starts the DMA, which does nothing
// when it already runs. A stream ends with 0xFF: the replayer starts over, the eagleplayer calls SongEnd.
//
// What the eagleplayer's patches change: the writes of the waveform address and of the period go through its own
// routines (the volume is written with them: 0x38, full volume at the default balance), and the replayer's DMA off at
// the start over calls SongEnd. The waveform address is also stored in the data register of the channel, a sample the
// Paula of the port does not take.

#include <nuade/players/jankomrsicflogel.h>

#include "jmf_internal.h"
#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>
#include <string.h>

// AUDxVOL the replayer and the eagleplayer write for the three voices, and the volume of the fourth one.
#define JMF_VOLUME 0x38u
#define JMF_IDLE_VOLUME 0x40u

static const int8_t* wave(const JmfPlayer* player, uint32_t offset) {
    return (const int8_t*)(player->module.bytes + offset);
}

// The start-over routine: counters, mode and stream of the voices, the waveforms and volumes of the four channels, DMA
// off.

static void start_over(JmfPlayer* player) {
    const JmfModule* module = &player->module;
    player->divider = 1;
    for (uint32_t i = 0; i < JMF_NOTE_VOICES; i++) {
        JmfVoice* voice = &player->voices[i];
        voice->counter = 1;
        voice->action = JmfAction_WaveA;
        voice->stream_pos = voice->stream_start;
        voice->paula.lc = wave(player, module->wave_a);
        voice->paula.len_words = module->driver->wave_a_words;
        voice->paula.volume = JMF_VOLUME;
    }
    JmfVoice* idle = &player->voices[JMF_IDLE_VOICE];
    idle->paula.lc = wave(player, module->idle_wave);
    if (module->driver->idle_words != 0) {
        idle->paula.len_words = module->driver->idle_words;
    }
    idle->paula.volume = JMF_IDLE_VOLUME;
    for (uint32_t i = 0; i < JMF_VOICES; i++) {
        nuade_paula_channel_dma_off(&player->voices[i].paula);
    }
}

bool jmf_start(JmfPlayer* player, uint32_t subsong, NuadeVoices* voices) {
    if (player == nullptr || subsong >= player->module.subsong_count) {
        return false;
    }
    for (uint32_t i = 0; i < JMF_VOICES; i++) {
        player->voices[i] = (JmfVoice) { 0 };
        if (i < JMF_NOTE_VOICES) {
            player->voices[i].stream_start = jmf_stream_start(&player->module, subsong, i);
        }
        if (voices != nullptr) {
            nuade_voice_stop(voices, i);
        }
    }
    start_over(player);
    return true;
}

// The period of the note byte the stream holds, written with the volume; then DMA on. The second waveform of a version
// plays at a multiple of the period.

static void play_note(JmfPlayer* player, JmfVoice* voice) {
    uint8_t note = 0;
    uint16_t period = 0;
    if (!jmf_byte(&player->module, voice->stream_pos++, &note) || !jmf_period(&player->module, note, &period)) {
        return;
    }
    if (voice->action == JmfAction_WaveB) {
        period = (uint16_t)(period << player->module.driver->wave_b_period_shift);
    }
    voice->paula.period = period;
    voice->paula.volume = JMF_VOLUME;
    nuade_paula_channel_dma_on(&voice->paula);
}

static void load_wave(JmfPlayer* player, JmfVoice* voice, uint32_t offset, uint16_t words) {
    voice->paula.lc = wave(player, offset);
    if (words != 0) {
        voice->paula.len_words = words;
    }
}

// The event after the duration, as the mode of the voice says.

static void run_action(JmfPlayer* player, JmfVoice* voice) {
    const JmfModule* module = &player->module;
    switch (voice->action) {
        case JmfAction_WaveA:
            load_wave(player, voice, module->wave_a, module->driver->wave_a_words);
            play_note(player, voice);
            break;
        case JmfAction_WaveB:
            load_wave(player, voice, module->wave_b, module->driver->wave_b_words);
            play_note(player, voice);
            break;
        case JmfAction_Note:
            play_note(player, voice);
            break;
        case JmfAction_Skip:
            voice->stream_pos++;
            break;
        case JmfAction_Stop:
            nuade_paula_channel_dma_off(&voice->paula);
            break;
    }
}

// The next event of a voice: mode bytes are taken on the way. False at the end of the stream (and, for crafted data,
// where the stream leaves the song data).

static bool next_event(JmfPlayer* player, JmfVoice* voice) {
    for (;;) {
        uint8_t value;
        if (!jmf_byte(&player->module, voice->stream_pos++, &value) || value == JMF_STREAM_END) {
            return false;
        }
        if (value == JMF_STREAM_MODE) {
            uint8_t mode;
            if (!jmf_byte(&player->module, voice->stream_pos++, &mode) || !jmf_mode_action(mode, &voice->action)) {
                return false;
            }
            continue;
        }
        voice->counter = value;
        run_action(player, voice);
        return true;
    }
}

// The play routine's step: the three voices in turn. A stream that ends starts the whole song over and ends the step.

static bool step(JmfPlayer* player) {
    player->divider = player->module.driver->tick_divisor;
    for (uint32_t i = 0; i < JMF_NOTE_VOICES; i++) {
        JmfVoice* voice = &player->voices[i];
        voice->counter--;
        if (voice->counter == 0 && !next_event(player, voice)) {
            start_over(player);
            return false;
        }
    }
    return true;
}

JmfTickResult jmf_tick(JmfPlayer* player, NuadeVoices* voices) {
    JmfTickResult result = { 0 };
    player->divider--;
    if (player->divider == 0) {
        result.looped = !step(player);
    }
    for (uint32_t i = 0; i < JMF_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, i, voices);
    }
    return result;
}

JmfPlayer* jmf_create(const uint8_t* data, size_t size) {
    JmfPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return nullptr;
    }
    player->chip = nuade_paula_padded_copy(data, size);
    if (player->chip == nullptr || !jmf_module_parse(&player->module, player->chip, size)) {
        jmf_destroy(player);
        return nullptr;
    }
    return player;
}

void jmf_destroy(JmfPlayer* player) {
    if (player != nullptr) {
        jmf_module_free(&player->module);
        free(player->chip);
        free(player);
    }
}

bool jmf_probe(const uint8_t* data, size_t size) {
    JmfPlayer* player = jmf_create(data, size);
    jmf_destroy(player);
    return player != nullptr;
}

uint32_t jmf_subsong_count(const JmfPlayer* player) {
    return player->module.subsong_count;
}

void jmf_text(const JmfPlayer* player, JmfText which, char* out, size_t cap) {
    if (cap == 0) {
        return;
    }
    out[0] = '\0';
    if ((unsigned)which >= JmfText_Count) {
        return;
    }
    const JmfModule* module = &player->module;
    size_t length = 0;
    uint8_t value;
    for (uint32_t at = module->texts[which]; length + 1 < cap && jmf_byte(module, at, &value) && value != 0; at++) {
        out[length++] = (char)value;
    }
    out[length] = '\0';
}

uint32_t jmf_grid_ticks(const JmfPlayer* player) {
    return player->module.driver->tick_divisor;
}

uint16_t jmf_voice_period(const JmfPlayer* player, uint32_t voice) {
    return player->voices[voice].paula.period;
}
